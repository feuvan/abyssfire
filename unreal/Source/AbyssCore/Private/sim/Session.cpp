// Session helpers and the difficulty ladder (save-ui-input.md 1.2, 2; combat-feel.md 14). Implemented.
#include "abyss/base/Platform.h"

#include "abyss/sim/Session.h"

#include <algorithm>

namespace abyss {

namespace {
bool Has(std::span<const Difficulty> list, Difficulty d) { return std::find(list.begin(), list.end(), d) != list.end(); }
}  // namespace

DifficultyStates GetDifficultyStates(std::span<const Difficulty> completed) {
  DifficultyStates out;
  for (size_t i = 0; i < EnumCount<Difficulty>(); ++i) {
    const Difficulty d = static_cast<Difficulty>(i);
    if (Has(completed, d)) {
      out.states[i] = DifficultyState::Completed;
    } else if (d == Difficulty::Normal || Has(completed, static_cast<Difficulty>(i - 1))) {
      out.states[i] = DifficultyState::Available;
    } else {
      out.states[i] = DifficultyState::Locked;
    }
  }
  if (Has(completed, Difficulty::Normal) && !Has(completed, Difficulty::Nightmare)) {
    out.hasNextUnlocked = true;
    out.nextUnlocked = Difficulty::Nightmare;
  } else if (Has(completed, Difficulty::Nightmare) && !Has(completed, Difficulty::Hell)) {
    out.hasNextUnlocked = true;
    out.nextUnlocked = Difficulty::Hell;
  }
  return out;
}

std::vector<Difficulty> DeriveCompletedDifficulties(Difficulty current, std::span<const Difficulty> completed) {
  if (!completed.empty()) return std::vector<Difficulty>(completed.begin(), completed.end());
  if (current == Difficulty::Nightmare) return {Difficulty::Normal};
  if (current == Difficulty::Hell) return {Difficulty::Normal, Difficulty::Nightmare};
  return {};
}

bool ShouldShowDifficultySelector(Difficulty current, std::span<const Difficulty> completed) {
  return current != Difficulty::Normal || !completed.empty();
}

bool PanelState::IsOpen(PanelId p) const { return std::find(open.begin(), open.end(), p) != open.end(); }

bool PanelState::AnyModalOpen() const {
  return std::any_of(open.begin(), open.end(), [](PanelId p) { return IsModalPanel(p) || p == PanelId::SystemMenu; });
}

bool PanelState::AnyHudPanelOpen() const {
  return std::any_of(open.begin(), open.end(), [](PanelId p) { return !IsModalPanel(p) && p != PanelId::SystemMenu; });
}

bool SessionState::HasVisited(std::string_view mapId) const {
  return std::find(visitedZones.begin(), visitedZones.end(), mapId) != visitedZones.end();
}

}  // namespace abyss
