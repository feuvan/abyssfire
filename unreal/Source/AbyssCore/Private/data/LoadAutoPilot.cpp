// autopilot_routes.json (implementation-plan.md 6): the bot's routes. Shape checks here (step kinds, the fields each
// kind needs); DataStore::Finalize checks the ids against the other tables.
#include "abyss/base/Platform.h"

#include <set>

#include "JsonReader.h"
#include "abyss/base/StrUtil.h"

namespace abyss {

const AutoPilotRoute* AutoPilotRouteTables::Find(std::string_view id) const {
  for (const AutoPilotRoute& r : routes) {
    if (r.id == id) return &r;
  }
  return nullptr;
}

const AutoPilotRoute* AutoPilotRouteTables::ForZone(std::string_view zone) const {
  for (const AutoPilotRoute& r : routes) {
    if (r.zone == zone) return &r;
  }
  return nullptr;
}

namespace dataload {

namespace {

// [col, row]
TilePos ApReadTile(const JNode& n) {
  const std::vector<JNode> xy = n.SelfItems();
  if (xy.size() != 2) {
    n.Error("expected a tile [col, row]");
    return {};
  }
  return TilePos{xy[0].AsInt(), xy[1].AsInt()};
}

std::vector<AutoPilotCondition> ApReadUntil(const JNode& s) {
  std::vector<AutoPilotCondition> out;
  for (const JNode& c : s.Items("until")) {
    AutoPilotCondition cond;
    cond.kind = c.Enum<AutoPilotConditionKind>("kind");
    cond.quest = c.Str("quest");
    if (cond.kind == AutoPilotConditionKind::Objective) {
      cond.obj = c.Int("obj");
      cond.atLeast = c.Int("atLeast");
      if (cond.obj < 0 || cond.atLeast < 1) c.Error("objective conditions need obj >= 0 and atLeast >= 1");
    }
    out.push_back(std::move(cond));
  }
  if (out.empty()) s.Error("'until' needs at least one condition");
  return out;
}

AutoPilotStep ApReadStep(const JNode& s) {
  AutoPilotStep st;
  st.kind = s.Enum<AutoPilotStepKind>("kind");
  using K = AutoPilotStepKind;
  switch (st.kind) {
    case K::Checkpoint:
      st.id = s.Str("id");
      if (st.id.empty()) s.Error("empty checkpoint id");
      break;
    case K::Note: st.id = s.Str("text"); break;
    case K::WaitIdle: st.maxSec = s.Num("maxSec"); break;
    case K::Talk:
    case K::TownTrip: st.npc = s.Str("npc"); break;
    case K::ClosePanels:
    case K::PortalHome:
    case K::EquipUpgrades:
    case K::SmokeEnd: break;
    case K::Tree:
      st.npc = s.Str("npc");
      st.ids = s.StrList("path");
      if (st.ids.empty()) s.Error("empty dialogue path");
      break;
    case K::Visit:
      st.npc = s.Str("npc");
      st.ids = s.StrList("accept");
      break;
    case K::Shop:
      st.npc = s.Str("npc");
      st.hp = s.Int("hp", -1);
      st.mp = s.Int("mp", -1);
      break;
    case K::Gather:
      st.quest = s.Str("quest");
      st.obj = s.Int("obj");
      st.count = s.Int("count");
      if (st.count < 1) s.Error("gather count must be >= 1");
      break;
    case K::Hunt:
      st.ids = s.StrList("monsters");
      if (st.ids.empty()) s.Error("a hunt needs monsters");
      st.tile = ApReadTile(s.Child("anchor"));
      st.until = ApReadUntil(s);
      st.maxSec = s.Num("maxSec");
      break;
    case K::Boss:
      st.role = s.Enum<AutoPilotBossRole>("role");
      st.tile = ApReadTile(s.Child("anchor"));
      st.until = ApReadUntil(s);
      st.maxSec = s.Num("maxSec");
      break;
    case K::Clues:
      st.quest = s.Str("quest");
      for (const JNode& t : s.Items("tiles")) st.tiles.push_back(ApReadTile(t));
      if (st.tiles.empty()) s.Error("clues need tiles");
      break;
    case K::Explore:
      st.quest = s.Str("quest");
      st.obj = s.Int("obj");
      st.tile = ApReadTile(s.Child("tile"));
      break;
    case K::Escort:
      st.quest = s.Str("quest");
      st.tries = s.Int("tries");
      break;
    case K::Defend:
      st.quest = s.Str("quest");
      st.tries = s.Int("tries");
      st.standOff = s.Num("standOff");
      if (st.standOff < 0) s.Error("standOff must be >= 0");
      break;
    case K::GoTo:
      st.tile = ApReadTile(s.Child("tile"));
      st.arrive = s.Num("arrive");
      st.maxSec = s.Num("maxSec");
      if (st.arrive <= 0) s.Error("arrive must be > 0");
      break;
  }
  if ((st.kind == K::Escort || st.kind == K::Defend) && st.tries < 1) s.Error("tries must be >= 1");
  if ((st.kind == K::WaitIdle || st.kind == K::Hunt || st.kind == K::Boss || st.kind == K::GoTo) && !(st.maxSec > 0)) {
    s.Error("maxSec must be > 0");
  }
  return st;
}

}  // namespace

void LoadAutoPilotRoutesFile(const JNode& r, AutoPilotRouteTables& out) {
  // The file names the step kinds it was written with: they must be this core's (same spelling, same order).
  const std::vector<std::string> kinds = r.StrList("stepKinds");
  bool same = kinds.size() == EnumCount<AutoPilotStepKind>();
  for (size_t i = 0; same && i < kinds.size(); ++i) same = kinds[i] == EnumName(static_cast<AutoPilotStepKind>(i));
  if (!same) r.Child("stepKinds").Error("step kinds differ from the core's AutoPilotStepKind");
  std::set<std::string> ids;
  for (const JNode& rn : r.Items("routes")) {
    AutoPilotRoute route;
    route.id = rn.Str("id");
    route.zone = rn.Str("zone");
    if (!ids.insert(route.id).second) rn.Error("duplicate route id '" + route.id + "'");
    std::set<std::string> checkpoints;
    size_t smokeEnds = 0;
    for (const JNode& sn : rn.Items("steps")) {
      AutoPilotStep st = ApReadStep(sn);
      if (st.kind == AutoPilotStepKind::Checkpoint && !checkpoints.insert(st.id).second) {
        sn.Error("duplicate checkpoint '" + st.id + "'");
      }
      if (st.kind == AutoPilotStepKind::SmokeEnd) ++smokeEnds;
      route.steps.push_back(std::move(st));
    }
    if (smokeEnds > 1) rn.Error("more than one smokeEnd");
    out.routes.push_back(std::move(route));
  }
}

}  // namespace dataload
}  // namespace abyss
