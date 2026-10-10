#include "UI/Hud/SAbyssHud.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SOverlay.h"

#include <string>
#include <variant>

#include "abyss/data/DataStore.h"
#include "abyss/hero/Buffs.h"
#include "abyss/items/Inventory.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/quests/QuestWorld.h"

#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssText.h"
#include "Input/Touch/SAbyssTouchControls.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Hud/AbyssMinimapTexture.h"
#include "UI/Hud/SAbyssHudBottom.h"
#include "UI/Hud/SAbyssHudWidgets.h"
#include "UI/Hud/SAbyssMinimap.h"
#include "UI/Hud/SAbyssNotices.h"

void SAbyssHud::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssMinimapTexture>& InMinimap)
{
	Ctx = InContext;
	State = MakeShared<FAbyssHudState>();
	MinimapTexture = InMinimap;
	SetVisibility(EVisibility::SelfHitTestInvisible);

	const TSharedRef<FAbyssHudState> StateRef = State.ToSharedRef();
	SAssignNew(Bottom, SAbyssHudBottom, InContext, StateRef);
	SAssignNew(CombatLog, SAbyssCombatLog, InContext);
	SAssignNew(Minimap, SAbyssMinimap, InContext, StateRef, InMinimap).Size(104.f).Rotate(true);
	SAssignNew(Tracker, SAbyssQuestTracker, InContext);
	SAssignNew(TargetFrame, SAbyssTargetFrame, InContext, StateRef);
	SAssignNew(BossBar, SAbyssBossBar, InContext, StateRef);
	SAssignNew(InfoPlate, SAbyssInfoPlate, InContext, StateRef);
	SAssignNew(DodgePlate, SAbyssDodgePlate, InContext, StateRef);
	SAssignNew(Buffs, SAbyssBuffRow, InContext, StateRef);
	SAssignNew(PetMedallion, SAbyssPetMedallion, InContext, StateRef);
	SAssignNew(Notices, SAbyssNotices, InContext);

	ChildSlot
	[
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		[
			SAssignNew(Canvas, SConstraintCanvas)
			.Visibility(EVisibility::SelfHitTestInvisible)
		]
		+ SOverlay::Slot()
		[
			Notices.ToSharedRef()
		]
	];
	RebuildLayout();
}

