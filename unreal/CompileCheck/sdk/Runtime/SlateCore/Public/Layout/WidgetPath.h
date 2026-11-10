// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Layout/WidgetPath.h (UE 5.x source knowledge).
// FWidgetPath: window + widgets from the window down to the leaf under a point (Widgets, an FArrangedChildren).
#pragma once

#include "CoreMinimal.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/Visibility.h"

class SWidget;
class SWindow;

class FWidgetPath
{
public:
	FWidgetPath();
	FWidgetPath(TSharedPtr<SWindow> InTopLevelWindow, const FArrangedChildren& InWidgetPath);
	FWidgetPath(TArrayView<FWidgetAndPointer> InWidgetsAndPointers);
	FWidgetPath GetPathDownTo(TSharedRef<const SWidget> MarkerWidget) const;
	bool ContainsWidget(const SWidget* WidgetToFind) const;
	TOptional<FArrangedWidget> FindArrangedWidget(TSharedRef<const SWidget> WidgetToFind) const;
	TSharedRef<SWindow> GetWindow() const;
	TSharedRef<SWindow> GetDeepestWindow() const;
	bool IsValid() const;
	FString ToString() const;
	TSharedRef<SWidget> GetLastWidget() const;
	FArrangedChildren Widgets;
	TSharedPtr<SWindow> TopLevelWindow;
};
class FWeakWidgetPath
{
public:
	FWeakWidgetPath(const FWidgetPath& InWidgetPath = FWidgetPath());
	FWidgetPath ToWidgetPath() const;
	bool ContainsWidget(const SWidget* WidgetToFind) const;
	bool IsValid() const;
	TWeakPtr<SWidget> GetLastWidget() const;
	TArray<TWeakPtr<SWidget>> Widgets;
	TWeakPtr<SWindow> Window;
};
