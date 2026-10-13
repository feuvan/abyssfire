#include "UI/World/SAbyssWorldLayer.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"
#include "UnrealClient.h"
#include "Widgets/SNullWidget.h"

#include <string>

#include "abyss/base/I18n.h"
#include "abyss/data/DataStore.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/quests/QuestWorld.h"

#include "Abyssfire.h"
#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssSimDriver.h"
#include "Framework/AbyssText.h"
#include "Framework/AbyssTypes.h"
#include "Framework/AbyssUnits.h"
#include "Framework/AbyssWorldView.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Core/AbyssUiDraw.h"

namespace
{
	const std::string AbyssWorldLayer_EmptyQuestId;

	/** FNV-1a over what a label's name is built from (no allocation): the name is rebuilt only when this changes. */
	struct FAbyssWorldLayerLabelSignature
	{
		uint64 Hash = 14695981039346656037ull;

		void Bytes(const void* Data, size_t Size)
		{
			const uint8* Raw = static_cast<const uint8*>(Data);
			for (size_t Offset = 0; Offset < Size; ++Offset)
			{
				Hash ^= Raw[Offset];
				Hash *= 1099511628211ull;
			}
		}
		void Value(uint64 V) { Bytes(&V, sizeof(V)); }
		void Str(const std::string& Text)
		{
			Bytes(Text.data(), Text.size());
			Value(static_cast<uint64>(Text.size()));
		}
	};

	FLinearColor AbyssWorldLayer_HpColor(double Ratio, uint32 High, uint32 Mid, uint32 Low)
	{
		return FAbyssUiStyle::Rgb(Ratio > 0.5 ? High : (Ratio > 0.25 ? Mid : Low));
	}

	/** Floats that pop, stack and rise as hit numbers (combat-feel 12). */
	bool AbyssWorldLayer_IsNumber(abyss::FloatingTextKind Kind)
	{
		return Kind == abyss::FloatingTextKind::MonsterDamage || Kind == abyss::FloatingTextKind::HeroDamage
			|| Kind == abyss::FloatingTextKind::Heal || Kind == abyss::FloatingTextKind::Miss;
	}

	/** Screen rect of a float at one sample time of the placement check (Slate px). */
	struct FAbyssWorldLayerRect
	{
		double MinX = 0.0;
		double MinY = 0.0;
		double MaxX = 0.0;
		double MaxY = 0.0;
		bool bValid = false;
	};

	// Placement check of the label floats (DECISIONS C13): sample the next 1.6 s (the longest float life) every 40 ms;
	// floats fainter than 0.2 do not count; 2 px clearance; the group climbs at most 96 web px in at most 6 steps.
	constexpr double AbyssWorldLayer_PlaceStep = 0.04;
	constexpr int32 AbyssWorldLayer_PlaceSamples = 41;
	constexpr float AbyssWorldLayer_PlaceMinAlpha = 0.2f;
	constexpr double AbyssWorldLayer_PlaceGap = 2.0;
	constexpr double AbyssWorldLayer_PlaceMaxLiftWeb = 96.0;
	constexpr int32 AbyssWorldLayer_PlacePasses = 6;

	/**
	 * Ink box of a float's text: the measured width, and the glyph height of digits / capitals / CJK (0.85 em) plus the
	 * outline on both sides. The measured line height also holds the font's ascender / descender gap, which would push
	 * the rows of a pack apart for nothing. A multi-line text (the quest clue "{targetName}\n{note}") spans one ink line
	 * plus a full line height per further line.
	 */
	FVector2D AbyssWorldLayer_InkBox(const FString& Text, const FVector2D& Measured, float FontPx, int32 Outline)
	{
		int32 Lines = 1;
		for (const TCHAR Char : Text)
		{
			Lines += Char == TEXT('\n') ? 1 : 0;
		}
		const double Ink = FontPx * 0.85 + 2.0 * Outline;
		if (Measured.Y <= 0.0)
		{
			return FVector2D(Measured.X, Ink * Lines);
		}
		return FVector2D(Measured.X, FMath::Min(Measured.Y, Ink + (Lines - 1) * Measured.Y / Lines));
	}

	FLinearColor AbyssWorldLayer_ElementColor(abyss::DamageType Element)
	{
		switch (Element)
		{
		case abyss::DamageType::Fire: return FAbyssUiStyle::Rgb(0xff6600);
		case abyss::DamageType::Ice: return FAbyssUiStyle::Rgb(0x66ccff);
		case abyss::DamageType::Lightning: return FAbyssUiStyle::Rgb(0xa8e6ff);
		case abyss::DamageType::Poison: return FAbyssUiStyle::Rgb(0x66ff66);
		case abyss::DamageType::Arcane: return FAbyssUiStyle::Rgb(0xcc66ff);
		case abyss::DamageType::Physical: return FAbyssUiStyle::Rgb(0xffffff);
		}
		return FLinearColor::White;
	}
}

void SAbyssWorldLayer::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Ctx = InContext;
	Random.Initialize(0x5eed);
	SetVisibility(EVisibility::HitTestInvisible);
	ChildSlot
	[
		SNullWidget::NullWidget
	];
}

// =====================================================================================================================
// IAbyssWorldUi
// =====================================================================================================================

void SAbyssWorldLayer::AddWidget(const FAbyssWorldWidgetDesc& Desc)
{
	FLabel& Label = Labels.FindOrAdd(Desc.Id);
	Label = FLabel();
	Label.Desc = Desc;
}

void SAbyssWorldLayer::RemoveWidget(abyss::EntityId Id)
{
	Labels.Remove(Id);
}

void SAbyssWorldLayer::ClearWidgets()
{
	Labels.Reset();
	Floats.Reset();
	Popups.Reset();
	StackKeys.Reset();
}

void SAbyssWorldLayer::UpdateFrames(TConstArrayView<FAbyssWorldWidgetFrame> Frames)
{
	for (TPair<uint32, FLabel>& Pair : Labels)
	{
		Pair.Value.bHasFrame = false;
	}
	for (const FAbyssWorldWidgetFrame& Frame : Frames)
	{
		if (FLabel* Label = Labels.Find(Frame.Id))
		{
			Label->Overhead = Frame.OverheadLocation;
			Label->Feet = Frame.FeetLocation;
			Label->bVisible = Frame.bVisible;
			Label->Opacity = Frame.Opacity;
			Label->bHasFrame = true;
		}
	}
}

