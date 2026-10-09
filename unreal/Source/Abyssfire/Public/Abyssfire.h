// Abyssfire UE module: log categories shared by every file of the module.
#pragma once

#include "CoreMinimal.h"

// The UE presentation layer.
ABYSSFIRE_API DECLARE_LOG_CATEGORY_EXTERN(LogAbyss, Log, All);
// Messages routed from AbyssCore's log sink and assert handler (abyss/base/Log.h, Assert.h).
ABYSSFIRE_API DECLARE_LOG_CATEGORY_EXTERN(LogAbyssCore, Log, All);
