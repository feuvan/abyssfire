// The only UE-aware file of AbyssCore (excluded from the CMake build, ue58-platform.md 4.4).
// IMPLEMENT_MODULE also emits the per-module operator new/delete forwarding to FMemory in modular builds, so memory
// allocated inside the core (e.g. std::string buffers) can be freed safely by the game module.
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, AbyssCore);
