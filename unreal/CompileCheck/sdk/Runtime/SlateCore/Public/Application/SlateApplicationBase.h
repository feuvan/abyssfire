// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Application/SlateApplicationBase.h (UE 5.x source
// knowledge). The part of the Slate application SlateCore knows: renderer, cursor position, time, top-level windows.
#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "Layout/Visibility.h"
#include "Types/SlateVector2.h"
#include "Widgets/SWidget.h"

class FSlateRenderer;
class FWidgetPath;
class SWindow;
class FActiveTimerHandle;
struct FDisplayMetrics;

class FSlateApplicationBase
{
public:
	FSlateApplicationBase();
	virtual ~FSlateApplicationBase() {}
	const FSlateRenderer* GetRenderer() const { return Renderer.Get(); }
	FSlateRenderer* GetRenderer() { return Renderer.Get(); }
	virtual TSharedRef<SWindow> AddWindow(TSharedRef<SWindow> InSlateWindow, const bool bShowImmediately = true) = 0;
	virtual TSharedPtr<SWindow> GetActiveTopLevelWindow() const = 0;
	virtual TSharedPtr<SWindow> GetActiveTopLevelRegularWindow() const = 0;
	virtual const TArray<TSharedRef<SWindow>> GetTopLevelWindows() const = 0;
	virtual bool HasAnyMouseCaptor() const = 0;
	virtual bool HasUserMouseCapture(int32 UserIndex) const = 0;
	virtual FSlateRect GetPreferredWorkArea() const = 0;
	virtual UE::Slate::FDeprecateVector2DResult GetCursorPos() const = 0;
	virtual UE::Slate::FDeprecateVector2DResult GetLastCursorPos() const = 0;
	virtual UE::Slate::FDeprecateVector2DResult GetCursorSize() const = 0;
	virtual bool GetSoftwareCursorAvailable() const = 0;
	virtual EVisibility GetSoftwareCursorVis() const = 0;
	virtual TSharedPtr<SWidget> GetKeyboardFocusedWidget() const = 0;
	virtual const double GetCurrentTime() const = 0;
	virtual void RequestDestroyWindow(TSharedRef<SWindow> WindowToDestroy) = 0;
	virtual bool IsExternalUIOpened() = 0;
	virtual FWidgetPath LocateWindowUnderMouse(UE::Slate::FDeprecateVector2DParameter ScreenspaceMouseCoordinate, const TArray<TSharedRef<SWindow>>& Windows, bool bIgnoreEnabledStatus = false, int32 UserIndex = INDEX_NONE) = 0;
	virtual bool IsWindowHousingInteractiveTooltip(const TSharedRef<const SWindow>& WindowToTest) const = 0;
	virtual TSharedRef<SWidget> MakeImage(const TAttribute<const FSlateBrush*>& Image, const TAttribute<FSlateColor>& Color, const TAttribute<EVisibility>& Visibility) const = 0;
	virtual float GetApplicationScale() const = 0;
	virtual bool IsActive() const = 0;
	void GetDisplayMetrics(FDisplayMetrics& OutDisplayMetrics);
	void GetCachedDisplayMetrics(FDisplayMetrics& OutDisplayMetrics) const;
	void GetSafeZoneSize(FMargin& SafeZone, const UE::Slate::FDeprecateVector2DParameter& OverrideSize);
	static bool IsInitialized();
	static FSlateApplicationBase& Get();
	bool IsSlateAsleep();
protected:
	TSharedPtr<FSlateRenderer> Renderer;
};