void SAbyssWorldLayer::AddFloatingText(const FAbyssFloatingTextRequest& Request)
{
	const double Now = Ctx->Now();
	FFloat Float;
	Float.Kind = Request.Kind;
	Float.World = Request.WorldLocation;
	Float.bCrit = Request.bCrit;
	Float.Start = Now;
	switch (Request.Kind)
	{
	case abyss::FloatingTextKind::Miss:
		Float.Text = TEXT("MISS");
		Float.Color = FAbyssUiStyle::Rgb(0x7f8c8d);
		Float.FontPx = 14.f;
		Float.Outline = 3;
		break;
	case abyss::FloatingTextKind::HeroDamage:
		Float.Text = TEXT("-") + FAbyssUiContext::Int(static_cast<int64>(FMath::RoundToDouble(Request.Value)));
		Float.Color = Request.bCrit ? FAbyssUiStyle::Rgb(0xff4444) : FAbyssUiStyle::Rgb(0xe74c3c);
		Float.FontPx = Request.bCrit ? 24.f : 20.f;
		Float.Outline = Request.bCrit ? 4 : 3;
		break;
	case abyss::FloatingTextKind::MonsterDamage:
		Float.Text = FAbyssUiContext::Int(static_cast<int64>(FMath::RoundToDouble(Request.Value)));
		Float.Color = Request.bCrit ? FAbyssUiStyle::Rgb(0xffd700) : AbyssWorldLayer_ElementColor(Request.Element);
		Float.FontPx = Request.bCrit ? 26.f : 20.f;
		Float.Outline = Request.bCrit ? 4 : 3;
		break;
	case abyss::FloatingTextKind::Heal:
		Float.Text = TEXT("+") + FAbyssUiContext::Int(static_cast<int64>(FMath::RoundToDouble(Request.Value)));
		Float.Color = FAbyssUiStyle::Rgb(0x7dff9a);
		Float.FontPx = 18.f;
		break;
	case abyss::FloatingTextKind::Exp:
		Float.Text = FString::Printf(TEXT("+%s EXP"), *FAbyssUiContext::Num(Request.Value));
		Float.Color = FAbyssUiStyle::Rgb(0xb39ddb);
		Float.FontPx = 13.f;
		Float.Outline = 2;
		Float.StartOffset = FVector2D(0.0, -40.0);
		break;
	case abyss::FloatingTextKind::Gold:
		Float.Text = FString::Printf(TEXT("+%sG"), *FAbyssUiContext::Num(Request.Value));
		Float.Color = FAbyssUiStyle::Rgb(0xffd700);
		Float.FontPx = 13.f;
		Float.Outline = 2;
		Float.StartOffset = FVector2D(15.0, -28.0);
		break;
	case abyss::FloatingTextKind::Embers:
		// quests-story-ch1.md 4 (OQ10, shown in Ch1; DECISIONS Q3 hides only the tower UI): 12 px Cinzel #ff9a4a, 2 px
		// stroke, 18 px left / 52 px above the corpse, rises 30 px and fades over 1400 ms (one Power2 = Cubic.Out tween).
		Float.Text = Ctx->LocArgsOrStr("homestead.float.embers", TEXT("+{n} Embers"),
			{ FAbyssUiContext::Arg("n", static_cast<int64>(FMath::RoundToDouble(Request.Value))) });
		Float.Color = FAbyssUiStyle::Rgb(0xff9a4a);
		Float.FontPx = 12.f;
		Float.Outline = 2;
		Float.StartOffset = FVector2D(-18.0, -52.0);
		break;
	case abyss::FloatingTextKind::Status:
	case abyss::FloatingTextKind::Custom:
		Float.Text = Request.Text.Empty() ? FString::Printf(TEXT("+%s"), *FAbyssUiContext::Num(Request.Value)) : Ctx->LocStr(Request.Text);
		Float.Color = Request.Text.Empty() ? FAbyssUiStyle::Rgb(0x6fb6ff) : FAbyssUiStyle::Rgb(0xf0dcae);
		Float.FontPx = 15.f;
		Float.Outline = 2;
		Float.StartOffset = FVector2D(0.0, -36.0);
		break;
	}
	if (Float.Text.IsEmpty())
	{
		return;
	}
	if (AbyssWorldLayer_IsNumber(Request.Kind))
	{
		// Stacking (combat-feel 12): a number on the same spot within 320 ms climbs one row (max 4).
		const int64 KeyX = static_cast<int64>(FMath::RoundToDouble(Float.World.X / 40.0));
		const int64 KeyY = static_cast<int64>(FMath::RoundToDouble(Float.World.Y / 40.0));
		const int64 Key = (KeyX << 32) ^ (KeyY & 0xffffffffll);
		int32 Index = 0;
		if (TPair<double, int32>* Previous = StackKeys.Find(Key))
		{
			Index = Now - Previous->Key < 0.32 ? FMath::Min(Previous->Value + 1, 4) : 0;
		}
		StackKeys.Add(Key, TPair<double, int32>(Now, Index));
		Float.StartOffset = FVector2D(Random.RandRange(-6, 6), -30.0 - Index * 11.0);
		Float.Drift = ((Index % 2 == 0) ? 1.0 : -1.0) * (Request.bCrit ? 14.0 : 10.0) + Random.RandRange(-4, 4);
		switch (Request.Slot)
		{
		case abyss::HitNumberSlot::DoubleStrike: Float.StartOffset.Y -= 20.0; break;
		case abyss::HitNumberSlot::DoubleShot: Float.StartOffset += FVector2D(15.0, -15.0); break;
		case abyss::HitNumberSlot::Primary: break;
		}
	}
	Float.BaseOffset = Float.StartOffset;
	Float.Box = AbyssWorldLayer_InkBox(Float.Text,
		FAbyssPainter::Measure(Float.Text, Ctx->Style().Font(EAbyssFontFace::Title, Float.FontPx, true, Float.Outline)), Float.FontPx,
		Float.Outline);
	const bool bLabelFloat = !AbyssWorldLayer_IsNumber(Request.Kind);
	const FVector World = Float.World;
	Floats.Add(MoveTemp(Float));
	if (Floats.Num() > 96)
	{
		Floats.RemoveAt(0, Floats.Num() - 96);
	}
	if (StackKeys.Num() > 256)
	{
		StackKeys.Reset();
	}
	if (bLabelFloat)
	{
		// Port addition (DECISIONS C13): kill rewards, status and pickup lines are placed against every float on screen.
		// Hit numbers keep their spec position and paint above them.
		PlaceFloatGroup(World, Now);
	}
	UE_LOG(LogAbyss, VeryVerbose, TEXT("Float kind=%d world=%s offset=%s base=%s (web px, x%.2f on screen)"), static_cast<int32>(Request.Kind),
		*Request.WorldLocation.ToString(), *Floats.Last().StartOffset.ToString(), *Floats.Last().BaseOffset.ToString(), WebPx);
}

