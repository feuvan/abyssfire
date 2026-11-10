// Stub SDK. Real header: Engine/Source/Runtime/ApplicationCore/Public/GenericPlatform/ICursor.h (UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Math/Vector2D.h"
#include "Math/IntRect.h"
#include "Math/Color.h"

namespace EMouseCursor
{
	enum Type : int
	{
		None,
		Default,
		TextEditBeam,
		ResizeLeftRight,
		ResizeUpDown,
		ResizeSouthEast,
		ResizeSouthWest,
		CardinalCross,
		Crosshairs,
		Hand,
		GrabHand,
		GrabHandClosed,
		SlashedCircle,
		EyeDropper,
		Custom,
		TotalCursorCount,
	};
}

class ICursor
{
public:
	virtual ~ICursor() {}
	virtual FVector2D GetPosition() const = 0;
	virtual void SetPosition(const int32 X, const int32 Y) = 0;
	virtual void SetType(const EMouseCursor::Type InNewCursor) = 0;
	virtual EMouseCursor::Type GetType() const = 0;
	virtual void GetSize(int32& Width, int32& Height) const = 0;
	virtual void Show(bool bShow) = 0;
	virtual void SetTypeShape(EMouseCursor::Type InCursorType, void* CursorHandle) = 0;
};