void SAbyssHud::RebuildLayout()
{
	const bool bTouch = Ctx->IsTouch();
	bLayoutTouch = bTouch;
	State->bTouch = bTouch;
	Canvas->ClearChildren();
	Bottom->RebuildLayout();
	CombatLog->SetTouch(bTouch);
	Tracker->SetTouch(bTouch);
	bTrackerDirty = true;

	const auto Place = [this](const TSharedRef<SWidget>& Widget, const FAnchors& Anchors, const FVector2D& Offset, const FVector2D& Alignment)
	{
		Canvas->AddSlot()
			.Anchors(Anchors)
			.Offset(FMargin(static_cast<float>(Offset.X), static_cast<float>(Offset.Y), 0.f, 0.f))
			.Alignment(Alignment)
			.AutoSize(true)
			[
				Widget
			];
	};

	if (!bTouch)
	{
		// save-ui-input 6.2 desktop column, anchored to the edges of wider / taller layouts.
		Place(Bottom.ToSharedRef(), FAnchors(0.5f, 1.f), FVector2D(0.0, 0.0), FVector2D(0.5, 1.0));
		Place(CombatLog.ToSharedRef(), FAnchors(0.f, 1.f), FVector2D(12.0, -12.0), FVector2D(0.0, 1.0));
		Place(DodgePlate.ToSharedRef(), FAnchors(0.f, 0.f), FVector2D(12.0, 12.0), FVector2D(0.0, 0.0));
		Place(Buffs.ToSharedRef(), FAnchors(0.f, 0.f), FVector2D(12.0, 44.0), FVector2D(0.0, 0.0));
		Place(TargetFrame.ToSharedRef(), FAnchors(0.5f, 0.f), FVector2D(0.0, 10.0), FVector2D(0.5, 0.0));
		Place(BossBar.ToSharedRef(), FAnchors(0.5f, 0.f), FVector2D(0.0, 48.0), FVector2D(0.5, 0.0));
		Place(InfoPlate.ToSharedRef(), FAnchors(1.f, 0.f), FVector2D(-222.0, 12.0), FVector2D(0.0, 0.0));
		Place(Minimap.ToSharedRef(), FAnchors(1.f, 0.f), FVector2D(-125.0, 77.0), FVector2D(0.0, 0.0));
		Place(PetMedallion.ToSharedRef(), FAnchors(1.f, 0.f), FVector2D(-163.0, 94.0), FVector2D(0.0, 0.0));
		Place(Tracker.ToSharedRef(), FAnchors(1.f, 0.f), FVector2D(-222.0, 204.0), FVector2D(0.0, 0.0));
	}
	else
	{
		// save-ui-input 6.2 touch column: the right side belongs to the skill fan, the top rows to the touch toggles.
		Place(Bottom.ToSharedRef(), FAnchors(0.f, 1.f), FVector2D(0.0, 0.0), FVector2D(0.0, 1.0));
		const FVector2D LogSize = CombatLog->GetDesignSize();
		Place(CombatLog.ToSharedRef(), FAnchors(0.f, 0.f), FVector2D(16.0, CombatLog->IsExpanded() ? 130.0 : 458.0 - LogSize.Y), FVector2D(0.0, 0.0));
		Place(Buffs.ToSharedRef(), FAnchors(0.f, 0.f), FVector2D(16.0, 92.0), FVector2D(0.0, 0.0));
		Place(TargetFrame.ToSharedRef(), FAnchors(0.5f, 0.f), FVector2D(-96.0 + TargetFrameX, 10.0), FVector2D(0.5, 0.0));
		Place(BossBar.ToSharedRef(), FAnchors(0.5f, 0.f), FVector2D(0.0, 48.0), FVector2D(0.5, 0.0));
		Place(InfoPlate.ToSharedRef(), FAnchors(1.f, 0.f), FVector2D(-222.0, 130.0), FVector2D(0.0, 0.0));
		Place(Minimap.ToSharedRef(), FAnchors(1.f, 0.f), FVector2D(-125.0, 205.0), FVector2D(0.0, 0.0));
		Place(Tracker.ToSharedRef(), FAnchors(0.f, 0.f), FVector2D(26.0, 134.0), FVector2D(0.0, 0.0));
	}
}

void SAbyssHud::ToggleCombatLog()
{
	CombatLog->SetExpanded(!CombatLog->IsExpanded());
	if (bLayoutTouch)
	{
		RebuildLayout();
	}
}

void SAbyssHud::OnSessionStarted()
{
	*State = FAbyssHudState();
	State->bTouch = Ctx->IsTouch();
	CombatLog->Clear();
	Notices->Clear();
	bTrackerDirty = true;
	if (bLayoutTouch != Ctx->IsTouch())
	{
		RebuildLayout();
	}
}

void SAbyssHud::OnSessionEnded()
{
	State->bValid = false;
	Notices->Clear();
	MinimapTexture->Reset();
}

// =====================================================================================================================
// Sync
// =====================================================================================================================

void SAbyssHud::Sync(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame)
{
	const double Now = Ctx->Now();
	const float DeltaSeconds = static_cast<float>(FMath::Clamp(Frame.RealDeltaMs / 1000.0, 0.0, 0.25));
	if (bLayoutTouch != Ctx->IsTouch())
	{
		RebuildLayout();
	}
	State->bCinematic = Snap.cinematic;
	SyncHero(Snap, Now, DeltaSeconds);
	SyncTarget(Snap);
	MinimapTexture->Update(Snap, Now);
	SyncMinimapMarkers(Snap, Now);
	if (bTrackerDirty || Now >= NextTrackerRefresh)
	{
		Tracker->Refresh(Snap, false);
		bTrackerDirty = false;
		NextTrackerRefresh = Now + 0.25;
	}
	if (bLayoutTouch)
	{
		PlaceTargetFrame();
	}
	State->bValid = true;
}

