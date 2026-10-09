#include "Anim/AbyssAnimInstance.h"

#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"

namespace AbyssAnimPrivate
{
	float SequenceLength(const UAnimSequence* Seq)
	{
		return Seq != nullptr ? static_cast<float>(Seq->GetPlayLength()) : 0.f;
	}

	/** Advances a playhead; loops wrap, one-shots clamp at the end. Returns true when a loop wrapped. */
	bool AdvanceTime(float& Time, float Delta, float Length, bool bLoop)
	{
		if (Length <= UE_KINDA_SMALL_NUMBER)
		{
			Time = 0.f;
			return false;
		}
		Time += Delta;
		if (bLoop)
		{
			if (Time >= Length)
			{
				Time = FMath::Fmod(Time, Length);
				return true;
			}
			if (Time < 0.f)
			{
				Time = Length + FMath::Fmod(Time, Length);
			}
			return false;
		}
		Time = FMath::Clamp(Time, 0.f, Length);
		return false;
	}

	void SampleLayer(const FAbyssAnimLayer& Layer, FPoseContext& Context)
	{
		FAnimationPoseData PoseData(Context);
		const FAnimExtractContext Extract(static_cast<double>(Layer.TimeSec), /*bExtractRootMotion*/ false,
			FDeltaTimeRecord(), Layer.bLoop);
		Layer.Seq->GetAnimationPose(PoseData, Extract);
	}
}

// =====================================================================================================================
// Proxy (worker thread evaluation)
// =====================================================================================================================

void FAbyssAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	const UAbyssAnimInstance* Instance = CastChecked<UAbyssAnimInstance>(InAnimInstance);
	FromLayer = FAbyssAnimLayer{ Instance->FromSeq.Get(), Instance->FromTime, Instance->bFromLoop };
	ToLayer = FAbyssAnimLayer{ Instance->ToSeq.Get(), Instance->ToTime, Instance->bToLoop };
	ToWeight = Instance->BlendSec <= 0.f ? 1.f : FMath::Clamp(Instance->BlendElapsedSec / Instance->BlendSec, 0.f, 1.f);
	AddLayer = FAbyssAnimLayer{ Instance->AdditiveSeq.Get(), Instance->AddTime, false };
	AddWeight = 0.f;
	if (AddLayer.Seq != nullptr)
	{
		// Full weight, fading out over the last 30 % of the clip so the jolt never pops.
		const float Length = AbyssAnimPrivate::SequenceLength(AddLayer.Seq);
		const float FadeStart = Length * 0.7f;
		const float Fade = Length > FadeStart ? FMath::Clamp((AddLayer.TimeSec - FadeStart) / (Length - FadeStart), 0.f, 1.f) : 0.f;
		AddWeight = Instance->AddWeightTarget * (1.f - Fade * Fade * (3.f - 2.f * Fade));
	}
}

bool FAbyssAnimProxy::Evaluate(FPoseContext& Output)
{
	using namespace AbyssAnimPrivate;
	if (ToLayer.Seq == nullptr)
	{
		Output.ResetToRefPose();
		return true;
	}
	if (FromLayer.Seq == nullptr || ToWeight >= 1.f)
	{
		SampleLayer(ToLayer, Output);
	}
	else
	{
		FPoseContext PoseA(Output);
		FPoseContext PoseB(Output);
		SampleLayer(FromLayer, PoseA);
		SampleLayer(ToLayer, PoseB);
		const FAnimationPoseData DataA(PoseA);
		const FAnimationPoseData DataB(PoseB);
		FAnimationPoseData OutData(Output);
		FAnimationRuntime::BlendTwoPosesTogether(DataA, DataB, /*WeightOfPoseOne*/ 1.f - ToWeight, OutData);
	}
	if (AddLayer.Seq != nullptr && AddWeight > UE_KINDA_SMALL_NUMBER)
	{
		FPoseContext AdditivePose(Output, /*bOverrideExpectsAdditivePose*/ true);
		SampleLayer(AddLayer, AdditivePose);
		FAnimationPoseData BaseData(Output);
		const FAnimationPoseData AdditiveData(AdditivePose);
		FAnimationRuntime::AccumulateAdditivePose(BaseData, AdditiveData, AddWeight, AAT_LocalSpaceBase);
		Output.Pose.NormalizeRotations();
	}
	return true;
}

// =====================================================================================================================
// Instance (game thread)
// =====================================================================================================================

FAnimInstanceProxy* UAbyssAnimInstance::CreateAnimInstanceProxy()
{
	return new FAbyssAnimProxy(this);
}

void UAbyssAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FAbyssAnimProxy*>(InProxy);
}

void UAbyssAnimInstance::Play(UAnimSequence* Seq, bool bLoop, float PlayRate, float InBlendSec, float StartTimeSec,
	TConstArrayView<FAbyssArtNotify> Notifies)
{
	using namespace AbyssAnimPrivate;
	if (Seq == nullptr)
	{
		return;
	}
	if (ToSeq != nullptr && InBlendSec > 0.f)
	{
		FromSeq = ToSeq;
		FromTime = ToTime;
		FromRate = bFrozen ? 0.f : Rate;
		bFromLoop = bToLoop;
		BlendSec = InBlendSec;
		BlendElapsedSec = 0.f;
	}
	else
	{
		FromSeq = nullptr;
		BlendSec = 0.f;
		BlendElapsedSec = 0.f;
	}
	ToSeq = Seq;
	bToLoop = bLoop;
	Rate = FMath::Max(0.f, PlayRate);
	const float Length = SequenceLength(Seq);
	ToTime = 0.f;
	AdvanceTime(ToTime, FMath::Max(0.f, StartTimeSec), Length, bLoop);
	CurrentNotifies.Reset();
	CurrentNotifies.Append(Notifies.GetData(), Notifies.Num());
	// Notifies at or before the start offset of a late-started action are not replayed; a notify exactly at the start
	// (FootL at 0 ms) fires on the first update.
	FireNotifies(FMath::Max(0.f, ToTime - UE_KINDA_SMALL_NUMBER), ToTime, Length, false);
}

void UAbyssAnimInstance::SetPlayRate(float PlayRate)
{
	Rate = FMath::Max(0.f, PlayRate);
}

void UAbyssAnimInstance::PlayAdditive(UAnimSequence* Seq, float Weight, float PlayRate)
{
	AdditiveSeq = Seq;
	AddTime = 0.f;
	AddWeightTarget = FMath::Clamp(Weight, 0.f, 1.f);
	AddRate = FMath::Max(0.01f, PlayRate);
}

void UAbyssAnimInstance::SetFrozen(bool bInFrozen)
{
	bFrozen = bInFrozen;
}

float UAbyssAnimInstance::GetCurrentLength() const
{
	return AbyssAnimPrivate::SequenceLength(ToSeq);
}

bool UAbyssAnimInstance::IsCurrentFinished() const
{
	if (ToSeq == nullptr || bToLoop)
	{
		return false;
	}
	return ToTime >= GetCurrentLength() - UE_KINDA_SMALL_NUMBER;
}

void UAbyssAnimInstance::FireNotifies(float PrevTimeSec, float NewTimeSec, float LengthSec, bool bWrapped)
{
	if (CurrentNotifies.Num() == 0 || !OnNotify.IsBound())
	{
		return;
	}
	for (const FAbyssArtNotify& Notify : CurrentNotifies)
	{
		const float T = Notify.TimeSec;
		const bool bCrossed = bWrapped ? (T > PrevTimeSec && T <= LengthSec) || (T >= 0.f && T <= NewTimeSec)
									   : (T > PrevTimeSec && T <= NewTimeSec);
		if (bCrossed)
		{
			OnNotify.Broadcast(Notify.Name);
		}
	}
}

void UAbyssAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	using namespace AbyssAnimPrivate;
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (bFrozen || DeltaSeconds <= 0.f)
	{
		return;
	}
	if (ToSeq != nullptr)
	{
		const float Length = SequenceLength(ToSeq);
		const float Previous = ToTime;
		const bool bWrapped = AdvanceTime(ToTime, DeltaSeconds * Rate, Length, bToLoop);
		if (ToTime != Previous || bWrapped)
		{
			FireNotifies(Previous, ToTime, Length, bWrapped);
		}
	}
	if (FromSeq != nullptr)
	{
		AdvanceTime(FromTime, DeltaSeconds * FromRate, SequenceLength(FromSeq), bFromLoop);
		BlendElapsedSec += DeltaSeconds;
		if (BlendElapsedSec >= BlendSec)
		{
			FromSeq = nullptr;
			BlendSec = 0.f;
			BlendElapsedSec = 0.f;
		}
	}
	if (AdditiveSeq != nullptr)
	{
		AddTime += DeltaSeconds * AddRate;
		if (AddTime >= SequenceLength(AdditiveSeq))
		{
			AdditiveSeq = nullptr;
			AddTime = 0.f;
		}
	}
}
