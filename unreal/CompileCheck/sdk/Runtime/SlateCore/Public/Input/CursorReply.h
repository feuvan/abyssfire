// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Input/CursorReply.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "GenericPlatform/ICursor.h"

class FCursorReply : public TReplyBase<FCursorReply>
{
public:
	static FCursorReply Unhandled() { return FCursorReply(); }
	static FCursorReply Cursor(EMouseCursor::Type InCursor) { return FCursorReply(InCursor); }
	EMouseCursor::Type GetCursorType() const;
private:
	FCursorReply() : TReplyBase<FCursorReply>(false) {}
	FCursorReply(EMouseCursor::Type InCursorType) : TReplyBase<FCursorReply>(true) {}
};
