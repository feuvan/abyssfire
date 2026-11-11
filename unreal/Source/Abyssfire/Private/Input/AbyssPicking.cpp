#include "Input/AbyssPicking.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/MeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"

#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssActorRegistry.h"
#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssSimDriver.h"
#include "Framework/AbyssTypes.h"
#include "Framework/AbyssUnits.h"
#include "Input/AbyssPickRules.h"

namespace
{
	// Press priority of world-map-nav 7.1 (lower wins): loot, NPC, chest / event prop, monster, proximity-only objects.
	constexpr int32 AbyssPickPriorityLoot = 0;
	constexpr int32 AbyssPickPriorityNpc = 1;
	constexpr int32 AbyssPickPriorityProp = 2;
	constexpr int32 AbyssPickPriorityMonster = 3;
	constexpr int32 AbyssPickPriorityWalk = 4;

	/** Pick volume (cm) for an actor that has no visible mesh yet (radius, height above the actor origin). */
	void AbyssPick_FallbackVolume(abyss::EntityKind Kind, double& OutRadius, double& OutHeight)
	{
		switch (Kind)
		{
		case abyss::EntityKind::Monster:
			OutRadius = 45.0;
			OutHeight = 120.0;
			return;
		case abyss::EntityKind::Npc:
		case abyss::EntityKind::Escort:
			OutRadius = 40.0;
			OutHeight = 175.0;
			return;
		case abyss::EntityKind::GroundItem:
		case abyss::EntityKind::PotionDrop:
			OutRadius = 30.0;
			OutHeight = 30.0;
			return;
		default:
			OutRadius = 45.0;
			OutHeight = 100.0;
			return;
		}
	}

	/** World box of the actor's visible mesh components (characters, props, loot meshes), else the fallback cylinder. */
	FBox AbyssPick_ActorWorldBox(const AActor& Actor, abyss::EntityKind Kind)
	{
		FBox MeshBox(ForceInit);
		TInlineComponentArray<UMeshComponent*> Meshes(&Actor);
		for (const UMeshComponent* Mesh : Meshes)
		{
			if (Mesh != nullptr && Mesh->IsRegistered() && Mesh->IsVisible())
			{
				MeshBox += Mesh->Bounds.GetBox();
			}
		}
		if (MeshBox.IsValid && MeshBox.GetExtent().GetMax() > 5.0)
		{
			return MeshBox;
		}
		double Radius = 45.0;
		double Height = 100.0;
		AbyssPick_FallbackVolume(Kind, Radius, Height);
		const FVector Base = Actor.GetActorLocation();
		return FBox(Base - FVector(Radius, Radius, 0.0), Base + FVector(Radius, Radius, Height));
	}

	/** One pick's projection, fetched once (audit F4: APlayerController::ProjectWorldLocationToScreen rebuilt the
	 *  view-projection matrix for each of the 8 corners of each candidate). */
	struct FAbyssPickProjection
	{
		FMatrix ViewProjection = FMatrix::Identity;
		FIntRect ViewRect;
		AbyssPickRules::FView Rules;
	};

