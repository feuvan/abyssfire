// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/Axis.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

namespace EAxis { enum Type : int { None, X, Y, Z }; }
namespace EAxisList { enum Type { None = 0, X = 1, Y = 2, Z = 4, Screen = 8, XY = X | Y, XZ = X | Z, YZ = Y | Z, XYZ = X | Y | Z, All = XYZ | Screen, ZRotation = YZ, Rotate2D = Screen }; }
