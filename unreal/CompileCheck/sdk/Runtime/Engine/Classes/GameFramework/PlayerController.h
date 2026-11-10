// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/GameFramework/PlayerController.h (UE 5.x source
// knowledge; UPROPERTY / UFUNCTION-marked members: checked against the 5.6 stub). Also FInputModeDataBase and its
// three modes, which live in this header.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"
#include "Engine/EngineTypes.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "Widgets/SWidget.h"
#include "GenericPlatform/ICursor.h"
#include "Camera/PlayerCameraManager.h"

class APlayerCameraManager;
class AHUD;
class UPlayer;
class ULocalPlayer;
class UInputComponent;
class UPlayerInput;
class UGameViewportClient;
class SWidget;
DECLARE_DELEGATE_RetVal(bool, FCanUnpause);


struct FInputModeDataBase
{
protected:
	virtual ~FInputModeDataBase() {}
	virtual void ApplyInputMode(class FReply& SlateOperations, class UGameViewportClient& GameViewportClient) const = 0;
	friend class APlayerController;
};
struct FInputModeUIOnly : public FInputModeDataBase
{
	FInputModeUIOnly& SetWidgetToFocus(TSharedPtr<SWidget> InWidgetToFocus);
	FInputModeUIOnly& SetLockMouseToViewportBehavior(EMouseLockMode InMouseLockMode);
	FInputModeUIOnly();
protected:
	virtual void ApplyInputMode(FReply& SlateOperations, class UGameViewportClient& GameViewportClient) const override;
};
struct FInputModeGameAndUI : public FInputModeDataBase
{
	FInputModeGameAndUI& SetWidgetToFocus(TSharedPtr<SWidget> InWidgetToFocus);
	FInputModeGameAndUI& SetLockMouseToViewportBehavior(EMouseLockMode InMouseLockMode);
	FInputModeGameAndUI& SetHideCursorDuringCapture(bool InHideCursorDuringCapture);
	FInputModeGameAndUI();
protected:
	virtual void ApplyInputMode(FReply& SlateOperations, class UGameViewportClient& GameViewportClient) const override;
};
struct FInputModeGameOnly : public FInputModeDataBase
{
	FInputModeGameOnly& SetConsumeCaptureMouseDown(bool InConsumeCaptureMouseDown);
	FInputModeGameOnly();
protected:
	virtual void ApplyInputMode(FReply& SlateOperations, class UGameViewportClient& GameViewportClient) const override;
};