void SAbyssWorldLayer::PlaceFloatGroup(const FVector& World, double Now)
{
	// The group of the float just added (the last one): label floats spawned this frame on this anchor (Slate time is
	// constant within a frame), split in arrival order into groups that hold each kind once, i.e. one kill's +EXP / +G /
	// +Embers, which keep their web layout among themselves. Two kills on one spot in one frame (an AoE on coincident
	// monsters) make two groups, and the second is placed above the first like any other float. The group is re-placed
	// from its base offsets each time a member joins, so it always moves as one; earlier groups keep their place.
	TArray<int32, TInlineAllocator<4>> Members;
	for (int32 Index = 0; Index < Floats.Num(); ++Index)
	{
		const FFloat& Candidate = Floats[Index];
		if (AbyssWorldLayer_IsNumber(Candidate.Kind) || FMath::Abs(Candidate.Start - Now) >= 1e-4
			|| FVector::DistSquared(Candidate.World, World) >= 1.0)
		{
			continue;
		}
		const abyss::FloatingTextKind Kind = Candidate.Kind;
		if (Members.ContainsByPredicate([this, Kind](int32 Member) { return Floats[Member].Kind == Kind; }))
		{
			Members.Reset();
		}
		Members.Add(Index);
	}
	for (const int32 Member : Members)
	{
		Floats[Member].StartOffset = Floats[Member].BaseOffset;
	}
	// Projected with the last paint's camera (one frame old at worst; every float uses the same one).
	FVector2D GroupAnchor;
	if (Members.Num() == 0 || !Project(World, GroupAnchor, PaintLocalSize))
	{
		return;
	}
	constexpr int32 Samples = AbyssWorldLayer_PlaceSamples;
	const auto SampleRect = [this](const FFloat& Float, const FVector2D& Anchor, double T, FAbyssWorldLayerRect& Out)
	{
		FVector2D Offset;
		float Alpha = 1.f;
		float Scale = 1.f;
		Out.bValid = FloatMotion(Float, T, Offset, Alpha, Scale) && Alpha > AbyssWorldLayer_PlaceMinAlpha;
		if (Out.bValid)
		{
			const FVector2D Center = Anchor + Offset * WebPx;
			const FVector2D Half = Float.Box * (0.5 * Scale);
			Out.MinX = Center.X - Half.X;
			Out.MaxX = Center.X + Half.X;
			Out.MinY = Center.Y - Half.Y;
			Out.MaxY = Center.Y + Half.Y;
		}
	};
	// Where every other float will be over the next 1.6 s, and the members at their base offsets.
	TArray<FAbyssWorldLayerRect> Occupied;
	Occupied.Reserve((Floats.Num() - Members.Num()) * Samples);
	for (int32 Index = 0; Index < Floats.Num(); ++Index)
	{
		const FFloat& Other = Floats[Index];
		FVector2D OtherAnchor;
		if (Members.Contains(Index) || !Project(Other.World, OtherAnchor, PaintLocalSize))
		{
			continue;
		}
		const double Age = Now - Other.Start;
		for (int32 K = 0; K < Samples; ++K)
		{
			SampleRect(Other, OtherAnchor, Age + K * AbyssWorldLayer_PlaceStep, Occupied.AddDefaulted_GetRef());
		}
	}
	if (Occupied.Num() == 0)
	{
		return;
	}
	TArray<FAbyssWorldLayerRect> Mine;
	Mine.SetNum(Members.Num() * Samples);
	for (int32 M = 0; M < Members.Num(); ++M)
	{
		for (int32 K = 0; K < Samples; ++K)
		{
			SampleRect(Floats[Members[M]], GroupAnchor, K * AbyssWorldLayer_PlaceStep, Mine[M * Samples + K]);
		}
	}
	// Lift the group (Slate px) until no member overlaps another float at any sample. Every label float rises on a
	// decelerating curve and starts at its fastest, so a group that starts clear above an older float stays clear; the
	// samples also catch a hit number that is still rising through the spot (the killing blow's -30 -> -58 px).
	const int32 OtherCount = Occupied.Num() / Samples;
	const double MaxLift = AbyssWorldLayer_PlaceMaxLiftWeb * WebPx;
	double Lift = 0.0;
	for (int32 Pass = 0; Pass < AbyssWorldLayer_PlacePasses; ++Pass)
	{
		double Need = 0.0;
		for (int32 Slot = 0; Slot < Mine.Num(); ++Slot)
		{
			const FAbyssWorldLayerRect& Own = Mine[Slot];
			if (!Own.bValid)
			{
				continue;
			}
			const int32 K = Slot % Samples;
			const double Top = Own.MinY - Lift;
			const double Bottom = Own.MaxY - Lift;
			for (int32 O = 0; O < OtherCount; ++O)
			{
				const FAbyssWorldLayerRect& Theirs = Occupied[O * Samples + K];
				if (Theirs.bValid && Own.MinX < Theirs.MaxX + AbyssWorldLayer_PlaceGap && Theirs.MinX < Own.MaxX + AbyssWorldLayer_PlaceGap
					&& Top < Theirs.MaxY + AbyssWorldLayer_PlaceGap && Theirs.MinY < Bottom + AbyssWorldLayer_PlaceGap)
				{
					Need = FMath::Max(Need, Bottom + AbyssWorldLayer_PlaceGap - Theirs.MinY + 0.01);
				}
			}
		}
		if (Need <= 0.0)
		{
			break;
		}
		Lift = FMath::Min(Lift + Need, MaxLift);
		if (Lift >= MaxLift)
		{
			break;
		}
	}
	if (Lift > 0.0)
	{
		for (const int32 Member : Members)
		{
			Floats[Member].StartOffset.Y = Floats[Member].BaseOffset.Y - Lift / WebPx;
		}
	}
}

// =====================================================================================================================
// Sync (labels read the snapshot; nothing is kept beyond copies)
// =====================================================================================================================

