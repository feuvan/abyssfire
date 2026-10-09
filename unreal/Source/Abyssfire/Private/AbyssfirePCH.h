// Private precompiled header of the Abyssfire module (Abyssfire.Build.cs: a module that overrides FPSemantics needs a
// private PCH). Only for build speed: every file still includes what it uses (IWYU).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// The core's facade pulls in most of the public core headers (Commands, Events, Snapshot, SaveIO, ...).
#include "abyss/sim/GameSim.h"
