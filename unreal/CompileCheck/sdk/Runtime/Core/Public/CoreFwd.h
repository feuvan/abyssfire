// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/CoreFwd.h (from UE 5.x source knowledge).
// Forward declarations of the Core types (the real file forwards to Containers/ContainersFwd.h, Math/MathFwd.h and
// UObject/UObjectHierarchyFwd.h and declares the rest here).
#pragma once

#include "CoreTypes.h"
#include "Containers/ContainersFwd.h"
#include "Math/MathFwd.h"
#include "UObject/UObjectHierarchyFwd.h"

class FArchive;
class FOutputDevice;
class FFeedbackContext;
class FName;
class FText;
struct FDateTime;
struct FGuid;
struct FTimespan;
class FExec;
class IFileManager;
class IPlatformFile;
class IFileHandle;
class FConfigCacheIni;
class FRunnable;
class FRunnableThread;
class FEvent;
class FDelegateHandle;
template <typename FuncType> class TFunction;
template <typename FuncType> class TFunctionRef;
template <typename FuncType> class TUniqueFunction;
