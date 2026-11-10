// Stub SDK. Real header: Engine/Source/Runtime/ApplicationCore/Public/GenericPlatform/GenericApplication.h
// (UE 5.x source knowledge). FModifierKeysState, FDisplayMetrics, FPlatformRect.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "Templates/SharedPointer.h"
#include "GenericPlatform/ICursor.h"

class FGenericWindow;

struct FPlatformRect
{
	int32 Left;
	int32 Top;
	int32 Right;
	int32 Bottom;
	FPlatformRect() : Left(0), Top(0), Right(0), Bottom(0) {}
	FPlatformRect(int32 InLeft, int32 InTop, int32 InRight, int32 InBottom) : Left(InLeft), Top(InTop), Right(InRight), Bottom(InBottom) {}
	bool operator==(const FPlatformRect& Other) const = default;
};

class FModifierKeysState
{
public:
	FModifierKeysState(const bool bInIsLeftShiftDown, const bool bInIsRightShiftDown, const bool bInIsLeftControlDown, const bool bInIsRightControlDown,
		const bool bInIsLeftAltDown, const bool bInIsRightAltDown, const bool bInIsLeftCommandDown, const bool bInIsRightCommandDown, const bool bInAreCapsLocked);
	FModifierKeysState();
	bool IsShiftDown() const;
	bool IsLeftShiftDown() const;
	bool IsRightShiftDown() const;
	bool IsControlDown() const;
	bool IsLeftControlDown() const;
	bool IsRightControlDown() const;
	bool IsAltDown() const;
	bool IsLeftAltDown() const;
	bool IsRightAltDown() const;
	bool IsCommandDown() const;
	bool AreCapsLocked() const;
	bool AnyModifiersDown() const;
};

struct FDisplayMetrics
{
	int32 PrimaryDisplayWidth;
	int32 PrimaryDisplayHeight;
	FPlatformRect PrimaryDisplayWorkAreaRect;
	FPlatformRect VirtualDisplayRect;
	FVector4 TitleSafePaddingSize;
	FVector4 ActionSafePaddingSize;
	static void RebuildDisplayMetrics(struct FDisplayMetrics& OutDisplayMetrics);
};
