// SaveData sections owned by hero+combat: player, settings, soulEcho (save-ui-input.md 3.2). STUB.
#include "abyss/base/Platform.h"

#include "abyss/base/Assert.h"
#include "SaveSections.h"

namespace abyss::savejson {

void WriteHero(JsonWriter& w, const SaveHero& h) {
  ABYSS_UNIMPLEMENTED();
  w.StartObject();
  w.EndObject();
}

void ReadHero(const JsonValue& v, SaveHero& out) { ABYSS_UNIMPLEMENTED(); }

void WriteSettings(JsonWriter& w, const SaveSettings& s) {
  ABYSS_UNIMPLEMENTED();
  w.StartObject();
  w.EndObject();
}

void ReadSettings(const JsonValue& v, SaveSettings& out) { ABYSS_UNIMPLEMENTED(); }

void WriteSoulEcho(JsonWriter& w, const SaveSoulEcho& e) {
  ABYSS_UNIMPLEMENTED();
  w.Null();
}

void ReadSoulEcho(const JsonValue& v, SaveSoulEcho& out) { ABYSS_UNIMPLEMENTED(); }

}  // namespace abyss::savejson
