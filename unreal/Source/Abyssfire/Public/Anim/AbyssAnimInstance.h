// UAbyssAnimInstance: the animation player of every character (DECISIONS P5, ue58-platform.md 7.2 Option A). No Animation
// Blueprint: a native UAnimInstance whose proxy evaluates a crossfade between two sequences plus one additive layer
// (the HurtAdd "jolt"). Timing belongs to the core: the owning actor starts clips from EvPlayAnim at the play rate and
// start offset that put the authored Contact / Release beat on the core's contact time; this class only advances time
// on the game thread (frozen during the actor's hit-stop) and reports the named notifies of the art manifest
// (Contact, Release, FootL, FootR, FX_*) when the playhead crosses them.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"

#include "World/AbyssArtManifest.h"

#include "AbyssAnimInstance.generated.h"

class UAnimSequence;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAbyssAnimNotify, FName /*NotifyName*/);

/** One sampled layer, copied to the proxy every update (worker-thread safe). */
struct FAbyssAnimLayer
{
	const UAnimSequence* Seq = nullptr;
	float TimeSec = 0.f;
	bool bLoop = true;
};

struct FAbyssAnimProxy : public FAnimInstanceProxy
{
	FAbyssAnimProxy() = default;
	explicit FAbyssAnimProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
	{
	}

protected:
	/** Game thread: copy the layers from the instance. */
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	/** Worker thread: sample + blend + additive. Returns true (there is no node graph). */
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	FAbyssAnimLayer FromLayer;
	FAbyssAnimLayer ToLayer;
	float ToWeight = 1.f;
	FAbyssAnimLayer AddLayer;
	float AddWeight = 0.f;
};

UCLASS(Transient, NotBlueprintable)
class ABYSSFIRE_API UAbyssAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

	friend struct FAbyssAnimProxy;

public:
	/**
	 * Starts Seq, crossfading from the current clip over BlendSec (blend table: anim_timing.json transitionsMs).
	 * StartTimeSec offsets the playhead (an action that began earlier in sim time). Notifies are those of the manifest
	 * clip (clip time at play rate 1).
	 */
	void Play(UAnimSequence* Seq, bool bLoop, float PlayRate, float BlendSec, float StartTimeSec,
		TConstArrayView<FAbyssArtNotify> Notifies);
	/** Locomotion speed matching: changes the rate of the current clip without restarting it. */
	void SetPlayRate(float PlayRate);
	/** Additive one-shot on top of the current pose (HurtAdd), weight fading out over the clip's last 30 %. */
	void PlayAdditive(UAnimSequence* Seq, float Weight, float PlayRate = 1.f);
	/** Hit-stop (combat-feel.md 11.2): freezes this actor's animation only. */
	void SetFrozen(bool bInFrozen);
	bool IsFrozen() const { return bFrozen; }

	UAnimSequence* GetCurrentSequence() const { return ToSeq; }
	float GetCurrentTime() const { return ToTime; }
	float GetCurrentLength() const;
	float GetPlayRate() const { return Rate; }
	/** A one-shot clip reached its end (holds the last pose). */
	bool IsCurrentFinished() const;

	/** Named notify crossed by the current clip's playhead (game thread, during the anim update). */
	FOnAbyssAnimNotify OnNotify;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void FireNotifies(float PrevTimeSec, float NewTimeSec, float LengthSec, bool bWrapped);

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> FromSeq;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ToSeq;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> AdditiveSeq;

	TArray<FAbyssArtNotify> CurrentNotifies;
	float FromTime = 0.f;
	float ToTime = 0.f;
	float FromRate = 1.f;
	float Rate = 1.f;
	float AddTime = 0.f;
	float AddWeightTarget = 0.f;
	float AddRate = 1.f;
	float BlendElapsedSec = 0.f;
	float BlendSec = 0.f;
	bool bFromLoop = true;
	bool bToLoop = true;
	bool bFrozen = false;
};
