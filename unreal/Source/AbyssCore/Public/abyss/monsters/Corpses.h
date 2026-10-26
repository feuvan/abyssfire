// The zone's corpse registry (dark remake K3, implementation-plan.md 3 K3; monsters.md 1.5, DECISIONS DR6).
//
// Owner area: monsters. A pure container: MonsterSystem registers the bodies (the kill pipeline's K3 step, the map's
// corpse seeds at zone entry), expires them, burns them, and stands them up (a raise, the Unburied rise); CombatSystem
// asks it for corpses to raise. No RNG, no SimContext.
//
// * Entries: {id, defId, family, kind, pos, deathMs, expiresMs, seeded, spent, monster, holder}. Ids count up for the
//   whole session (a zone change never reuses one). A kill's body lasts its TTL; a seeded body (map corpseSeeds) never
//   expires. At most `cap` kill bodies per zone: a new one beyond it pushes out the oldest kill bodies first (death time,
//   then id); seeds do not count.
// * Kinds (SimTypes.h CorpseKind): a `corpse` is raised (unless spent: a body that rose once) and eaten; `remains` are
//   neither (a carcass, a boss's body). Both burn. A holder (a raise winding up, a consume channel, a pending Unburied
//   rise) keeps everyone else off a corpse; burning or eviction removes it anyway (the holder finds it gone).
// * Queries sort by distance (squared, exact doubles), then death time, then id: the same order on every platform.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

struct CorpseEntry {
  uint32_t id = 0;
  std::string defId;   // the dead monster's definition id (a hunt leader's hunt id); a seed's monsterId
  std::string artId;   // its spriteKey (the husk / carcass look, U3)
  std::string family;  // MonsterDef::family ("" = none)
  CorpseKind kind = CorpseKind::Corpse;
  Vec2 pos;
  double deathMs = 0;
  double expiresMs = 0;          // 0 = never (a seed)
  bool seeded = false;           // map corpseSeeds: no TTL, outside the cap
  bool spent = false;            // the body already rose once (a Raised's, a self-risen monster's): never raised again
  EntityId monster = kNoEntity;  // the dead instance (kNoEntity for a seed)
  EntityId holder = kNoEntity;   // a raise / consume channel / pending rise holding it

  bool Raisable() const { return kind == CorpseKind::Corpse && !spent && holder == kNoEntity; }
  bool Consumable() const { return kind == CorpseKind::Corpse && holder == kNoEntity; }
};

enum class CorpseFilter : uint8_t { Any, Raisable, Consumable };

class ABYSS_API Corpses {
 public:
  // Adds `e` (its id is assigned and returned). A kill body (not seeded) beyond `cap` kill bodies (cap > 0) evicts the
  // oldest kill bodies (death time, then id) until `cap` remain; their ids go to `evicted` (removal order).
  uint32_t Add(CorpseEntry e, int32_t cap, std::vector<uint32_t>& evicted);
  bool Remove(uint32_t id);
  const CorpseEntry* Find(uint32_t id) const;
  // Kill bodies with 0 < expiresMs <= nowMs, add order.
  void CollectExpired(double nowMs, std::vector<uint32_t>& out) const;
  // Bodies within `radius` of `centre` (distance <= radius) passing `filter`, sorted (distance, death time, id).
  void Query(Vec2 centre, double radius, CorpseFilter filter, std::vector<uint32_t>& out) const;
  // Holds a free corpse for `holder` (false when it is gone or held by someone else).
  bool Hold(uint32_t id, EntityId holder);
  // Releases one / every corpse `holder` holds.
  void Release(uint32_t id, EntityId holder);
  void ReleaseAll(EntityId holder);
  void Clear() { entries_.clear(); }
  int32_t KillBodies() const;

  std::span<const CorpseEntry> Entries() const { return entries_; }

 private:
  CorpseEntry* FindMutable(uint32_t id);
  std::vector<CorpseEntry> entries_;  // add order
  uint32_t nextId_ = 1;
};

}  // namespace abyss
