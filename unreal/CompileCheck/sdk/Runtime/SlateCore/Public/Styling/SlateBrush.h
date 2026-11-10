// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Styling/SlateBrush.h (UE 5.x source knowledge; reflected
// struct / enums: checked against the 5.6 stub). FSlateBrush: how a rectangle is drawn (image, box, border, rounded
// box). ImageSize is an FVector2f-based FDeprecateSlateVector2D; TintColor an FSlateColor; the resource a UObject (the
// brush does not keep it alive) or a named texture.
#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateShaderResource.h"
#include "UObject/ObjectMacros.h"
#include "Layout/Margin.h"
#include "Styling/SlateColor.h"
#include "Types/SlateVector2.h"
#include "Math/Box2D.h"
#include "UObject/ObjectPtr.h"

UENUM(BlueprintType)
namespace ESlateBrushDrawType
{
	enum Type : int
	{
		NoDrawType UMETA(DisplayName = "None"),
		Box,
		Border,
		Image,
		RoundedBox,
	};
}
UENUM(BlueprintType)
namespace ESlateBrushTileType
{
	enum Type : int
	{
		NoTile,
		Horizontal,
		Vertical,
		Both,
	};
}
UENUM()
namespace ESlateBrushMirrorType
{
	enum Type : int
	{
		NoMirror,
		Horizontal,
		Vertical,
		Both,
	};
}
UENUM()
namespace ESlateBrushImageType
{
	enum Type : int
	{
		NoImage,
		FullColor,
		Linear,
		Vector,
	};
}
UENUM()
namespace ESlateBrushRoundingType
{
	enum Type : int
	{
		FixedRadius,
		HalfHeightRadius,
	};
}

struct FSlateBrushOutlineSettings
{
	UE_STUB_STRUCT_BODY(FSlateBrushOutlineSettings)
	FSlateBrushOutlineSettings();
	FSlateBrushOutlineSettings(float InUniformRadius);
	FSlateBrushOutlineSettings(FVector4 InRadius);
	FSlateBrushOutlineSettings(const FSlateColor& InColor, float InWidth);
	FSlateBrushOutlineSettings(float InUniformRadius, const FSlateColor& InColor, float InWidth);
	FSlateBrushOutlineSettings(FVector4 InRadius, const FSlateColor& InColor, float InWidth);
	bool operator==(const FSlateBrushOutlineSettings& Other) const;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Brush)
	FVector4 CornerRadii;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Brush)
	FSlateColor Color;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Brush)
	float Width;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Brush)
	TEnumAsByte<enum ESlateBrushRoundingType::Type> RoundingType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Brush)
	bool bUseBrushTransparency;
};

struct FSlateBrush
{
	UE_STUB_STRUCT_BODY(FSlateBrush)
	FSlateBrush();
	virtual ~FSlateBrush() {}
	UPROPERTY()
	uint8 bIsDynamicallyLoaded : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Brush)
	TEnumAsByte<enum ESlateBrushDrawType::Type> DrawAs;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Brush)
	TEnumAsByte<enum ESlateBrushTileType::Type> Tiling;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Brush)
	TEnumAsByte<enum ESlateBrushMirrorType::Type> Mirroring;
	UPROPERTY()
	TEnumAsByte<enum ESlateBrushImageType::Type> ImageType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Brush)
	UE::Slate::FDeprecateSlateVector2D ImageSize;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Brush)
	FMargin Margin;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Brush, meta = (DisplayName = "Tint", sRGB = "true"))
	FSlateColor TintColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Brush)
	FSlateBrushOutlineSettings OutlineSettings;
	UE::Slate::FDeprecateVector2DResult GetImageSize() const;
	void SetImageSize(UE::Slate::FDeprecateVector2DParameter InImageSize);
	const FMargin& GetMargin() const;
	ESlateBrushTileType::Type GetTiling() const;
	ESlateBrushMirrorType::Type GetMirroring() const;
	ESlateBrushImageType::Type GetImageType() const;
	ESlateBrushDrawType::Type GetDrawType() const;
	FLinearColor GetTint(const FWidgetStyle& InWidgetStyle) const;
	const FName GetResourceName() const;
	class UObject* GetResourceObject() const;
	void SetResourceObject(class UObject* InResourceObject);
	const FBox2f& GetUVRegion() const;
	void SetUVRegion(const FBox2d& InUVRegion);
	void SetUVRegion(const FBox2f& InUVRegion);
	bool operator==(const FSlateBrush& Other) const;
	bool operator!=(const FSlateBrush& Other) const;
	bool IsDynamicallyLoaded() const;
	bool HasUObject() const;
	bool IsSet() const;
	static const FString UTextureIdentifier();
	const FSlateResourceHandle& GetRenderingResource(UE::Slate::FDeprecateVector2DParameter LocalSize, float DrawScale) const;
	const FSlateResourceHandle& GetRenderingResource() const;
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Brush)
	TObjectPtr<UObject> ResourceObject;
	UPROPERTY()
	FName ResourceName;
	FBox2f UVRegion;
	FSlateBrush(ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const UE::Slate::FDeprecateVector2DParameter& InImageSize, const FLinearColor& InTint = FLinearColor::White, UObject* InObjectResource = nullptr, bool bInDynamicallyLoaded = false);
	FSlateBrush(ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const UE::Slate::FDeprecateVector2DParameter& InImageSize, const TSharedRef<FLinearColor>& InTint, UObject* InObjectResource = nullptr, bool bInDynamicallyLoaded = false);
	FSlateBrush(ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const UE::Slate::FDeprecateVector2DParameter& InImageSize, const FSlateColor& InTint, UObject* InObjectResource = nullptr, bool bInDynamicallyLoaded = false);
};
struct FSlateOptionalBrush : public FSlateBrush
{
	FSlateOptionalBrush();
};
