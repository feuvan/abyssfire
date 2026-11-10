// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Rendering/SlateRenderer.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateShaderResource.h"
#include "Types/SlateVector2.h"

class FSlateFontMeasure;
class FSlateFontCache;
class FSlateFontServices;
class SWindow;
struct FSlateBrush;

class FSlateRenderer
{
public:
	FSlateRenderer(const TSharedRef<FSlateFontServices>& InSlateFontServices);
	virtual ~FSlateRenderer();
	virtual bool Initialize() = 0;
	virtual void Destroy() = 0;
	virtual void FlushCommands() const;
	virtual void Sync() const;
	virtual void ReloadTextureResources();
	virtual bool GenerateDynamicImageResource(const FName InTextureName);
	virtual FSlateResourceHandle GetResourceHandle(const FSlateBrush& Brush, FVector2f LocalSize, float DrawScale) = 0;
	FSlateResourceHandle GetResourceHandle(const FSlateBrush& Brush);
	virtual bool CanRenderResource(UObject& InResourceObject) const;
	virtual void ReleaseDynamicResource(const FSlateBrush& Brush) = 0;
	TSharedRef<FSlateFontMeasure> GetFontMeasureService() const;
	TSharedRef<FSlateFontCache> GetFontCache() const;
	virtual void RequestResize(const TSharedPtr<SWindow>& Window, uint32 NewWidth, uint32 NewHeight) = 0;
	virtual FIntPoint GenerateDynamicImageResource(const FName ResourceName, uint32 Width, uint32 Height, const TArray<uint8>& Bytes);
	virtual void PrepareToTakeScreenshot(const FIntRect& Rect, TArray<FColor>* OutColorData, SWindow* InScreenshotWindow);
};