FString SAbyssWorldLayer::MonsterLabel(const abyss::MonsterView& Monster) const
{
	FString Name = Monster.nameKey.empty() ? Ctx->MonsterName(Monster.defId) : Ctx->NameOr(Monster.nameKey, Monster.defId);
	if (!Monster.storyNamed && !Monster.affixes.empty())
	{
		// localized affix names in front of the name (monsters-ai 15)
		FString Prefix;
		for (const abyss::EliteAffixType Affix : Monster.affixes)
		{
			const std::string Id(abyss::EnumName(Affix));
			Prefix += Ctx->NameOr("sys.eliteAffix.name." + Id, Id) + TEXT(" ");
		}
		Name = Prefix + Name;
	}
	return Name;
}

void SAbyssWorldLayer::Sync(const abyss::Snapshot& Snap)
{
	TargetId = Snap.hero.target;

	// Entity lookups through the driver's per-frame index (O(1)); a linear scan only when the index is not for this
	// snapshot. Label names are rebuilt only when what they show changes (signature) or the locale changes; HP, presence
	// and markers are copied every frame.
	const UAbyssGameInstance* GameInstance = Ctx->GetGameInstance();
	const UWorld* GameWorld = GameInstance ? GameInstance->GetWorld() : nullptr;
	const UAbyssSimDriver* Driver = GameWorld ? GameWorld->GetSubsystem<UAbyssSimDriver>() : nullptr;
	const FAbyssSnapshotIndex* Index = Driver != nullptr && Driver->GetSnapshotIndex().GetSnapshot() == &Snap ? &Driver->GetSnapshotIndex() : nullptr;
	const auto FindMonster = [&Snap, Index](abyss::EntityId Id) -> const abyss::MonsterView*
	{
		if (Index != nullptr)
		{
			return Index->FindMonster(Id);
		}
		for (const abyss::MonsterView& Monster : Snap.monsters)
		{
			if (Monster.id == Id)
			{
				return &Monster;
			}
		}
		return nullptr;
	};
	const auto FindNpc = [&Snap, Index](abyss::EntityId Id) -> const abyss::NpcView*
	{
		if (Index != nullptr)
		{
			return Index->FindNpc(Id);
		}
		for (const abyss::NpcView& Npc : Snap.npcs)
		{
			if (Npc.id == Id)
			{
				return &Npc;
			}
		}
		return nullptr;
	};
	const auto FindGroundItem = [&Snap, Index](abyss::EntityId Id) -> const abyss::GroundItemView*
	{
		if (Index != nullptr)
		{
			return Index->FindGroundItem(Id);
		}
		for (const abyss::GroundItemView& Item : Snap.groundItems)
		{
			if (Item.id == Id)
			{
				return &Item;
			}
		}
		return nullptr;
	};
	const auto FindMarker = [&Snap, Index](abyss::EntityId Id) -> const abyss::WorldMarkerView*
	{
		if (Index != nullptr)
		{
			return Index->FindMarker(Id);
		}
		for (const abyss::WorldMarkerView& Marker : Snap.markers)
		{
			if (Marker.id == Id)
			{
				return &Marker;
			}
		}
		return nullptr;
	};

	// A locale change invalidates every cached name.
	const abyss::I18n* Strings = Ctx->GetStrings();
	const int32 Locale = Strings != nullptr ? static_cast<int32>(Strings->Current()) : -1;
	if (Locale != NameLocale)
	{
		NameLocale = Locale;
		for (TPair<uint32, FLabel>& Pair : Labels)
		{
			Pair.Value.bNameValid = false;
		}
	}
	// true when the label's name must be rebuilt for this signature
	const auto NeedsName = [](FLabel& Label, const FAbyssWorldLayerLabelSignature& Signature)
	{
		if (Label.bNameValid && Label.NameSignature == Signature.Hash)
		{
			return false;
		}
		Label.bNameValid = true;
		Label.NameSignature = Signature.Hash;
		return true;
	};

	for (TPair<uint32, FLabel>& Pair : Labels)
	{
		FLabel& Label = Pair.Value;
		const abyss::EntityId Id = Pair.Key;
		Label.bPresent = false;
		Label.bHpBar = false;
		Label.Marker = abyss::NpcMarker::None;
		switch (Label.Desc.Kind)
		{
		case EAbyssWorldWidgetKind::Monster:
			if (const abyss::MonsterView* Monster = FindMonster(Id))
			{
				FAbyssWorldLayerLabelSignature Signature;
				Signature.Str(Monster->defId);
				Signature.Str(Monster->nameKey);
				Signature.Value(Monster->storyNamed ? 1u : 0u);
				Signature.Value(static_cast<uint64>(Monster->affixes.size()));
				for (const abyss::EliteAffixType Affix : Monster->affixes)
				{
					Signature.Value(static_cast<uint64>(Affix));
				}
				if (NeedsName(Label, Signature))
				{
					Label.Name = MonsterLabel(*Monster);
				}
				Label.bPresent = Monster->alive;
				Label.Hp = Monster->hp;
				Label.MaxHp = FMath::Max(1.0, Monster->maxHp);
				Label.bHpBar = Monster->hp < Monster->maxHp;   // hidden at full HP (monsters-ai 5)
				// A story-renamed boss uses the colour of EvMonsterRenamed (carried in the desc); 0xffcf6a only when the
				// rename happened before this widget existed (core request: MonsterView::nameColor).
				const FLinearColor StoryColor = Label.Desc.bHasNameColor ? FAbyssUiStyle::Rgb(Label.Desc.NameColorRgb) : FAbyssUiStyle::Rgb(0xffcf6a);
				Label.NameColor = Monster->storyNamed ? StoryColor
					: (!Monster->affixes.empty() ? FAbyssUiStyle::Rgb(0xff6600) : (Monster->elite || Monster->miniBoss ? FAbyssUiStyle::Rgb(0xe74c3c) : FAbyssUiStyle::Rgb(0xcccccc)));
				Label.bAlwaysShowName = Monster->storyNamed || Monster->elite || Monster->miniBoss || !Monster->affixes.empty();
				Label.NameFontPx = 12;
				Label.bTitleFont = true;
			}
			break;
		case EAbyssWorldWidgetKind::Npc:
			if (const abyss::NpcView* Npc = FindNpc(Id))
			{
				FAbyssWorldLayerLabelSignature Signature;
				Signature.Str(Npc->npcId);
				if (NeedsName(Label, Signature))
				{
					Label.Name = Ctx->NpcName(Npc->npcId);
				}
				Label.bPresent = true;
				Label.NameColor = Ctx->Style().Colors().Parchment;
				Label.Marker = Npc->marker;
				Label.bAlwaysShowName = true;
				Label.NameFontPx = 13;
			}
			break;
		case EAbyssWorldWidgetKind::Pet:
			if (Snap.pet.present && Snap.pet.id == Id)
			{
				int32 Level = 1;
				if (Snap.pets != nullptr)
				{
					if (const abyss::PetInstance* Instance = Snap.pets->Find(Snap.pet.petId))
					{
						Level = Instance->level;
					}
				}
				FAbyssWorldLayerLabelSignature Signature;
				Signature.Str(Snap.pet.petId);
				Signature.Value(static_cast<uint64>(Snap.pet.stage));
				Signature.Value(static_cast<uint64>(Level));
				Signature.Value(Snap.pet.exhausted ? 1u : 0u);
				if (NeedsName(Label, Signature))
				{
					Label.Name = FString::Printf(TEXT("%s Lv.%d"), *Ctx->PetName(Snap.pet.petId, Snap.pet.stage), Level);
					if (Snap.pet.exhausted)
					{
						Label.Name += FString::Printf(TEXT(" [%s]"), *Ctx->LocOrStr("zone.pet.exhaustedTag", TEXT("Exhausted")));
					}
				}
				Label.bPresent = true;
				Label.NameColor = FAbyssUiStyle::Rgb(0xaaddff);
				Label.NameFontPx = 9;
				Label.Hp = Snap.pet.hp;
				Label.MaxHp = FMath::Max(1.0, Snap.pet.maxHp);
				Label.bHpBar = Snap.pet.hp / Label.MaxHp < 0.999;
				Label.bSmallBar = true;
				Label.bAlwaysShowName = true;
			}
			break;
		case EAbyssWorldWidgetKind::Escort:
		case EAbyssWorldWidgetKind::DefendTarget:
		{
			const bool bEscort = Label.Desc.Kind == EAbyssWorldWidgetKind::Escort;
			if (const abyss::WorldMarkerView* Marker = FindMarker(Id))
			{
				Label.bPresent = true;
				Label.Hp = Marker->hp;
				Label.MaxHp = FMath::Max(1.0, Marker->maxHp);
				Label.bHpBar = Marker->maxHp > 0.0;
			}
			const std::string& QuestId = bEscort ? (Snap.escort ? Snap.escort->questId : AbyssWorldLayer_EmptyQuestId) : (Snap.defend ? Snap.defend->questId : AbyssWorldLayer_EmptyQuestId);
			if (!QuestId.empty())
			{
				FAbyssWorldLayerLabelSignature Signature;
				Signature.Str(QuestId);
				if (NeedsName(Label, Signature))
				{
					const abyss::DataStore* Data = Ctx->GetData();
					const abyss::QuestDef* Quest = Data ? Data->FindQuest(QuestId) : nullptr;
					const std::string Raw = Quest ? (bEscort ? Quest->escortNpc.name : Quest->defendTarget.name) : std::string();
					Label.Name = Ctx->NameOr((bEscort ? "data.escortNpc." : "data.defendTarget.") + QuestId, Raw);
				}
				if (!Label.bPresent)
				{
					Label.bPresent = true;
					Label.Hp = bEscort ? (Snap.escort ? Snap.escort->hp : 0.0) : (Snap.defend ? Snap.defend->hp : 0.0);
					Label.MaxHp = FMath::Max(1.0, bEscort ? (Snap.escort ? Snap.escort->maxHp : 1.0) : (Snap.defend ? Snap.defend->maxHp : 1.0));
					Label.bHpBar = true;
				}
			}
			Label.NameColor = FAbyssUiStyle::Rgb(0xffe0a0);
			Label.bAlwaysShowName = true;
			Label.NameFontPx = 12;
			break;
		}
		case EAbyssWorldWidgetKind::GroundItem:
			if (const abyss::GroundItemView* Item = FindGroundItem(Id))
			{
				// The item's own name in its quality colour, as the web nameplate (ZoneScene.dropLootAtPosition: item.name):
				// affix name for magic / rare, the legendary / set name (ItemDisplayName, loot 18 / FIX Q16).
				FAbyssWorldLayerLabelSignature Signature;
				Signature.Str(Item->uid);
				Signature.Str(Item->baseId);
				Signature.Value(static_cast<uint64>(Item->quality));
				Signature.Value(static_cast<uint64>(Item->quantity));
				if (NeedsName(Label, Signature))
				{
					Label.Name = Item->item != nullptr ? Ctx->ItemName(*Item->item) : Ctx->ItemBaseName(Item->baseId);
					if (Item->quantity > 1)
					{
						Label.Name += FString::Printf(TEXT(" x%d"), Item->quantity);
					}
				}
				Label.bPresent = true;
				Label.NameColor = Ctx->Style().QualityColor(Item->quality);
				Label.bAlwaysShowName = true;
				Label.NameFontPx = 11;
			}
			break;
		case EAbyssWorldWidgetKind::Lore:
		case EAbyssWorldWidgetKind::Prop:
		{
			const FString& DefId = Label.Desc.DefId;
			const bool bSoulEcho = DefId == TEXT("soul_echo");
			const abyss::WorldMarkerView* EchoMarker = nullptr;
			if (bSoulEcho)
			{
				const abyss::WorldMarkerView* Marker = FindMarker(Id);
				EchoMarker = Marker != nullptr && Marker->kind == abyss::MarkerKind::SoulEcho ? Marker : nullptr;
			}
			FAbyssWorldLayerLabelSignature Signature;
			if (EchoMarker != nullptr)
			{
				Signature.Str(EchoMarker->key);
			}
			if (NeedsName(Label, Signature))
			{
				FString Text;
				if (DefId.StartsWith(TEXT("lore:")))
				{
					const std::string LoreId = AbyssText::ToStd(DefId.Mid(5));
					const abyss::DataStore* Data = Ctx->GetData();
					const abyss::LoreEntryDef* Entry = Data ? Data->Lore().Find(LoreId) : nullptr;
					Text = Ctx->NameOr("data.lore." + LoreId + ".name", Entry ? Entry->name : LoreId);
				}
				else if (DefId.StartsWith(TEXT("hidden_reward:")))
				{
					Text = DefId.Contains(TEXT("gold")) ? Ctx->LocOrStr("zone.hiddenArea.rewardGoldPile", TEXT("Gold pile"))
						: Ctx->LocOrStr("zone.hiddenArea.rewardChest", TEXT("Chest"));
				}
				else if (DefId == TEXT("treasure_cache"))
				{
					Text = Ctx->LocOrStr("zone.event.treasureChest.label", TEXT("Treasure chest"));
				}
				else if (DefId == TEXT("wandering_merchant"))
				{
					Text = Ctx->LocOrStr("zone.event.merchant.label", TEXT("Wandering merchant"));
				}
				else if (DefId == TEXT("environmental_puzzle"))
				{
					Text = Ctx->LocOrStr("zone.event.puzzle.label", TEXT("Puzzle device"));
				}
				else if (bSoulEcho)
				{
					const FString Gold = EchoMarker != nullptr ? AbyssText::ToFString(EchoMarker->key) : FString(TEXT("?"));
					Text = Ctx->LocArgsOrStr("zone.soulEcho.label", TEXT("Soul echo - {gold} gold"), { FAbyssUiContext::Arg("gold", Gold) });
				}
				Label.Name = Text.IsEmpty() ? Text : FString(TEXT("\x25C6 ")) + Text;
			}
			Label.bPresent = !Label.Name.IsEmpty();
			Label.NameColor = FAbyssUiStyle::Rgb(0xffe7a0);
			Label.bAlwaysShowName = true;
			Label.NameFontPx = 11;
			break;
		}
		}
	}

	// the hero's overhead point (quest progress popups)
	bHeroKnown = false;
	if (GameInstance != nullptr)
	{
		const IAbyssWorldView* View = Driver ? Driver->GetWorldView() : nullptr;
		const abyss::Vec2 Tile = AbyssUnits::LerpTile(Snap.hero.prevPos, Snap.hero.pos, Snap.interpolationAlpha);
		const FVector Ground = AbyssUnits::TileToWorld(Tile);
		const double Z = View ? View->GetGroundHeight(Ground.X, Ground.Y) : 0.0;
		HeroOverhead = FVector(Ground.X, Ground.Y, Z + 210.0);
		HeroGround = FVector(Ground.X, Ground.Y, Z);
		bHeroKnown = true;
	}
}

