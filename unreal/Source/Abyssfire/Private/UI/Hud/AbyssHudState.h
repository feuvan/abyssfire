// FAbyssHudState: the HUD's per-frame copy of the snapshot (save-ui-input 6). Filled once per frame by SAbyssHud::Sync and
// read by the HUD widgets while they paint, so no widget keeps snapshot pointers (they are only valid until the next call
// into the GameSim).
#pragma once

#include "CoreMinimal.h"

#include <string>

#include "abyss/base/Enums.h"
#include "abyss/base/Types.h"
#include "abyss/hero/Hero.h"
#include "abyss/sim/SimTypes.h"
#include "abyss/sim/Snapshot.h"

struct FAbyssHudSkillSlot
{
	bool bBound = false;
	std::string SkillId;
	FString Name;
	int32 Level = 0;
	double CooldownRemainingMs = 0.0;
	double CooldownTotalMs = 0.0;
	bool bUsable = false;
	bool bAffordable = false;
	abyss::DamageType DamageType = abyss::DamageType::Physical;
	/** Real time the cooldown ended (ready flash 320 ms). */
	double ReadyFlashTime = -10.0;
	/** Real time until which the skill shows the buffered glow (EvSkillBuffered). */
	double BufferedUntil = -10.0;
};

struct FAbyssHudBuff
{
	/** Status (burn..stun) or buff stat. */
	bool bStatus = true;
	abyss::StatusType Status = abyss::StatusType::Burn;
	abyss::BuffStat Stat = abyss::BuffStat::DamageReduction;
	double RemainingMs = 0.0;
	double DurationMs = 0.0;
	double Value = 0.0;
};

/** Minimap layers (save-ui-input 6.11), refreshed every 250 ms (the web refresh). Tile-space positions. */
struct FAbyssMinimapMarkers
{
	struct FDot
	{
		abyss::Vec2 Pos;
		uint8 Kind = 0;   // monsters: 1 = aggro; NPCs: 1 = turn-in, 2 = available
	};
	struct FArea
	{
		abyss::Vec2 Pos;
		double Radius = 0.0;
		bool bMain = false;
	};
	TArray<FDot> Monsters;
	TArray<FDot> Npcs;
	TArray<abyss::Vec2> Exits;
	TArray<abyss::Vec2> SealedExits;
	TArray<abyss::Vec2> Camps;
	TArray<FArea> QuestAreas;
	TArray<abyss::Vec2> ExplorePoints;
	TArray<abyss::Vec2> Clues;
	TArray<abyss::Vec2> EscortDestinations;
	TArray<abyss::Vec2> EscortNpcs;
	TArray<abyss::Vec2> DefendTargets;
	TArray<abyss::Vec2> GatherNodes;
	bool bGuide = false;
	abyss::Vec2 Guide;
	bool bGuideTurnIn = false;
	double NextRefresh = 0.0;
};

struct FAbyssHudState
{
	bool bValid = false;
	bool bTouch = false;
	bool bCinematic = false;
	abyss::ClassId Class = abyss::ClassId::Warrior;

	// resources (orb levels animate towards the ratios, dt-correct, save-ui-input Q31)
	double Hp = 0.0, MaxHp = 1.0, Mana = 0.0, MaxMana = 1.0;
	float HpLevel = 1.f, MpLevel = 1.f;
	float WavePhase = 0.f;
	bool bDying = false;
	bool bLowHp = false;

	int32 Level = 1;
	int64 Exp = 0, ExpToNext = 1, Gold = 0;
	int32 FreeStatPoints = 0, FreeSkillPoints = 0;

	double Spirit = 0.0, SpiritMax = 100.0;
	bool bResonating = false;
	double ResonanceRemainingMs = 0.0;
	FLinearColor SpiritColor = FLinearColor(1.f, 0.6f, 0.2f);

	FAbyssHudSkillSlot Slots[6];
	double DodgeRemainingMs = 0.0, DodgeTotalMs = 0.0;
	bool bAutoCombat = false;
	abyss::AutoLootMode AutoLoot = abyss::AutoLootMode::Off;
	abyss::PotionSlotView Potions[2];

	// target frame (save-ui-input 6.7)
	abyss::EntityId Target = abyss::kNoEntity;
	FString TargetName;
	double TargetHp = 0.0, TargetMaxHp = 1.0;
	bool bTargetFound = false;

	// boss bar (6.13)
	bool bBossBar = false;
	FString BossName, BossEpithet;
	double BossHp = 0.0, BossMaxHp = 1.0;

	// town portal channel (W3) and the interact prompt (world 7.4)
	bool bPortaling = false;
	double PortalProgress = 0.0;
	abyss::InteractKind PromptKind = abyss::InteractKind::None;

	// statuses / buffs
	uint32 StatusMask = 0;
	TArray<FAbyssHudBuff> Buffs;

	// zone / info plate
	std::string MapId;
	FString ZoneName;
	int32 ZoneLevelMin = 1, ZoneLevelMax = 1;

	// pets (desktop medallion, U6)
	bool bHasPets = false;
	std::string ActivePet;
	int32 ActivePetStage = 0;
	bool bActivePetExhausted = false;

	// hero position (minimap centre, world-anchored popups) and the live camera yaw (W4 rotated minimap)
	abyss::Vec2 HeroPos;
	float CameraYawDeg = 45.f;
	FAbyssMinimapMarkers Minimap;
};