void SAbyssHud::PlaceTargetFrame()
{
	// world 7.4: at large touch scales the top-left toggle row reaches the target frame: keep the frame right of it.
	const TSharedPtr<SAbyssTouchControls> Controls = TouchControls.Pin();
	double ShiftX = 0.0;
	if (Controls.IsValid() && Controls->IsLayerShown())
	{
		TArray<FSlateRect> Rects;
		Controls->GetOccupiedRects(Rects);
		const FVector2D Size(GetTickSpaceGeometry().GetLocalSize());
		const double FrameLeft = Size.X * 0.5 - 96.0 - 130.0;
		double RowRight = 0.0;
		for (const FSlateRect& Rect : Rects)
		{
			if (Rect.Top < 64.f && Rect.Left < Size.X * 0.5)
			{
				RowRight = FMath::Max(RowRight, static_cast<double>(Rect.Right));
			}
		}
		if (RowRight + 8.0 > FrameLeft)
		{
			ShiftX = RowRight + 8.0 - FrameLeft;
		}
	}
	if (!FMath::IsNearlyEqual(ShiftX, TargetFrameX, 1.0))
	{
		TargetFrameX = ShiftX;
		RebuildLayout();
	}
}

void SAbyssHud::SyncHero(const abyss::Snapshot& Snap, double Now, float DeltaSeconds)
{
	FAbyssHudState& S = *State;
	const abyss::HeroView& Hero = Snap.hero;
	const abyss::DataStore* Data = Ctx->GetData();
	S.Class = Hero.cls;
	S.Hp = Hero.hp;
	S.MaxHp = FMath::Max(1.0, Hero.maxHp);
	S.Mana = Hero.mana;
	S.MaxMana = FMath::Max(1.0, Hero.maxMana);
	const float HpRatio = FMath::Clamp(static_cast<float>(S.Hp / S.MaxHp), 0.f, 1.f);
	const float MpRatio = FMath::Clamp(static_cast<float>(S.Mana / S.MaxMana), 0.f, 1.f);
	if (!S.bValid)
	{
		S.HpLevel = HpRatio;
		S.MpLevel = MpRatio;
	}
	else
	{
		// 6.3: level += (ratio - level) * 0.15 per frame, made frame-rate independent (Q31).
		S.HpLevel = AbyssEase::Approach(S.HpLevel, HpRatio, 0.15f, DeltaSeconds);
		S.MpLevel = AbyssEase::Approach(S.MpLevel, MpRatio, 0.15f, DeltaSeconds);
	}
	S.WavePhase += DeltaSeconds * 2.2f;
	if (S.WavePhase > 1000.f)
	{
		S.WavePhase -= 1000.f;
	}
	S.bDying = Hero.life == abyss::HeroLife::Dying || Hero.hp <= 0.0;
	S.bLowHp = Hero.lowHp;
	S.Level = Hero.level;
	S.Exp = Hero.exp;
	S.ExpToNext = FMath::Max<int64>(1, Hero.expToNext);
	S.Gold = Hero.gold;
	S.FreeStatPoints = Hero.freeStatPoints;
	S.FreeSkillPoints = Hero.freeSkillPoints;
	S.Spirit = Hero.spirit;
	S.SpiritMax = FMath::Max(1.0, Hero.spiritMax);
	S.bResonating = Hero.resonating;
	S.ResonanceRemainingMs = Hero.resonanceRemainingMs;
	if (Data != nullptr)
	{
		S.SpiritColor = FAbyssUiStyle::Rgb(Data->Classes().spirit.For(Hero.cls).visualColor);
	}

	for (int32 Slot = 0; Slot < 6; ++Slot)
	{
		const abyss::SkillSlotView& View = Hero.hotbar[static_cast<size_t>(Slot)];
		FAbyssHudSkillSlot& Out = S.Slots[Slot];
		const bool bWasCooling = Out.CooldownRemainingMs > 0.0;
		Out.bBound = View.skillIndex >= 0 && !View.skillId.empty();
		if (Out.SkillId != View.skillId)
		{
			Out.SkillId = View.skillId;
			Out.Name = Out.bBound ? Ctx->SkillName(View.skillId) : FString();
			Out.ReadyFlashTime = -10.0;
		}
		Out.Level = View.level;
		Out.CooldownRemainingMs = View.cooldownRemainingMs;
		Out.CooldownTotalMs = View.cooldownTotalMs;
		Out.bUsable = View.usable;
		Out.bAffordable = View.affordable;
		if (const abyss::SkillDef* Skill = Data && Out.bBound ? Data->FindSkill(View.skillId) : nullptr)
		{
			Out.DamageType = Skill->damageType;
		}
		if (bWasCooling && Out.CooldownRemainingMs <= 0.0 && S.bValid)
		{
			Out.ReadyFlashTime = Now;
		}
	}
	S.DodgeRemainingMs = Hero.dodgeCooldownRemainingMs;
	S.DodgeTotalMs = Hero.dodgeCooldownMs;
	S.bAutoCombat = Hero.autoCombat;
	S.AutoLoot = Hero.autoLoot;
	S.Potions[0] = Snap.potionSlots[0];
	S.Potions[1] = Snap.potionSlots[1];
	S.bPortaling = Hero.portaling;
	S.PortalProgress = Hero.portalProgress;
	S.PromptKind = Snap.prompt.kind;
	S.StatusMask = Hero.statusMask;
	S.Buffs.Reset();
	if (Snap.heroBuffs != nullptr)
	{
		for (const abyss::ActiveBuff& Buff : Snap.heroBuffs->Items())
		{
			if (!Buff.ActiveAt(Snap.simNowMs))
			{
				continue;
			}
			FAbyssHudBuff Entry;
			Entry.bStatus = false;
			Entry.Stat = Buff.stat;
			Entry.RemainingMs = Buff.RemainingMs(Snap.simNowMs);
			Entry.DurationMs = Buff.durationMs;
			Entry.Value = Buff.value;
			S.Buffs.Add(Entry);
		}
	}

	// zone / info plate
	if (S.MapId != Snap.zone.mapId)
	{
		S.MapId = Snap.zone.mapId;
	}
	S.ZoneName = Snap.zone.nameKey.empty() ? Ctx->ZoneName(Snap.zone.mapId) : Ctx->NameOr(Snap.zone.nameKey, Snap.zone.mapId);
	S.ZoneLevelMin = Snap.zone.levelMin;
	S.ZoneLevelMax = Snap.zone.levelMax;

	// pets (U6: the medallion once a beast is owned)
	S.bHasPets = Snap.pets != nullptr && !Snap.pets->Owned().empty();
	S.ActivePet.clear();
	S.ActivePetStage = 0;
	if (Snap.pets != nullptr)
	{
		if (const abyss::PetInstance* Active = Snap.pets->Active())
		{
			S.ActivePet = Active->petId;
			S.ActivePetStage = Active->evolved;
		}
	}
	S.bActivePetExhausted = Snap.pet.present && Snap.pet.exhausted;

	// hero position at the interpolated pose and the camera yaw (W4)
	S.HeroPos = abyss::Vec2(Hero.prevPos.x + (Hero.pos.x - Hero.prevPos.x) * Snap.interpolationAlpha,
		Hero.prevPos.y + (Hero.pos.y - Hero.prevPos.y) * Snap.interpolationAlpha);
	if (const UAbyssGameInstance* GameInstance = Ctx->GetGameInstance())
	{
		if (const APlayerController* Controller = GameInstance->GetFirstLocalPlayerController())
		{
			if (Controller->PlayerCameraManager != nullptr)
			{
				S.CameraYawDeg = static_cast<float>(Controller->PlayerCameraManager->GetCameraRotation().Yaw);
			}
		}
	}
}