void SAbyssWorldLayer::HandleQuestUpdate(const abyss::EvQuestUpdate& Update, const abyss::Snapshot& Snap)
{
	if (Update.kind != abyss::EvQuestUpdate::Kind::Progress)
	{
		return;
	}
	const abyss::DataStore* Data = Ctx->GetData();
	const abyss::QuestDef* Quest = Data ? Data->FindQuest(Update.questId) : nullptr;
	if (Quest == nullptr || Update.objectiveIndex < 0 || static_cast<size_t>(Update.objectiveIndex) >= Quest->objectives.size())
	{
		return;
	}
	const abyss::QuestObjectiveDef& Objective = Quest->objectives[static_cast<size_t>(Update.objectiveIndex)];
	const bool bDone = Update.current >= Update.required;
	const bool bKill = Objective.type == abyss::ObjectiveType::Kill;
	// quests 5.8: kill objectives with required > 3 only pop when done or at every ceil(required / 4) step
	if (bKill && Update.required > 3 && !bDone)
	{
		const int32 Step = FMath::Max(1, FMath::CeilToInt(Update.required / 4.0));
		if (Update.current % Step != 0)
		{
			return;
		}
	}
	const FString Target = Ctx->ObjectiveTargetLabel(*Quest, Update.objectiveIndex);
	FPopup Popup;
	Popup.bDone = bDone;
	Popup.Text = bDone ? FString::Printf(TEXT("\x2713 %s"), *Target)
		: FString::Printf(TEXT("%s%s  %d/%d"), bKill ? TEXT("") : TEXT("+1 "), *Target, Update.current, Update.required);
	Popup.Start = Ctx->Now();
	int32 Stack = 0;
	for (const FPopup& Other : Popups)
	{
		if (Popup.Start - Other.Start < 0.9)
		{
			Stack = FMath::Max(Stack, Other.Stack + 1);
		}
	}
	Popup.Stack = Stack;
	Popups.Add(MoveTemp(Popup));
}

