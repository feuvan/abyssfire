#include "Framework/AbyssTypes.h"

const TCHAR* LexToString(EAbyssAppState State)
{
	switch (State)
	{
	case EAbyssAppState::Boot: return TEXT("Boot");
	case EAbyssAppState::DataError: return TEXT("DataError");
	case EAbyssAppState::MainMenu: return TEXT("MainMenu");
	case EAbyssAppState::InGame: return TEXT("InGame");
	}
	return TEXT("Unknown");
}

namespace
{
	template <class ViewT>
	void AbyssSnapIndex_IndexById(const std::vector<ViewT>& Views, TMap<uint32, int32>& Out)
	{
		Out.Reset();
		for (int32 Index = 0; Index < static_cast<int32>(Views.size()); ++Index)
		{
			const abyss::EntityId Id = Views[static_cast<size_t>(Index)].id;
			if (Id != abyss::kNoEntity)
			{
				Out.Add(Id, Index);
			}
		}
	}

	template <class ViewT>
	const ViewT* AbyssSnapIndex_FindById(const std::vector<ViewT>* Views, const TMap<uint32, int32>& Map, abyss::EntityId Id)
	{
		if (Views == nullptr)
		{
			return nullptr;
		}
		const int32* Index = Map.Find(Id);
		if (Index == nullptr || *Index < 0 || static_cast<size_t>(*Index) >= Views->size())
		{
			return nullptr;
		}
		return &(*Views)[static_cast<size_t>(*Index)];
	}
}

void FAbyssSnapshotIndex::Rebuild(const abyss::Snapshot& Snap)
{
	Snapshot = &Snap;
	AbyssSnapIndex_IndexById(Snap.monsters, Monsters);
	AbyssSnapIndex_IndexById(Snap.npcs, Npcs);
	AbyssSnapIndex_IndexById(Snap.groundItems, GroundItems);
	AbyssSnapIndex_IndexById(Snap.potions, Potions);
	AbyssSnapIndex_IndexById(Snap.projectiles, Projectiles);
	AbyssSnapIndex_IndexById(Snap.groundEffects, GroundEffects);
	AbyssSnapIndex_IndexById(Snap.markers, Markers);
}

void FAbyssSnapshotIndex::Reset()
{
	Snapshot = nullptr;
	Monsters.Reset();
	Npcs.Reset();
	GroundItems.Reset();
	Potions.Reset();
	Projectiles.Reset();
	GroundEffects.Reset();
	Markers.Reset();
}

const abyss::MonsterView* FAbyssSnapshotIndex::FindMonster(abyss::EntityId Id) const
{
	return AbyssSnapIndex_FindById(Snapshot ? &Snapshot->monsters : nullptr, Monsters, Id);
}

const abyss::NpcView* FAbyssSnapshotIndex::FindNpc(abyss::EntityId Id) const
{
	return AbyssSnapIndex_FindById(Snapshot ? &Snapshot->npcs : nullptr, Npcs, Id);
}

const abyss::GroundItemView* FAbyssSnapshotIndex::FindGroundItem(abyss::EntityId Id) const
{
	return AbyssSnapIndex_FindById(Snapshot ? &Snapshot->groundItems : nullptr, GroundItems, Id);
}

const abyss::PotionDropView* FAbyssSnapshotIndex::FindPotion(abyss::EntityId Id) const
{
	return AbyssSnapIndex_FindById(Snapshot ? &Snapshot->potions : nullptr, Potions, Id);
}

const abyss::ProjectileView* FAbyssSnapshotIndex::FindProjectile(abyss::EntityId Id) const
{
	return AbyssSnapIndex_FindById(Snapshot ? &Snapshot->projectiles : nullptr, Projectiles, Id);
}

const abyss::GroundEffectView* FAbyssSnapshotIndex::FindGroundEffect(abyss::EntityId Id) const
{
	return AbyssSnapIndex_FindById(Snapshot ? &Snapshot->groundEffects : nullptr, GroundEffects, Id);
}

const abyss::WorldMarkerView* FAbyssSnapshotIndex::FindMarker(abyss::EntityId Id) const
{
	return AbyssSnapIndex_FindById(Snapshot ? &Snapshot->markers : nullptr, Markers, Id);
}

bool FAbyssSnapshotIndex::FindEntityPosition(abyss::EntityId Id, abyss::Vec2& OutPrev, abyss::Vec2& OutCur) const
{
	if (Snapshot == nullptr || Id == abyss::kNoEntity)
	{
		return false;
	}
	if (Id == abyss::kHeroEntityId)
	{
		OutPrev = Snapshot->hero.prevPos;
		OutCur = Snapshot->hero.pos;
		return true;
	}
	if (Snapshot->pet.present && Snapshot->pet.id == Id)
	{
		OutPrev = Snapshot->pet.prevPos;
		OutCur = Snapshot->pet.pos;
		return true;
	}
	if (const abyss::MonsterView* Monster = FindMonster(Id))
	{
		OutPrev = Monster->prevPos;
		OutCur = Monster->pos;
		return true;
	}
	if (const abyss::NpcView* Npc = FindNpc(Id))
	{
		OutPrev = OutCur = Npc->pos;
		return true;
	}
	if (const abyss::GroundItemView* Item = FindGroundItem(Id))
	{
		OutPrev = OutCur = Item->pos;
		return true;
	}
	if (const abyss::PotionDropView* Potion = FindPotion(Id))
	{
		OutPrev = OutCur = Potion->pos;
		return true;
	}
	if (const abyss::ProjectileView* Projectile = FindProjectile(Id))
	{
		const double T = Projectile->progress;
		OutPrev = OutCur = abyss::Vec2(Projectile->from.x + (Projectile->to.x - Projectile->from.x) * T,
			Projectile->from.y + (Projectile->to.y - Projectile->from.y) * T);
		return true;
	}
	if (const abyss::GroundEffectView* Effect = FindGroundEffect(Id))
	{
		OutPrev = OutCur = Effect->center;
		return true;
	}
	if (const abyss::WorldMarkerView* Marker = FindMarker(Id))
	{
		OutPrev = OutCur = Marker->pos;
		return true;
	}
	return false;
}