void SAbyssHud::SyncTarget(const abyss::Snapshot& Snap)
{
	FAbyssHudState& S = *State;
	S.Target = Snap.hero.target;
	S.bTargetFound = false;
	S.TargetName.Reset();
	if (S.Target != abyss::kNoEntity)
	{
		for (const abyss::MonsterView& Monster : Snap.monsters)
		{
			if (Monster.id == S.Target)
			{
				S.bTargetFound = Monster.alive;
				S.TargetHp = Monster.hp;
				S.TargetMaxHp = FMath::Max(1.0, Monster.maxHp);
				S.TargetName = Monster.nameKey.empty() ? Ctx->MonsterName(Monster.defId) : Ctx->NameOr(Monster.nameKey, Monster.defId);
				break;
			}
		}
	}
	S.bBossBar = Snap.bossBar.show;
	if (S.bBossBar)
	{
		S.BossName = Snap.bossBar.nameKey.empty() ? FString() : Ctx->LocStr(Snap.bossBar.nameKey);
		S.BossEpithet = Snap.bossBar.epithetKey.empty() ? FString() : Ctx->LocStr(Snap.bossBar.epithetKey);
		S.BossHp = Snap.bossBar.hp;
		S.BossMaxHp = FMath::Max(1.0, Snap.bossBar.maxHp);
	}
}