// =====================================================================================================================
// Paint
// =====================================================================================================================

bool SAbyssWorldLayer::Project(const FVector& World, FVector2D& OutLocal, const FVector2D& LocalSize) const
{
	if (!bProjectionValid || ViewportSize.X <= 0.0 || ViewportSize.Y <= 0.0)
	{
		return false;
	}
	FVector2D Screen;
	if (!FSceneView::ProjectWorldToScreen(World, ViewRect, ViewProjection, Screen))
	{
		return false;
	}
	OutLocal = FVector2D(Screen.X * LocalSize.X / ViewportSize.X, Screen.Y * LocalSize.Y / ViewportSize.Y);
	return true;
}

int32 SAbyssWorldLayer::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// One projection per paint, with the camera of this frame (Slate paints after the world and camera updates).
	bProjectionValid = false;
	const UAbyssGameInstance* GameInstance = Ctx->GetGameInstance();
	const APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	if (LocalPlayer != nullptr && LocalPlayer->ViewportClient != nullptr && LocalPlayer->ViewportClient->Viewport != nullptr)
	{
		FSceneViewProjectionData ProjectionData;
		if (LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
		{
			ViewProjection = ProjectionData.ComputeViewProjectionMatrix();
			ViewRect = ProjectionData.GetConstrainedViewRect();
			const FIntPoint Size = LocalPlayer->ViewportClient->Viewport->GetSizeXY();
			ViewportSize = FVector2D(Size.X, Size.Y);
			bProjectionValid = true;
		}
	}
	if (!bProjectionValid)
	{
		return LayerId;
	}

	const double Now = Ctx->Now();
	const FVector2D LocalSize(AllottedGeometry.GetLocalSize());
	PaintLocalSize = LocalSize;
	// The web draws zone texts as world objects under ZONE_CAMERA_ZOOM 1.8, so its float offsets are web world px. One
	// web tile diagonal (64 px) = the projected tile diagonal at the hero ((+1, -1) runs screen-horizontal at the 45 deg
	// yaw, W1): ~1.77 at the default framing, following the wheel zoom.
	if (bHeroKnown)
	{
		FVector2D TileFrom;
		FVector2D TileTo;
		if (Project(HeroGround, TileFrom, LocalSize) && Project(HeroGround + FVector(AbyssUnits::TileUU, -AbyssUnits::TileUU, 0.0), TileTo, LocalSize))
		{
			WebPx = FMath::Clamp(FVector2D::Distance(TileFrom, TileTo) / 64.0, 1.0, 3.0);
		}
	}
	if (!LocalSize.Equals(LoggedLocalSize, 0.5) || !ViewportSize.Equals(LoggedViewportSize, 0.5) || FMath::Abs(WebPx - LoggedWebPx) > 0.25)
	{
		LoggedLocalSize = LocalSize;
		LoggedViewportSize = ViewportSize;
		LoggedWebPx = WebPx;
		UE_LOG(LogAbyss, Log, TEXT("WorldLayer: LocalSize=%s Viewport=%s WebPx=%.2f (hero %s)"), *LocalSize.ToString(), *ViewportSize.ToString(), WebPx,
			bHeroKnown ? TEXT("known") : TEXT("unknown"));
	}
	FAbyssPainter P(AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, Ctx->Style());
	for (const TPair<uint32, FLabel>& Pair : Labels)
	{
		PaintLabel(P, Pair.Value, LocalSize, Now);
	}
	P.NextLayer();
	Floats.RemoveAll([Now](const FFloat& Float) { return Now - Float.Start > 1.6; });
	// Kill rewards and status lines first, hit numbers above them (each text is its own layer, so paint order is draw
	// order): a number spawned after a reward on that spot (the next target's hit, a DoT tick) rises through it readable.
	for (const FFloat& Float : Floats)
	{
		if (!AbyssWorldLayer_IsNumber(Float.Kind))
		{
			PaintFloat(P, Float, LocalSize, Now);
		}
	}
	for (const FFloat& Float : Floats)
	{
		if (AbyssWorldLayer_IsNumber(Float.Kind))
		{
			PaintFloat(P, Float, LocalSize, Now);
		}
	}
	// quest progress popups above the hero: pop 160 ms, hold 800 ms (1300 when done), rise 26 px and fade 700 ms (rows and
	// rise in web px)
	Popups.RemoveAll([Now](const FPopup& Popup) { return Now - Popup.Start > (Popup.bDone ? 2.16 : 1.66); });
	FVector2D HeroScreen;
	if (bHeroKnown && Popups.Num() > 0 && Project(HeroOverhead, HeroScreen, LocalSize))
	{
		for (const FPopup& Popup : Popups)
		{
			const double T = Now - Popup.Start;
			const double Hold = Popup.bDone ? 1.3 : 0.8;
			const float Pop = AbyssEase::BackOut(AbyssEase::Clamp01(static_cast<float>(T / 0.16)));
			const float Out = T > 0.16 + Hold ? AbyssEase::Clamp01(static_cast<float>((T - 0.16 - Hold) / 0.7)) : 0.f;
			const FVector2D Pos = HeroScreen + FVector2D(0.0, -(16.0 * Popup.Stack + 26.0 * Out) * WebPx);
			const FLinearColor Color = Popup.bDone ? FAbyssUiStyle::Rgb(0x7ee07e) : FAbyssUiStyle::Rgb(0xffe7a0);
			P.TextCentered(Pos, Popup.Text, Ctx->Style().Body(16.f, true, 3), FAbyssUiStyle::WithAlpha(Color, 1.f - Out), FMath::Max(0.01f, Pop));
		}
	}
	return P.Layer;
}