	/** What UGameplayStatics::ProjectWorldToScreen reads per call: the local player's projection data. False when the
	 *  controller has no viewport (the per-corner call then failed for every corner: nothing was pickable). */
	bool AbyssPick_FetchProjection(const APlayerController& Controller, FAbyssPickProjection& Out)
	{
		const ULocalPlayer* LocalPlayer = Controller.GetLocalPlayer();
		if (LocalPlayer == nullptr || LocalPlayer->ViewportClient == nullptr || LocalPlayer->ViewportClient->Viewport == nullptr)
		{
			return false;
		}
		FSceneViewProjectionData ProjectionData;
		if (!LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
		{
			return false;
		}
		Out.ViewProjection = ProjectionData.ComputeViewProjectionMatrix();
		Out.ViewRect = ProjectionData.GetConstrainedViewRect();
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Col = 0; Col < 4; ++Col)
			{
				Out.Rules.M[Row][Col] = Out.ViewProjection.M[Row][Col];
			}
		}
		Out.Rules.RectX = Out.ViewRect.Min.X;
		Out.Rules.RectY = Out.ViewRect.Min.Y;
		Out.Rules.RectW = Out.ViewRect.Width();
		Out.Rules.RectH = Out.ViewRect.Height();
		return true;
	}

	/** Screen rectangle (viewport px) of a world box; false when a corner is behind the camera. The engine's own
	 *  projection with the cached matrix: the numbers ProjectWorldLocationToScreen(bPlayerViewportRelative false) gave. */
	bool AbyssPick_ProjectBox(const FAbyssPickProjection& Projection, const FBox& Box, AbyssPickRules::FRect& OutRect)
	{
		AbyssPickRules::FVec2 Corners[8];
		for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
		{
			const FVector Corner((CornerIndex & 1) ? Box.Max.X : Box.Min.X, (CornerIndex & 2) ? Box.Max.Y : Box.Min.Y,
				(CornerIndex & 4) ? Box.Max.Z : Box.Min.Z);
			FVector2D Projected;
			if (!FSceneView::ProjectWorldToScreen(Corner, Projection.ViewRect, Projection.ViewProjection, Projected))
			{
				return false;
			}
			Corners[CornerIndex] = AbyssPickRules::FVec2{ Projected.X, Projected.Y };
		}
		OutRect = AbyssPickRules::RectOf(Corners);
		return true;
	}

	/**
	 * The pickable entity under ScreenPosition: web press priority, then the most centred rectangle, then the nearest to the
	 * camera (AbyssPickRules::FPicker). bMonstersOnly: only living monsters compete (PickMonster).
	 */
	FAbyssPickResult AbyssPick_Best(const APlayerController& Controller, const FVector2D& ScreenPosition, float SlopPixels,
		bool bMonstersOnly)
	{
		FAbyssPickResult Best;
		const UAbyssActorRegistry* Registry = UAbyssActorRegistry::Get(&Controller);
		FAbyssPickProjection Projection;
		if (Registry == nullptr || Registry->Num() == 0 || !AbyssPick_FetchProjection(Controller, Projection))
		{
			return Best;
		}
		const FVector CameraLocation = Controller.PlayerCameraManager != nullptr
			? Controller.PlayerCameraManager->GetCameraLocation()
			: FVector::ZeroVector;
		const double Slop = FMath::Max(0.0, static_cast<double>(SlopPixels));
		const AbyssPickRules::FVec2 Pointer{ ScreenPosition.X, ScreenPosition.Y };
		AbyssPickRules::FPicker Picker(Pointer, Slop);

		// Read-only visit (no registry copy): nothing below registers or unregisters an actor.
		Registry->ForEachReadOnly([&](abyss::EntityId Id, AActor* Actor, abyss::EntityKind Kind)
		{
			if (Actor == nullptr || Actor->IsHidden() || (bMonstersOnly && Kind != abyss::EntityKind::Monster))
			{
				return;
			}
			abyss::Vec2 Tile;
			int32 Priority = AbyssPickPriorityWalk;
			const EAbyssPickAction Action = AbyssPicking::ClassifyEntity(Controller, Id, Kind, Tile, Priority);
			if (Action == EAbyssPickAction::None || (bMonstersOnly && Action != EAbyssPickAction::Attack)
				|| !Picker.CanWin(Priority))
			{
				return;
			}
			const FBox Box = AbyssPick_ActorWorldBox(*Actor, Kind);
			// Pre-cull: a conservative screen bound of the box. Only boxes near the pointer are projected corner by corner.
			const double BoxMin[3] = { Box.Min.X, Box.Min.Y, Box.Min.Z };
			const double BoxMax[3] = { Box.Max.X, Box.Max.Y, Box.Max.Z };
			if (!AbyssPickRules::MayContain(Projection.Rules, BoxMin, BoxMax, Pointer, Slop))
			{
				return;
			}
			AbyssPickRules::FRect Rect;
			if (!AbyssPick_ProjectBox(Projection, Box, Rect))
			{
				return;
			}
			const double Depth = FVector::DistSquared(CameraLocation, Actor->GetActorLocation());
			if (Picker.Offer(Id, Priority, Rect, Depth))
			{
				Best.Id = Id;
				Best.Kind = Kind;
				Best.Action = Action;
				Best.Tile = Tile;
			}
		});
		return Best;
	}
}

