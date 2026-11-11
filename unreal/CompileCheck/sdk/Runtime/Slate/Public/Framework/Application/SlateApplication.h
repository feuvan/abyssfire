// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Framework/Application/SlateApplication.h (UE 5.x source
// knowledge). FSlateApplication: Get() is only valid when IsInitialized() (dedicated servers / commandlets have none).
#pragma once

#include "CoreMinimal.h"
#include "Application/SlateApplicationBase.h"
#include "Framework/Application/IInputProcessor.h"
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/ICursor.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Layout/WidgetPath.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"

class SViewport;
class FSlateUser;
class FNavigationConfig;
class GenericApplication;

class FSlateApplication : public FSlateApplicationBase
{
public:
	static bool IsInitialized();
	static FSlateApplication& Get();
	static void Create();
	static void Shutdown(bool bShutdownPlatform = true);

	void PumpMessages();
	bool IsFakingTouchEvents() const;
	void SetGameIsFakingTouchEvents(const bool bIsFaking, FVector2D* CursorLocation = nullptr);
	TSharedPtr<GenericApplication> GetPlatformApplication() const;
	TSharedPtr<ICursor> GetPlatformCursor();
	FModifierKeysState GetModifierKeys() const;

	bool RegisterInputPreProcessor(TSharedPtr<IInputProcessor> InputProcessor, const int32 Index = INDEX_NONE);
	void UnregisterInputPreProcessor(TSharedPtr<IInputProcessor> InputProcessor);
	void UnregisterAllInputPreProcessors();
	int32 FindInputPreProcessor(TSharedPtr<IInputProcessor> InputProcessor) const;

	void RegisterGameViewport(TSharedRef<SViewport> InViewport);
	void UnregisterGameViewport();
	TSharedPtr<SViewport> GetGameViewport() const;
	void SetAllUserFocusToGameViewport(EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);
	void SetUserFocusToGameViewport(uint32 UserIndex, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);
	void ActivateGameViewport();
	void ResetToDefaultInputSettings();
	void ResetToDefaultPointerInputSettings();

	bool SetKeyboardFocus(const TSharedPtr<SWidget>& OptionalWidgetToFocus, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);
	bool SetKeyboardFocus(const FWidgetPath& InFocusPath, const EFocusCause InCause);
	void ClearKeyboardFocus(const EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);
	bool SetUserFocus(uint32 UserIndex, const TSharedPtr<SWidget>& WidgetToFocus, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);
	void SetAllUserFocus(const TSharedPtr<SWidget>& WidgetToFocus, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);
	void ClearUserFocus(uint32 UserIndex, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);
	void ClearAllUserFocus(EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);
	TSharedPtr<SWidget> GetUserFocusedWidget(uint32 UserIndex) const;
	virtual TSharedPtr<SWidget> GetKeyboardFocusedWidget() const override;
	void ReleaseAllPointerCapture();
	void ReleaseAllPointerCapture(int32 UserIndex);
	TSharedPtr<FSlateUser> GetUser(int32 UserIndex) const;
	TSharedPtr<FSlateUser> GetCursorUser() const;

	virtual UE::Slate::FDeprecateVector2DResult GetCursorPos() const override;
	virtual UE::Slate::FDeprecateVector2DResult GetLastCursorPos() const override;
	virtual UE::Slate::FDeprecateVector2DResult GetCursorSize() const override;
	void SetCursorPos(const FVector2D& MouseCoordinate);
	float GetCursorRadius() const;
	/** The distance (in Slate units, already DPI scaled) the pointer must move before a press becomes a drag. */
	float GetDragTriggerDistance() const;
	void SetDragTriggerDistance(float ScreenPixels);
	bool IsMouseAttached() const;
	bool IsGamepadAttached() const;
	const TSet<FKey>& GetPressedMouseButtons() const;
	float GetDeltaTime() const;
	virtual const double GetCurrentTime() const override;
	double GetLastUserInteractionTime() const;
	float GetAverageDeltaTime() const;
	virtual float GetApplicationScale() const override;
	void SetApplicationScale(float InScale);
	virtual bool IsActive() const override;
	bool IsDragDropping() const;
	bool AnyMenusVisible() const;

