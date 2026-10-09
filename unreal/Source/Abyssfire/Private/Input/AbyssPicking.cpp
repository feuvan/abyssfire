#include "Input/AbyssPicking.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

#include <limits>

#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssActorRegistry.h"
#include "Framework/AbyssSimDriver.h"
#include "Framework/AbyssTypes.h"
#include "Framework/AbyssUnits.h"

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

	/** Screen rectangle (viewport px) of a world box; false when a corner is behind the camera. */
	bool AbyssPick_ProjectBox(const APlayerController& Controller, const FBox& Box, FVector2D& OutMin, FVector2D& OutMax)
	{
		OutMin = FVector2D(std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
		OutMax = FVector2D(std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest());
		for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
		{
			const FVector Corner((CornerIndex & 1) ? Box.Max.X : Box.Min.X, (CornerIndex & 2) ? Box.Max.Y : Box.Min.Y,
				(CornerIndex & 4) ? Box.Max.Z : Box.Min.Z);
			FVector2D Projected;
			if (!Controller.ProjectWorldLocationToScreen(Corner, Projected, /*bPlayerViewportRelative*/ false))
			{
				return false;
			}
			OutMin = OutMin.ComponentMin(Projected);
			OutMax = OutMax.ComponentMax(Projected);
		}
		return true;
	}
}

namespace AbyssPicking
{
	EAbyssPickAction ClassifyEntity(const APlayerController& Controller, abyss::EntityId Id, abyss::EntityKind Kind,
		abyss::Vec2& OutTile, int32& OutPriority)
	{
		const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(&Controller);
		if (Driver == nullptr || Driver->GetSnapshotIndex().GetSnapshot() == nullptr)
		{
			return EAbyssPickAction::None;
		}
		const FAbyssSnapshotIndex& SnapIndex = Driver->GetSnapshotIndex();
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
		FAbyssPickResult Best;
		const UAbyssActorRegistry* Registry = UAbyssActorRegistry::Get(&Controller);
		if (Registry == nullptr || Registry->Num() == 0)
		{
			return Best;
		}
		const FVector CameraLocation = Controller.PlayerCameraManager != nullptr
			? Controller.PlayerCameraManager->GetCameraLocation()
			: FVector::ZeroVector;
		const double Slop = FMath::Max(0.0, static_cast<double>(SlopPixels));

		int32 BestPriority = TNumericLimits<int32>::Max();
		double BestCentredness = TNumericLimits<double>::Max();
		double BestDepth = TNumericLimits<double>::Max();

		Registry->ForEach([&](abyss::EntityId Id, AActor* Actor, abyss::EntityKind Kind)
		{
			if (Actor == nullptr || Actor->IsHidden())
			{
				return;
			}
			abyss::Vec2 Tile;
			int32 Priority = AbyssPickPriorityWalk;
			const EAbyssPickAction Action = ClassifyEntity(Controller, Id, Kind, Tile, Priority);
			if (Action == EAbyssPickAction::None || Priority > BestPriority)
			{
				return;
			}
			FVector2D RectMin;
			FVector2D RectMax;
			if (!AbyssPick_ProjectBox(Controller, AbyssPick_ActorWorldBox(*Actor, Kind), RectMin, RectMax))
			{
				return;
			}
			RectMin -= FVector2D(Slop, Slop);
			RectMax += FVector2D(Slop, Slop);
			if (ScreenPosition.X < RectMin.X || ScreenPosition.X > RectMax.X || ScreenPosition.Y < RectMin.Y
				|| ScreenPosition.Y > RectMax.Y)
			{
				return;
			}
			const FVector2D Centre = (RectMin + RectMax) * 0.5;
			const FVector2D HalfSize = (RectMax - RectMin) * 0.5;
			const double Centredness = FVector2D::Distance(ScreenPosition, Centre) / FMath::Max(1.0, HalfSize.GetMax());
			const double Depth = FVector::DistSquared(CameraLocation, Actor->GetActorLocation());

			const bool bBetter = Priority < BestPriority
				|| (Priority == BestPriority && (Centredness < BestCentredness - 1.0e-3
					|| (FMath::Abs(Centredness - BestCentredness) <= 1.0e-3 && Depth < BestDepth)));
			if (!bBetter)
			{
				return;
			}
			BestPriority = Priority;
			BestCentredness = Centredness;
			BestDepth = Depth;
			Best.Id = Id;
			Best.Kind = Kind;
			Best.Action = Action;
			Best.Tile = Tile;
		});
		return Best;
	}
}
