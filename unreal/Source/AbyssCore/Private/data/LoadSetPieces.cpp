// set_pieces.json (implementation-plan.md 4 U4): the UE set-piece layer. Shape checks here; DataStore::Finalize checks
// the anchors, quests and lore against the other tables.
#include "abyss/base/Platform.h"

#include <set>

#include "JsonReader.h"

namespace abyss {

const SetPieceZoneDef* SetPieceTables::ForZone(std::string_view zone) const {
  for (const SetPieceZoneDef& z : zones) {
    if (z.zone == zone) return &z;
  }
  return nullptr;
}

namespace dataload {

namespace {

// [x, y] -> Vec2
Vec2 SpReadPair(const JNode& n) {
  const std::vector<JNode> xy = n.SelfItems();
  if (xy.size() != 2) {
    n.Error("expected [col, row]");
    return {};
  }
  return Vec2(xy[0].AsNum(), xy[1].AsNum());
}

SetPieceDef SpReadPiece(const JNode& p) {
  SetPieceDef d;
  d.id = p.Str("id");
  if (d.id.empty()) p.Error("empty set-piece id");
  d.area = p.Str("area", "");
  d.gameId = p.Str("gameId");
  if (d.gameId.empty()) p.Error("empty gameId");
  d.fallbackIds = p.StrList("fallbackIds", true);
  d.variant = p.Int("variant", -1);
  if (d.variant < -1) p.Child("variant").Error("variant must be >= -1");

  const JNode a = p.Child("anchor");
  d.anchor = a.Enum<SetPieceAnchorKind>("kind");
  switch (d.anchor) {
    case SetPieceAnchorKind::Tile: d.tile = Vec2(a.Num("col"), a.Num("row")); break;
    case SetPieceAnchorKind::Camp:
      d.camp = a.Int("camp");
      d.ref = a.Str("prop", "");
      if (d.camp < 1) a.Error("camp numbers start at 1");
      break;
    case SetPieceAnchorKind::Entrance:
    case SetPieceAnchorKind::Region:
      d.ref = a.Str("id");
      if (d.ref.empty()) a.Error("empty anchor id");
      break;
    case SetPieceAnchorKind::DefendTarget:
      d.ref = a.Str("quest");
      if (d.ref.empty()) a.Error("empty anchor quest");
      break;
  }
  if (p.Has("offset")) d.offset = SpReadPair(p.Child("offset"));

  if (p.Has("ring")) {
    const JNode r = p.Child("ring");
    d.hasRing = true;
    d.ring.count = r.Int("count");
    d.ring.radius = r.Num("radius");
    d.ring.startDeg = r.Num("startDeg", 0.0);
    d.ring.stepDeg = r.Num("stepDeg", d.ring.count > 0 ? 360.0 / d.ring.count : 0.0);
    d.ring.skip = r.IntList("skip", true);
    d.ring.variants = r.IntList("variants", true);
    if (d.ring.count < 1 || d.ring.count > 64) r.Error("ring count must be 1..64");
    if (!(d.ring.radius > 0)) r.Error("ring radius must be > 0");
    for (const int32_t k : d.ring.skip) {
      if (k < 0 || k >= d.ring.count) r.Error("ring skip index out of range");
    }
    for (const int32_t v : d.ring.variants) {
      if (v < 0) r.Error("ring variants are asset indices (>= 0)");
    }
  }

  const JNode f = p.Child("facing");
  d.facing = f.Enum<SetPieceFacingKind>("kind");
  switch (d.facing) {
    case SetPieceFacingKind::Yaw:
    case SetPieceFacingKind::Front: d.facingDeg = f.Num("deg"); break;
    case SetPieceFacingKind::Tile: d.toward = Vec2(f.Num("col"), f.Num("row")); break;
    case SetPieceFacingKind::RingCentre:
      if (!d.hasRing) f.Error("ringCentre facing needs a ring");
      break;
    case SetPieceFacingKind::Camera:
    case SetPieceFacingKind::Anchor: break;
  }
  d.turnDeg = p.Num("turnDeg", 0.0);
  d.scale = p.Num("scale", 1.0);
  if (!(d.scale > 0)) p.Child("scale").Error("scale must be > 0");
  d.partners = p.StrList("partners", true);
  for (const JNode& e : p.Items("effects", true)) {
    SetPieceEffectDef fx{e.Str("socket"), e.Str("recipe")};
    if (fx.socket.empty() || fx.recipe.empty()) e.Error("an effect needs a socket and a recipe");
    d.effects.push_back(std::move(fx));
  }
  for (const JNode& e : p.Items("emitters", true)) {
    SetPieceEmitterDef em{e.Str("socket"), e.Str("emitter")};
    if (em.socket.empty() || em.emitter.empty()) e.Error("an emitter needs a socket and an emitter id");
    d.emitters.push_back(std::move(em));
  }
  for (const JNode& c : p.Items("cues", true)) {
    SetPieceCueDef cue;
    cue.cues = c.StrList("cues");
    cue.socket = c.Str("socket");
    cue.recipe = c.Str("recipe");
    if (cue.cues.empty() || cue.socket.empty() || cue.recipe.empty()) c.Error("a cue entry needs cues, a socket and a recipe");
    d.cues.push_back(std::move(cue));
  }
  d.echoes = p.StrList("echoes", true);
  d.echoTurnDeg = p.Num("echoTurnDeg", 0.0);
  d.occluder = p.Bool("occluder", false);
  d.walkOver = p.Str("walkOver", "");
  d.yieldsToDefendTarget = p.Bool("yieldsToDefendTarget", false);
  d.note = p.Str("note", "");
  return d;
}

}  // namespace

void LoadSetPiecesFile(const JNode& r, SetPieceTables& out) {
  out = SetPieceTables{};
  out.loaded = true;
  for (const auto& [zone, zn] : r.Members("zones")) {
    SetPieceZoneDef z;
    z.zone = zone;
    std::set<std::string> ids;
    for (const JNode& pn : zn.Items("pieces")) {
      SetPieceDef d = SpReadPiece(pn);
      if (!ids.insert(d.id).second) pn.Error("duplicate set-piece id '" + d.id + "'");
      z.pieces.push_back(std::move(d));
    }
    out.zones.push_back(std::move(z));
  }
}

}  // namespace dataload
}  // namespace abyss
