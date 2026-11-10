// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Widgets/SWindow.h (UE 5.x source knowledge).
// A top-level Slate window (only the queries a game normally makes; windows are created by the engine).
#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "Widgets/SCompoundWidget.h"


class SWindow : public SCompoundWidget
{
public:
	UE::Slate::FDeprecateVector2DResult GetPositionInScreen() const;
	UE::Slate::FDeprecateVector2DResult GetSizeInScreen() const;
	UE::Slate::FDeprecateVector2DResult GetClientSizeInScreen() const;
	UE::Slate::FDeprecateVector2DResult GetViewportSize() const;
	FSlateRect GetRectInScreen() const;
	FSlateRect GetClientRectInScreen() const;
	FGeometry GetWindowGeometryInScreen() const;
	FGeometry GetWindowGeometryInWindow() const;
	float GetDPIScaleFactor() const;
	FText GetTitle() const;
	bool IsActive() const;
	bool IsVisible() const;
	bool IsWindowMinimized() const;
	bool IsWindowMaximized() const;
	bool IsRegularWindow() const;
	bool IsTopmostWindow() const;
	TSharedRef<const SWidget> GetContent() const;
	void SetContent(TSharedRef<SWidget> InContent);
	TSharedPtr<class FGenericWindow> GetNativeWindow();
	void BringToFront(bool bForce = false);
	void RequestDestroyWindow();
	void Minimize();
	void Maximize();
	void Restore();
	void ShowWindow();
	void HideWindow();
};
