// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Layout/ArrangedChildren.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Layout/ArrangedWidget.h"
#include "Layout/Visibility.h"

class FArrangedChildren
{
public:
	typedef TArray<FArrangedWidget, TInlineAllocator<4>> FArrangedWidgetArray;
	FArrangedChildren(EVisibility VisibilityFilter, bool bInAllow3DWidgets = false);
	static FArrangedChildren Hittest2_FromArray(const TArray<struct FWidgetAndPointer>& InWidgets);
	void AddWidget(const FArrangedWidget& InWidgetGeometry);
	void AddWidget(EVisibility VisibilityOverride, const FArrangedWidget& InWidgetGeometry);
	bool Accepts(EVisibility InVisibility) const;
	bool Allows3DWidgets() const;
	void Empty(int32 Slack = 0);
	int32 Num() const;
	const FArrangedWidget& operator[](int32 Index) const;
	FArrangedWidget& operator[](int32 Index);
	const FArrangedWidget& Last() const;
	FArrangedWidget& Last();
	FArrangedWidgetArray& GetInternalArray();
	const FArrangedWidgetArray& GetInternalArray() const;
	int32 IndexOfByPredicate(TFunctionRef<bool(const FArrangedWidget&)> Pred) const;
	void Remove(int32 Index, int32 Count = 1);
	void Append(const FArrangedChildren& Other);
	void Reverse();
};