void SAbyssHud::SyncMinimapMarkers(const abyss::Snapshot& Snap, double Now)
{
	FAbyssMinimapMarkers& M = State->Minimap;
	if (Now < M.NextRefresh)
	{
		return;
	}
	M.NextRefresh = Now + 0.25;   // web refresh: every 250 ms
	M.Monsters.Reset();
	M.Npcs.Reset();
	M.Exits.Reset();
	M.SealedExits.Reset();
	M.Camps.Reset();
	M.QuestAreas.Reset();
	M.ExplorePoints.Reset();
	M.Clues.Reset();
	M.EscortDestinations.Reset();
	M.EscortNpcs.Reset();
	M.DefendTargets.Reset();
	M.GatherNodes.Reset();

	for (const abyss::MonsterView& Monster : Snap.monsters)
	{
		if (!Monster.alive)
		{
			continue;
		}
		FAbyssMinimapMarkers::FDot Dot;
		Dot.Pos = Monster.pos;
		Dot.Kind = (Monster.state == abyss::MonsterState::Chase || Monster.state == abyss::MonsterState::Attack) ? 1 : 0;
		M.Monsters.Add(Dot);
	}
	for (const abyss::NpcView& Npc : Snap.npcs)
	{
		if (Npc.marker == abyss::NpcMarker::TurnIn || Npc.marker == abyss::NpcMarker::Available)
		{
			FAbyssMinimapMarkers::FDot Dot;
			Dot.Pos = Npc.pos;
			Dot.Kind = Npc.marker == abyss::NpcMarker::TurnIn ? 1 : 2;
			M.Npcs.Add(Dot);
		}
	}
	for (const abyss::WorldMarkerView& Marker : Snap.markers)
	{
		switch (Marker.kind)
		{
		case abyss::MarkerKind::Exit:
			(Marker.sealed ? M.SealedExits : M.Exits).Add(Marker.pos);
			break;
		case abyss::MarkerKind::Camp: M.Camps.Add(Marker.pos); break;
		case abyss::MarkerKind::Clue: M.Clues.Add(Marker.pos); break;
		case abyss::MarkerKind::GatherNode: M.GatherNodes.Add(Marker.pos); break;
		case abyss::MarkerKind::EscortNpc: M.EscortNpcs.Add(Marker.pos); break;
		case abyss::MarkerKind::DefendTarget: M.DefendTargets.Add(Marker.pos); break;
		default: break;
		}
	}
	if (Snap.escort != nullptr && Snap.escort->active)
	{
		M.EscortDestinations.Add(Snap.escort->dest.Center());
		if (M.EscortNpcs.Num() == 0)
		{
			M.EscortNpcs.Add(Snap.escort->pos);
		}
	}
	if (Snap.defend != nullptr && Snap.defend->active && M.DefendTargets.Num() == 0)
	{
		M.DefendTargets.Add(Snap.defend->pos);
	}
	// layer 8: per open quest of this zone, its area and unfinished explore locations
	if (Snap.quests != nullptr)
	{
		for (const std::pair<const abyss::QuestDef*, const abyss::QuestProgress*>& Open : Snap.quests->OpenQuests())
		{
			const abyss::QuestDef* Def = Open.first;
			const abyss::QuestProgress* Progress = Open.second;
			if (Def == nullptr || Progress == nullptr || Def->zone != Snap.zone.mapId || Progress->status != abyss::QuestStatus::Active)
			{
				continue;
			}
			if (Def->hasQuestArea)
			{
				FAbyssMinimapMarkers::FArea Area;
				Area.Pos = Def->questArea.Center();
				Area.Radius = Def->questArea.radius;
				Area.bMain = Def->category == abyss::QuestCategory::Main;
				M.QuestAreas.Add(Area);
			}
			for (size_t Index = 0; Index < Def->objectives.size(); ++Index)
			{
				const abyss::QuestObjectiveDef& Objective = Def->objectives[Index];
				const int32 Current = Index < Progress->objectives.size() ? Progress->objectives[Index] : 0;
				if (Objective.type == abyss::ObjectiveType::Explore && Objective.hasLocation && Current < Objective.required)
				{
					M.ExplorePoints.Add(Objective.location.Center());
				}
			}
		}
	}
	M.bGuide = Snap.guide.Valid();
	if (M.bGuide)
	{
		M.Guide = Snap.guide.pos;
		M.bGuideTurnIn = Snap.guide.reason == abyss::GuideReason::TurnIn;
	}
}