void SAbyssWorldLayer::PaintLabel(FAbyssPainter& P, const FLabel& Label, const FVector2D& LocalSize, double Now) const
{
	if (!Label.bHasFrame || !Label.bVisible || !Label.bPresent || Label.Opacity <= 0.01f)
	{
		return;
	}
	FVector2D Anchor;
	if (!Project(Label.Overhead, Anchor, LocalSize))
	{
		return;
	}
	if (Anchor.X < -100.0 || Anchor.Y < -100.0 || Anchor.X > LocalSize.X + 100.0 || Anchor.Y > LocalSize.Y + 100.0)
	{
		return;
	}
	const float Alpha = Label.Opacity;
	double Y = Anchor.Y;

	// HP bar (monsters 40x4, pets 24x3; escort / defend 60x5)
	if (Label.bHpBar)
	{
		const double Ratio = FMath::Clamp(Label.Hp / FMath::Max(1.0, Label.MaxHp), 0.0, 1.0);
		const bool bQuestBar = Label.Desc.Kind == EAbyssWorldWidgetKind::Escort || Label.Desc.Kind == EAbyssWorldWidgetKind::DefendTarget;
		const FVector2D BarSize = Label.bSmallBar ? FVector2D(24.0, 3.0) : (bQuestBar ? FVector2D(60.0, 5.0) : FVector2D(40.0, 4.0));
		const FVector2D BarPos(Anchor.X - BarSize.X * 0.5, Y - BarSize.Y);
		const FLinearColor Fill = Label.bSmallBar ? AbyssWorldLayer_HpColor(Ratio, 0x6fd35a, 0xf39c12, 0xe74c3c)
			: AbyssWorldLayer_HpColor(Ratio, 0x2ecc71, 0xf39c12, 0xe74c3c);
		P.Box(BarPos - FVector2D(1.0, 1.0), BarSize + FVector2D(2.0, 2.0), FLinearColor(0.f, 0.f, 0.f, 0.75f * Alpha));
		P.Box(BarPos, FVector2D(BarSize.X * Ratio, BarSize.Y), FAbyssUiStyle::WithAlpha(Fill, Alpha));
		Y -= BarSize.Y + 3.0;
	}
	// name
	const bool bEmphasis = Label.Desc.Id == HoveredId || Label.Desc.Id == TargetId;
	const bool bShowName = Label.bAlwaysShowName || Label.bHpBar || bEmphasis;
	if (bShowName && !Label.Name.IsEmpty())
	{
		const FSlateFontInfo Font = Label.bTitleFont ? Ctx->Style().Title(static_cast<float>(Label.NameFontPx), true, 2)
			: Ctx->Style().Body(static_cast<float>(Label.NameFontPx), true, 2);
		const FVector2D TextSize = FAbyssPainter::Measure(Label.Name, Font);
		FLinearColor Color = Label.NameColor;
		if (bEmphasis && Label.Desc.Kind == EAbyssWorldWidgetKind::Monster)
		{
			Color = FAbyssUiStyle::Lighten(Color, 0.2f);
		}
		P.TextCentered(FVector2D(Anchor.X, Y - TextSize.Y * 0.5), Label.Name, Font, FAbyssUiStyle::WithAlpha(Color, Alpha));
		Y -= TextSize.Y + 2.0;
	}
	// NPC quest marker (quests 5.6): "!" available, "?" turn-in (gold) / in progress (grey), bobbing 4 px over 600 ms
	if (Label.Desc.Kind == EAbyssWorldWidgetKind::Npc && Label.Marker != abyss::NpcMarker::None && Label.Marker != abyss::NpcMarker::AvailableLocked)
	{
		const bool bExclaim = Label.Marker == abyss::NpcMarker::Available;
		const FLinearColor Color = Label.Marker == abyss::NpcMarker::InProgress ? FAbyssUiStyle::Rgb(0x888888) : FAbyssUiStyle::Rgb(0xf1c40f);
		const double Bob = FMath::Sin(Now * UE_PI / 0.6) * 4.0;
		const FVector2D Glyph(Anchor.X, Y - 14.0 + Bob);
		if (Label.Marker != abyss::NpcMarker::InProgress)
		{
			P.Glow(Glyph, 14.f, FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xffc040), 0.45f * Alpha), 5);
		}
		P.TextCentered(Glyph, bExclaim ? TEXT("!") : TEXT("?"), Ctx->Style().Title(24.f, true, 4), FAbyssUiStyle::WithAlpha(Color, Alpha));
	}
}

