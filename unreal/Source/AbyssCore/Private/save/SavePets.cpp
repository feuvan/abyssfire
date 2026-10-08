// SaveData sections owned by pets / homestead (save-ui-input.md 3.2-3.3; quests-story-ch1.md 18.2). STUB.
#include "abyss/base/Platform.h"

#include "abyss/base/Assert.h"
#include "abyss/save/SaveIO.h"
#include "SaveSections.h"

namespace abyss {

void WritePetJson(JsonWriter& w, const PetInstance& p) {
  ABYSS_UNIMPLEMENTED();
  w.StartObject();
  w.EndObject();
}

bool ReadPetJson(const JsonValue& v, PetInstance& out) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

namespace savejson {

void WritePets(JsonWriter& w, const SavePets& p) {
  ABYSS_UNIMPLEMENTED();
  w.StartObject();
  w.EndObject();
}

void ReadPets(const JsonValue& v, SavePets& out) { ABYSS_UNIMPLEMENTED(); }

void WriteHomestead(JsonWriter& w, const SaveHomestead& h) {
  ABYSS_UNIMPLEMENTED();
  w.StartObject();
  w.EndObject();
}

void ReadHomestead(const JsonValue& v, SaveHomestead& out) { ABYSS_UNIMPLEMENTED(); }

}  // namespace savejson
}  // namespace abyss