// =====================================================================================================================
// Events
// =====================================================================================================================

void SAbyssHud::HandleEvent(const abyss::Event& Event, const abyss::Snapshot* Snap)
{
	if (const abyss::EvLog* Log = std::get_if<abyss::EvLog>(&Event))
	{
		CombatLog->AddLine(Log->text, Log->type);
	}
	else if (const abyss::EvBanner* Banner = std::get_if<abyss::EvBanner>(&Event))
	{
		const int32 Min = Snap ? Snap->zone.levelMin : State->ZoneLevelMin;
		const int32 Max = Snap ? Snap->zone.levelMax : State->ZoneLevelMax;
		Notices->ShowBanner(Banner->kind, Banner->title, Banner->subtitle, Min, Max);
	}
	else if (const abyss::EvLevelUp* LevelUp = std::get_if<abyss::EvLevelUp>(&Event))
	{
		Notices->ShowLevelUp(LevelUp->level);
	}
	else if (const abyss::EvItemPicked* Picked = std::get_if<abyss::EvItemPicked>(&Event))
	{
		FString Name;
		if (Snap != nullptr && Snap->inventory != nullptr)
		{
			if (const abyss::ItemInstance* Item = Snap->inventory->FindInBag(Picked->itemUid))
			{
				Name = Ctx->ItemName(*Item);
			}
		}
		if (Name.IsEmpty())
		{
			Name = Ctx->ItemBaseName(Picked->baseId);
		}
		Notices->AddLootNotice(Picked->baseId, Picked->quality, Picked->quantity, Name);
	}
	else if (const abyss::EvAchievementUnlocked* Achievement = std::get_if<abyss::EvAchievementUnlocked>(&Event))
	{
		Notices->ShowAchievement(Achievement->achievementId);
	}
	else if (std::holds_alternative<abyss::EvHeroDied>(Event))
	{
		Notices->ShowDeath(true);
	}
	else if (std::holds_alternative<abyss::EvHeroRespawned>(Event))
	{
		Notices->ShowDeath(false);
	}
	else if (const abyss::EvSkillBuffered* Buffered = std::get_if<abyss::EvSkillBuffered>(&Event))
	{
		const double Now = Ctx->Now();
		const double RemainingMs = Snap ? FMath::Max(0.0, Buffered->expiresAtMs - Snap->simNowMs) : 180.0;
		for (FAbyssHudSkillSlot& Slot : State->Slots)
		{
			if (Slot.SkillId == Buffered->skillId)
			{
				Slot.BufferedUntil = Now + RemainingMs / 1000.0;
			}
		}
	}
	else if (std::holds_alternative<abyss::EvQuestUpdate>(Event))
	{
		bTrackerDirty = true;
	}
	else if (const abyss::EvZone* Zone = std::get_if<abyss::EvZone>(&Event))
	{
		if (Zone->phase == abyss::EvZone::Phase::Entered)
		{
			State->Minimap.NextRefresh = 0.0;
			bTrackerDirty = true;
		}
	}
}