class APlayerController : public AController
{
	UE_STUB_CLASS_BODY(APlayerController, AController)
public:
	UPROPERTY()
	TObjectPtr<UPlayer> Player;
	UPROPERTY(BlueprintReadOnly, Category = PlayerController)
	TObjectPtr<APlayerCameraManager> PlayerCameraManager;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = PlayerController)
	TSubclassOf<APlayerCameraManager> PlayerCameraManagerClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerController)
	bool bAutoManageActiveCameraTarget;
	UPROPERTY(Transient)
	TObjectPtr<UPlayerInput> PlayerInput;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MouseInterface)
	uint32 bShowMouseCursor : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MouseInterface)
	uint32 bEnableClickEvents : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MouseInterface)
	uint32 bEnableTouchEvents : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MouseInterface)
	uint32 bEnableMouseOverEvents : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MouseInterface)
	uint32 bEnableTouchOverEvents : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MouseInterface)
	TEnumAsByte<EMouseCursor::Type> DefaultMouseCursor;
	UPROPERTY(BlueprintReadWrite, Category = MouseInterface)
	TEnumAsByte<EMouseCursor::Type> CurrentMouseCursor;
	UPROPERTY(BlueprintReadWrite, Category = MouseInterface)
	float HitResultTraceDistance;

	ULocalPlayer* GetLocalPlayer() const;
	UFUNCTION(BlueprintCallable, Category = "Game|Player")
	AHUD* GetHUD() const;
	template <class T> T* GetHUD() const { return Cast<T>(GetHUD()); }
	virtual void SetViewTarget(class AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams());
	UFUNCTION(BlueprintCallable, Category = "Game|Player", meta = (Keywords = "Camera"))
	virtual void SetViewTargetWithBlend(class AActor* NewViewTarget, float BlendTime = 0, enum EViewTargetBlendFunction BlendFunc = VTBlend_Linear, float BlendExp = 0, bool bLockOutgoing = false);
	virtual void SetInputMode(const FInputModeDataBase& InData);
	UFUNCTION(BlueprintCallable, Category = "Game|Player", meta = (DisplayName = "Convert Screen Location To World Space", Keywords = "deproject"))
	bool DeprojectScreenPositionToWorld(float ScreenX, float ScreenY, FVector& WorldLocation, FVector& WorldDirection) const;
	UFUNCTION(BlueprintCallable, Category = "Game|Player", meta = (DisplayName = "Convert Mouse Location To World Space", Keywords = "deproject"))
	bool DeprojectMousePositionToWorld(FVector& WorldLocation, FVector& WorldDirection) const;
	UFUNCTION(BlueprintCallable, Category = "Game|Player", meta = (DisplayName = "Convert World Location To Screen Location", Keywords = "project"))
	bool ProjectWorldLocationToScreen(FVector WorldLocation, FVector2D& ScreenLocation, bool bPlayerViewportRelative = false) const;
	bool ProjectWorldLocationToScreenWithDistance(FVector WorldLocation, FVector& ScreenLocation, bool bPlayerViewportRelative = false) const;
	UFUNCTION(BlueprintCallable, Category = "Game|Player", meta = (DisplayName = "Get Hit Result Under Cursor By Channel", TraceChannel = ECC_Visibility, bTraceComplex = true))
	bool GetHitResultUnderCursorByChannel(ETraceTypeQuery TraceChannel, bool bTraceComplex, FHitResult& HitResult) const;
	bool GetHitResultUnderCursor(ECollisionChannel TraceChannel, bool bTraceComplex, FHitResult& HitResult) const;
	bool GetHitResultAtScreenPosition(const FVector2D ScreenPosition, const ECollisionChannel TraceChannel, bool bTraceComplex, FHitResult& HitResult) const;
	UFUNCTION(BlueprintCallable, Category = "Game|Player")
	bool GetMousePosition(float& LocationX, float& LocationY) const;
	bool GetMousePosition(double& LocationX, double& LocationY) const;
	/** Screen position and pressed state of one touch (the float form is the reflected one; the double form is native-only). */
	UFUNCTION(BlueprintCallable, Category = "Game|Player")
	void GetInputTouchState(ETouchIndex::Type FingerIndex, float& LocationX, float& LocationY, bool& bIsCurrentlyPressed) const;
	void GetInputTouchState(ETouchIndex::Type FingerIndex, double& LocationX, double& LocationY, bool& bIsCurrentlyPressed) const;
	UFUNCTION(BlueprintCallable, Category = "Game|Player")
	void GetViewportSize(int32& SizeX, int32& SizeY) const;
	UFUNCTION(BlueprintCallable, Category = "Game|Player")
	bool IsInputKeyDown(FKey Key) const;
	UFUNCTION(BlueprintCallable, Category = "Game|Player")
	bool WasInputKeyJustPressed(FKey Key) const;
	UFUNCTION(BlueprintCallable, Category = "Game|Player")
	bool WasInputKeyJustReleased(FKey Key) const;
	UFUNCTION(BlueprintCallable, Category = "Game|Player")
	float GetInputAnalogKeyState(FKey Key) const;
	UFUNCTION(BlueprintCallable, Category = "Game|Player")
	void GetInputMouseDelta(float& DeltaX, float& DeltaY) const;
	void GetInputMouseDelta(double& DeltaX, double& DeltaY) const;
	UFUNCTION(BlueprintCallable, Category = "Game|Player")
	void SetMouseLocation(const int X, const int Y);
	virtual bool SetPause(bool bPause, FCanUnpause CanUnpauseDelegate = FCanUnpause());
	virtual bool IsPaused() const;
	virtual void FlushPressedKeys();
	virtual void UpdateRotation(float DeltaTime);
	virtual void PlayerTick(float DeltaTime);
	virtual void PreProcessInput(const float DeltaTime, const bool bGamePaused);
	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused);
	virtual bool InputKey(const struct FInputKeyEventArgs& Params);
	virtual bool InputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, float Force, FDateTime DeviceTimestamp, uint32 TouchpadIndex);
	FString ConsoleCommand(const FString& Command, bool bWriteToLog = true);
	virtual void GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const;
	virtual bool ShouldShowMouseCursor() const;
	virtual EMouseCursor::Type GetMouseCursor() const;
	void SetShowMouseCursor(bool bShow);
	virtual void PushInputComponent(UInputComponent* Input);
	virtual bool PopInputComponent(UInputComponent* Input);
	virtual void ReceivedPlayer();
	virtual void SetPawn(APawn* InPawn);
protected:
	virtual void SetupInputComponent();
	virtual void BeginPlayingState();
	virtual void OnPossess(APawn* aPawn) override;
};