namespace AbyssPicking
{
	EAbyssPickAction ClassifyEntity(const APlayerController& Controller, abyss::EntityId Id, abyss::EntityKind Kind,
		abyss::Vec2& OutTile, int32& OutPriority)
	{
		const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(&Controller);
		const UAbyssGameInstance* AbyssGame = UAbyssGameInstance::Get(&Controller);
		if (Driver == nullptr || AbyssGame == nullptr)
		{
			return EAbyssPickAction::None;
		}
		const FAbyssSnapshotIndex& SnapIndex = Driver->GetSnapshotIndex();
		// The index is rebuilt by the driver after each GameSim::Frame; a session started / ended by a UI flow call since
		// then leaves it pointing at another snapshot: pick nothing until the next rebuild.
		if (SnapIndex.GetSnapshot() == nullptr || SnapIndex.GetSnapshot() != AbyssGame->GetSnapshot())
		{
			return EAbyssPickAction::None;
		}
		switch (Kind)
		{
		case abyss::EntityKind::Monster:
			if (const abyss::MonsterView* Monster = SnapIndex.FindMonster(Id); Monster != nullptr && Monster->alive)
			{
				OutTile = Monster->pos;
				OutPriority = AbyssPickPriorityMonster;
				return EAbyssPickAction::Attack;
			}
			return EAbyssPickAction::None;
		case abyss::EntityKind::Npc:
			if (const abyss::NpcView* Npc = SnapIndex.FindNpc(Id))
			{
				OutTile = Npc->pos;
				OutPriority = AbyssPickPriorityNpc;
				return EAbyssPickAction::Interact;
			}
			return EAbyssPickAction::None;
		case abyss::EntityKind::GroundItem:
			if (const abyss::GroundItemView* Item = SnapIndex.FindGroundItem(Id))
			{
				OutTile = Item->pos;
				OutPriority = AbyssPickPriorityLoot;
				return EAbyssPickAction::PickUp;
			}
			return EAbyssPickAction::None;
		case abyss::EntityKind::PotionDrop:
			if (const abyss::PotionDropView* Potion = SnapIndex.FindPotion(Id))
			{
				OutTile = Potion->pos;   // auto-pickup within 2 tiles (loot spec 6.2): walking there is the action
				OutPriority = AbyssPickPriorityWalk;
				return EAbyssPickAction::WalkTo;
			}
			return EAbyssPickAction::None;
		case abyss::EntityKind::Prop:
		case abyss::EntityKind::Escort:
		case abyss::EntityKind::DefendTarget:
			if (const abyss::WorldMarkerView* Marker = SnapIndex.FindMarker(Id))
			{
				OutTile = Marker->pos;
				switch (Marker->kind)
				{
				case abyss::MarkerKind::HiddenReward:
				case abyss::MarkerKind::EventProp:
					// ZoneRuntime::InteractWith: walk-then-act (W8), chest within 2 tiles / puzzle prompt.
					OutPriority = AbyssPickPriorityProp;
					return EAbyssPickAction::Interact;
				case abyss::MarkerKind::Exit:
				case abyss::MarkerKind::Camp:
					return EAbyssPickAction::None;   // static, never registered as entities; the core chain handles exits
				default:
					// Proximity-only objects (world 7.4): gather nodes, clues, quest items, lore, soul echo, escort, defend,
					// story decorations - walking there is the action.
					OutPriority = AbyssPickPriorityWalk;
					return EAbyssPickAction::WalkTo;
				}
			}
			return EAbyssPickAction::None;
		default:
			return EAbyssPickAction::None;   // hero, pet, projectiles, ground effects are never press targets
		}
	}

	FAbyssPickResult PickEntity(const APlayerController& Controller, const FVector2D& ScreenPosition, float SlopPixels)
	{
		return AbyssPick_Best(Controller, ScreenPosition, SlopPixels, /*bMonstersOnly*/ false);
	}

	FAbyssPickResult PickMonster(const APlayerController& Controller, const FVector2D& ScreenPosition, float SlopPixels)
	{
		return AbyssPick_Best(Controller, ScreenPosition, SlopPixels, /*bMonstersOnly*/ true);
	}
}
