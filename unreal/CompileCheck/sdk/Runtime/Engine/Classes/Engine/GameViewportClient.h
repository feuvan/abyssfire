// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/GameViewportClient.h (UE 5.x source knowledge;
// reflected class: checked against the 5.6 stub). The game viewport: viewport size, the Slate layers the game adds its
// widgets to, mouse capture / lock settings.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Exec.h"
#include "Engine/ScriptViewportClient.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "Widgets/SWidget.h"
#include "Input/Reply.h"

class SWindow;
class SViewport;
class SOverlay;
class ULocalPlayer;
class UGameInstance;
class IGameLayerManager;
enum class EMouseCaptureMode : uint8;
enum class EMouseLockMode : uint8;

class UGameViewportClient : public UScriptViewportClient, public FExec
{
	UE_STUB_CLASS_BODY(UGameViewportClient, UScriptViewportClient)
public:
	FViewport* Viewport;
	virtual UWorld* GetWorld() const override;
	UGameInstance* GetGameInstance() const;
	void GetViewportSize(FVector2D& ViewportSize) const;
	bool IsFullScreenViewport() const;
	bool GetMousePosition(FVector2D& MousePosition) const;
	bool IsFocused(FViewport* Viewport) override;
	virtual void AddViewportWidgetContent(TSharedRef<class SWidget> ViewportContent, const int32 ZOrder = 0);
	virtual void RemoveViewportWidgetContent(TSharedRef<class SWidget> ViewportContent);
	virtual void AddViewportWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent, const int32 ZOrder);
	virtual void RemoveViewportWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent);
	void RemoveAllViewportWidgets();
	TSharedPtr<SWindow> GetWindow();
	TSharedPtr<SViewport> GetGameViewportWidget() const;
	TSharedPtr<IGameLayerManager> GetGameLayerManager() const;
	EMouseCaptureMode GetMouseCaptureMode() const;
	void SetMouseCaptureMode(EMouseCaptureMode Mode);
	void SetMouseLockMode(EMouseLockMode InMouseLockMode);
	void SetHideCursorDuringCapture(bool InHideCursorDuringCapture);
	bool HideCursorDuringCapture() const;
	void SetIgnoreInput(bool Ignore);
	bool IgnoreInput();
	virtual void Tick(float DeltaTime);
	virtual void LostFocus(FViewport* Viewport) override;
	virtual void ReceivedFocus(FViewport* Viewport) override;
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
};
