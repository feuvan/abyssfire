// IAbyssWorldUi: the world-anchored UI contract between the world agent (this side: positions) and the UI agent (the
// implementation: Slate widgets in one world-space overlay layer, ue58-platform.md 9.6). The UI agent implements it on
// its UI object and registers it with UAbyssWorldBuilder::SetWorldUi (e.g. from IAbyssUiRoot::OnAppStateChanged(MainMenu),
// when the game world exists). The world builder replays the current widgets on registration.
//
// The world side decides WHICH entities get a world widget and WHERE they are; the UI side decides what they show and
// reads the details (names, HP, quest markers, item quality, renames) from the snapshot by entity id, so locale changes
// need no extra calls (a rename re-sends the desc with the event's nameplate colour):
//   Monster   -> nameplate + HP bar (monsters-ai.md 5: hidden at full HP; name colour by elite / affix / story boss)
//   Npc       -> name + quest marker (NpcView::marker)
//   Pet       -> "{name} Lv.{level}" (+ exhausted)
//   Escort / DefendTarget -> name + HP bar (quests-story-ch1.md 3.9 / 3.10)
//   GroundItem-> loot label in the quality colour (art-inventory-ch1.md 7)
//   Lore / Prop -> "(star) name" label (lore pickups, hidden rewards, event props)
// Positions are UE world locations: the UI projects them (ProjectWorldLocationToScreen / DPI scale).
//
// Floating combat text (combat-feel.md 12) is pushed by the world side with its resolved world anchor (the entity's feet,
// as in the web) after the "damage numbers" setting was applied; stacking / tweening are the UI's.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "abyss/base/Enums.h"
#include "abyss/base/I18n.h"
#include "abyss/base/Types.h"
#include "abyss/sim/SimTypes.h"

#include "AbyssWorldUi.generated.h"

enum class EAbyssWorldWidgetKind : uint8
{
	Monster,
	Npc,
	Pet,
	Escort,
	DefendTarget,
	GroundItem,
	Lore,
	Prop,
};

/** Static description of a world widget (sent once; re-sent with the same id on change). */
struct FAbyssWorldWidgetDesc
{
	abyss::EntityId Id = abyss::kNoEntity;
	EAbyssWorldWidgetKind Kind = EAbyssWorldWidgetKind::Monster;
	abyss::EntityKind EntityKind = abyss::EntityKind::None;
	/** Monster def id / NPC id / pet id / item base id / lore id ("lore:<id>" props carry it after the colon). */
	FString DefId;
	/** Story-renamed monster: the nameplate colour of EvMonsterRenamed (0xRRGGBB). */
	uint32 NameColorRgb = 0;
	bool bHasNameColor = false;
};

/** Per-frame placement of one world widget. */
struct FAbyssWorldWidgetFrame
{
	abyss::EntityId Id = abyss::kNoEntity;
	/** Above the head (fx_overhead): nameplates, markers, labels. */
	FVector OverheadLocation = FVector::ZeroVector;
	/** Ground point under the entity. */
	FVector FeetLocation = FVector::ZeroVector;
	/** False while the actor is hidden (dead and faded, despawning, cinematic hide). */
	bool bVisible = true;
	/** 0..1 (death / despawn fades). */
	float Opacity = 1.f;
};

/** One floating combat text request (EvFloatingText with its anchor resolved). */
struct FAbyssFloatingTextRequest
{
	abyss::FloatingTextKind Kind = abyss::FloatingTextKind::Custom;
	abyss::EntityId Anchor = abyss::kNoEntity;
	/** Feet of the anchor entity (web anchor), else the event position on the ground. */
	FVector WorldLocation = FVector::ZeroVector;
	/** Top of the anchor (for kinds the UI prefers to show over the head). */
	FVector OverheadLocation = FVector::ZeroVector;
	double Value = 0.0;
	bool bCrit = false;
	abyss::DamageType Element = abyss::DamageType::Physical;
	/** Custom / Status text (i18n key + args). */
	abyss::LocText Text;
	/** Placement slot of the hit this number belongs to (EvHit::numberSlot of the same frame, else Primary). */
	abyss::HitNumberSlot Slot = abyss::HitNumberSlot::Primary;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UAbyssWorldUi : public UInterface
{
	GENERATED_BODY()
};

class ABYSSFIRE_API IAbyssWorldUi
{
	GENERATED_BODY()

public:
	/** Create (or replace) the world widget of an entity. */
	virtual void AddWorldWidget(const FAbyssWorldWidgetDesc& Desc) = 0;
	virtual void RemoveWorldWidget(abyss::EntityId Id) = 0;
	/** Zone teardown / session end: drop every world widget. */
	virtual void ClearWorldWidgets() = 0;
	/** Once per frame after the world view synced (before IAbyssUiRoot::SyncFrame): every live widget's placement. */
	virtual void UpdateWorldWidgets(TConstArrayView<FAbyssWorldWidgetFrame> Frames) = 0;
	virtual void ShowFloatingText(const FAbyssFloatingTextRequest& Request) = 0;
};