void SAbyssWorldLayer::PaintFloat(FAbyssPainter& P, const FFloat& Float, const FVector2D& LocalSize, double Now) const
{
	FVector2D Offset;
	float Alpha = 1.f;
	float Scale = 1.f;
	if (!FloatMotion(Float, Now - Float.Start, Offset, Alpha, Scale) || Alpha <= 0.01f)
	{
		return;
	}
	FVector2D Anchor;
	if (!Project(Float.World, Anchor, LocalSize))
	{
		return;
	}
	// offsets, rises, drift and sinks are web world px (combat-feel 12), scaled to the camera by WebPx
	const FSlateFontInfo Font = Ctx->Style().Font(EAbyssFontFace::Title, Float.FontPx, true, Float.Outline);
	P.TextCentered(Anchor + Offset * WebPx, Float.Text, Font, FAbyssUiStyle::WithAlpha(Float.Color, Alpha), FMath::Max(0.05f, Scale));
}

bool SAbyssWorldLayer::FloatMotion(const FFloat& Float, double T, FVector2D& OutOffset, float& OutAlpha, float& OutScale)
{
	OutOffset = Float.StartOffset;
	OutAlpha = 1.f;
	OutScale = 1.f;
	switch (Float.Kind)
	{
	case abyss::FloatingTextKind::Miss:
	{
		// web: scale 0.8 -> 1 in 90 ms (Quad.easeOut); after 120 ms y - 22 px and alpha -> 0 in one 650 ms Quad.easeOut tween
		if (T > 0.77)
		{
			return false;
		}
		OutScale = 0.8f + 0.2f * AbyssEase::QuadOut(AbyssEase::Clamp01(static_cast<float>(T / 0.09)));
		const float E = AbyssEase::QuadOut(AbyssEase::Clamp01(static_cast<float>((T - 0.12) / 0.65)));
		OutOffset.Y -= 22.0 * E;
		OutAlpha = 1.f - E;
		return true;
	}
	case abyss::FloatingTextKind::Exp:
	case abyss::FloatingTextKind::Gold:
	case abyss::FloatingTextKind::Embers:
	{
		// web: y and alpha -> 0 in one 'Power2' tween (Phaser Power2 = Cubic.Out; ZoneScene.ts onMonsterKilled / floatText):
		// +EXP 35 px over 1500 ms, +G 30 px over 1200 ms, +Embers 30 px over 1400 ms. Half visible after ~0.2 of the life.
		const bool bExp = Float.Kind == abyss::FloatingTextKind::Exp;
		const double Duration = bExp ? 1.5 : (Float.Kind == abyss::FloatingTextKind::Gold ? 1.2 : 1.4);
		if (T > Duration)
		{
			return false;
		}
		const float E = AbyssEase::CubicOut(AbyssEase::Clamp01(static_cast<float>(T / Duration)));
		OutOffset.Y -= (bExp ? 35.0 : 30.0) * E;
		OutAlpha = 1.f - E;
		return true;
	}
	case abyss::FloatingTextKind::Status:
	case abyss::FloatingTextKind::Custom:
	{
		if (T > 1.4)
		{
			return false;
		}
		const float U = AbyssEase::Clamp01(static_cast<float>(T / 1.4));
		OutOffset.Y -= 30.0 * AbyssEase::QuadOut(U);
		OutAlpha = U > 0.6f ? 1.f - (U - 0.6f) / 0.4f : 1.f;
		return true;
	}
	default:
		break;
	}
	// Hit numbers: pop from 0.35 (crit) / 0.5 -> 1.5 / 1.2 in 90 / 70 ms (quad-out) -> settle 1.1 / 1.0 in 160 / 110 ms
	// (back-out); life 1050 / 780 ms: x eases to start + drift (sine-out), y rises 40 / 28 px over 0.45 L, then sinks 8 px
	// while fading out over 0.55 L (quad-in).
	const bool bCrit = Float.bCrit;
	const double Life = bCrit ? 1.05 : 0.78;
	if (T > Life)
	{
		return false;
	}
	const double PopTime = bCrit ? 0.09 : 0.07;
	const double SettleTime = bCrit ? 0.16 : 0.11;
	const float From = bCrit ? 0.35f : 0.5f;
	const float Peak = bCrit ? 1.5f : 1.2f;
	const float Rest = bCrit ? 1.1f : 1.f;
	if (T < PopTime)
	{
		OutScale = FMath::Lerp(From, Peak, AbyssEase::QuadOut(AbyssEase::Clamp01(static_cast<float>(T / PopTime))));
	}
	else
	{
		OutScale = FMath::Lerp(Peak, Rest, AbyssEase::BackOut(AbyssEase::Clamp01(static_cast<float>((T - PopTime) / SettleTime))));
	}
	const double Rise = bCrit ? 40.0 : 28.0;
	OutOffset.X += Float.Drift * AbyssEase::SineOut(AbyssEase::Clamp01(static_cast<float>(T / Life)));
	const double RiseT = Life * 0.45;
	if (T < RiseT)
	{
		OutOffset.Y -= Rise * AbyssEase::QuadOut(AbyssEase::Clamp01(static_cast<float>(T / RiseT)));
	}
	else
	{
		const float U = AbyssEase::Clamp01(static_cast<float>((T - RiseT) / (Life - RiseT)));
		OutOffset.Y -= Rise - 8.0 * AbyssEase::QuadIn(U);
		OutAlpha = 1.f - AbyssEase::QuadIn(U);
	}
	return true;
}