	virtual TSharedRef<SWindow> AddWindow(TSharedRef<SWindow> InSlateWindow, const bool bShowImmediately = true) override;
	virtual TSharedPtr<SWindow> GetActiveTopLevelWindow() const override;
	virtual TSharedPtr<SWindow> GetActiveTopLevelRegularWindow() const override;
	virtual const TArray<TSharedRef<SWindow>> GetTopLevelWindows() const override;
	TArray<TSharedRef<SWindow>> GetInteractiveTopLevelWindows();
	void GetAllVisibleWindowsOrdered(TArray<TSharedRef<SWindow>>& OutWindows);
	TSharedPtr<SWindow> FindWidgetWindow(TSharedRef<const SWidget> InWidget) const;
	TSharedPtr<SWindow> FindWidgetWindow(TSharedRef<const SWidget> InWidget, FWidgetPath& OutWidgetPath) const;
	bool GeneratePathToWidgetUnchecked(TSharedRef<SWidget> InWidget, FWidgetPath& OutWidgetPath, EVisibility VisibilityFilter = EVisibility::Visible) const;
	bool FindPathToWidget(TSharedRef<const SWidget> InWidget, FWidgetPath& OutWidgetPath, EVisibility VisibilityFilter = EVisibility::Visible);
	virtual FWidgetPath LocateWindowUnderMouse(UE::Slate::FDeprecateVector2DParameter ScreenspaceMouseCoordinate, const TArray<TSharedRef<SWindow>>& Windows, bool bIgnoreEnabledStatus = false, int32 UserIndex = INDEX_NONE) override;
	FWidgetPath LocateWidgetInWindow(UE::Slate::FDeprecateVector2DParameter ScreenspaceMouseCoordinate, const TSharedRef<SWindow>& Window, bool bIgnoreEnabledStatus, int32 UserIndex) const;
	virtual bool HasAnyMouseCaptor() const override;
	virtual bool HasUserMouseCapture(int32 UserIndex) const override;
	virtual FSlateRect GetPreferredWorkArea() const override;
	virtual bool GetSoftwareCursorAvailable() const override;
	virtual EVisibility GetSoftwareCursorVis() const override;
	virtual void RequestDestroyWindow(TSharedRef<SWindow> WindowToDestroy) override;
	virtual bool IsExternalUIOpened() override;
	virtual bool IsWindowHousingInteractiveTooltip(const TSharedRef<const SWindow>& WindowToTest) const override;
	virtual TSharedRef<SWidget> MakeImage(const TAttribute<const FSlateBrush*>& Image, const TAttribute<FSlateColor>& Color, const TAttribute<EVisibility>& Visibility) const override;

	bool ProcessKeyDownEvent(const FKeyEvent& InKeyEvent);
	bool ProcessKeyUpEvent(const FKeyEvent& InKeyEvent);
	bool ProcessMouseButtonDownEvent(const TSharedPtr<class FGenericWindow>& PlatformWindow, const FPointerEvent& InMouseEvent);
	bool ProcessMouseButtonUpEvent(const FPointerEvent& MouseEvent);
	bool ProcessMouseMoveEvent(const FPointerEvent& MouseEvent, bool bIsSynthetic = false);
	void SetNavigationConfig(TSharedRef<FNavigationConfig> InNavigationConfig);
	TSharedRef<FNavigationConfig> GetNavigationConfig() const;
	void PlaySound(const struct FSlateSound& SoundToPlay, int32 UserIndex = 0) const;
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnApplicationActivationStateChanged, const bool);
	FOnApplicationActivationStateChanged& OnApplicationActivationStateChanged();
	DECLARE_MULTICAST_DELEGATE_OneParam(FSlateTickEvent, float);
	FSlateTickEvent& OnPreTick();
	FSlateTickEvent& OnPostTick();
};
