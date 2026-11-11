// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/UnrealClient.h (UE 5.x source knowledge).
// FViewport, FViewportClient, FScreenshotRequest.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Engine/EngineBaseTypes.h"

class FViewportClient;
class FCanvas;

struct FScreenshotRequest
{
	static void RequestScreenshot(bool bInShowUI);
	static void RequestScreenshot(const FString& InFilename, bool bInShowUI, bool bAddFilenameSuffix, bool bHdrScreenshot = false);
	static void Reset();
	static const FString& GetFilename();
	static bool IsScreenshotRequested();
	static bool ShouldShowUI();
	static void CreateViewportScreenShotFilename(FString& InOutFilename);
	static TArray<FColor>* GetHighresScreenshotMaskColorArray();
	static FIntPoint& GetHighresScreenshotMaskExtents();
};

class FRenderTarget
{
public:
	virtual ~FRenderTarget() {}
	virtual FIntPoint GetSizeXY() const = 0;
	bool ReadPixels(TArray<FColor>& OutImageData);
};

class FViewport : public FRenderTarget
{
public:
	FViewport(FViewportClient* InViewportClient);
	virtual ~FViewport();
	virtual void* GetWindow() = 0;
	virtual bool HasMouseCapture() const;
	virtual bool HasFocus() const;
	virtual bool IsForegroundWindow() const;
	virtual bool KeyState(FKey Key) const = 0;
	virtual int32 GetMouseX() const = 0;
	virtual int32 GetMouseY() const = 0;
	virtual void GetMousePos(FIntPoint& MousePosition, const bool bLocalPosition = true) = 0;
	virtual void SetMouse(int32 x, int32 y) = 0;
	virtual bool IsFullscreen() const;
	virtual FIntPoint GetSizeXY() const override;
	FIntPoint GetInitialPositionXY() const;
	FViewportClient* GetClient() const;
	bool IsPlayInEditorViewport() const;
	void Invalidate();
	void Draw(bool bShouldPresent = true);
	float GetDesiredAspectRatio() const;
};

class FViewportClient
{
public:
	virtual ~FViewportClient() {}
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) {}
	virtual UWorld* GetWorld() const { return nullptr; }
	virtual void LostFocus(FViewport* Viewport) {}
	virtual void ReceivedFocus(FViewport* Viewport) {}
	virtual bool IsFocused(FViewport* Viewport) { return true; }
};
#if !(defined(UE_STUB_UNVERIFIED_NO_STAT_UNIT_DATA) && UE_STUB_UNVERIFIED_NO_STAT_UNIT_DATA)
// UNVERIFIED (API_UNCERTAIN.md #9): what `stat unit` computes at each draw (FStatUnitData::DrawStat), believed to be
// declared in this header since 4.x with these members: the filtered values the overlay prints and the unfiltered
// ("Raw") ones of the last frame, in milliseconds; a GPU value per GPU index. Not reflected, so the 5.6 Python stub
// cannot confirm it. The module reaches every member through a hedge (AbyssAutoTestSubsystem.cpp,
// AbyssAutoTest_ReadStatUnit / AbyssAutoTest_CopyStatUnit): define the macro to type-check the fallback of an engine
// without FCommonViewportClient::GetStatUnitData().
struct FStatUnitData
{
	float RenderThreadTime;
	float GameThreadTime;
	float GPUFrameTime[8];
	float FrameTime;
	float RHITTime;
	float RawRenderThreadTime;
	float RawGameThreadTime;
	float RawGPUFrameTime[8];
	float RawFrameTime;
	float RawRHITTime;
};
// end UNVERIFIED
#endif

class FCommonViewportClient : public FViewportClient
{
public:
	virtual ~FCommonViewportClient();
	float GetDPIScale() const;
	float GetDPIDerivedResolutionFraction() const;
#if !(defined(UE_STUB_UNVERIFIED_NO_STAT_UNIT_DATA) && UE_STUB_UNVERIFIED_NO_STAT_UNIT_DATA)
	// UNVERIFIED (API_UNCERTAIN.md #9): see FStatUnitData above; IsStatEnabled takes the stat's name without "STAT_"
	// ("Unit", "FPS"), believed virtual on this class.
	const FStatUnitData* GetStatUnitData() const;
	virtual bool IsStatEnabled(const FString& InName) const;
	// end UNVERIFIED
#endif
};
