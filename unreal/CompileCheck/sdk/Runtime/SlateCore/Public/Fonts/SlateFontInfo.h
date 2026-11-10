// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Fonts/SlateFontInfo.h (UE 5.x source knowledge;
// reflected structs: checked against the 5.6 stub). FSlateFontInfo: font object or composite font + typeface name +
// size (float since 5.3) + outline / letter spacing.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Fonts/CompositeFont.h"

struct FFontOutlineSettings
{
	UE_STUB_STRUCT_BODY(FFontOutlineSettings)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OutlineSettings)
	int32 OutlineSize;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OutlineSettings)
	bool bMiteredCorners;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OutlineSettings)
	bool bSeparateFillAlpha;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OutlineSettings)
	bool bApplyOutlineToDropShadows;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OutlineSettings)
	TObjectPtr<UObject> OutlineMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OutlineSettings)
	FLinearColor OutlineColor;
	FFontOutlineSettings();
	FFontOutlineSettings(int32 InOutlineSize, FLinearColor InColor = FLinearColor::Black);
	bool IsIdenticalToForCaching(const FFontOutlineSettings& Other) const;
	bool IsVisible() const;
	static FFontOutlineSettings NoOutline;
};

struct FSlateFontInfo
{
	UE_STUB_STRUCT_BODY(FSlateFontInfo)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SlateStyleRules)
	TObjectPtr<const UObject> FontObject;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SlateStyleRules)
	TObjectPtr<UObject> FontMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SlateStyleRules)
	FFontOutlineSettings OutlineSettings;
	TSharedPtr<const FCompositeFont> CompositeFont;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SlateStyleRules)
	FName TypefaceFontName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SlateStyleRules)
	float Size;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SlateStyleRules)
	int32 LetterSpacing;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SlateStyleRules)
	float SkewAmount;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SlateStyleRules)
	bool bForceMonospaced;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SlateStyleRules)
	float MonospacedWidth;
	FSlateFontInfo();
	FSlateFontInfo(TSharedPtr<const FCompositeFont> InCompositeFont, const float InSize, const FName& InTypefaceFontName = NAME_None, const FFontOutlineSettings& InOutlineSettings = FFontOutlineSettings());
	FSlateFontInfo(const UObject* InFontObject, const float InSize, const FName& InTypefaceFontName = NAME_None, const FFontOutlineSettings& InOutlineSettings = FFontOutlineSettings());
	FSlateFontInfo(const FString& InFontName, float InSize, EFontHinting InHinting = EFontHinting::Default, const FFontOutlineSettings& InOutlineSettings = FFontOutlineSettings());
	FSlateFontInfo(const FName& InFontName, float InSize, EFontHinting InHinting = EFontHinting::Default);
	FSlateFontInfo(const ANSICHAR* InFontName, float InSize, EFontHinting InHinting = EFontHinting::Default);
	FSlateFontInfo(const WIDECHAR* InFontName, float InSize, EFontHinting InHinting = EFontHinting::Default);
	bool IsIdenticalTo(const FSlateFontInfo& Other) const;
	bool operator==(const FSlateFontInfo& Other) const;
	bool HasValidFont() const;
	const FCompositeFont* GetCompositeFont() const;
	float GetClampSize() const;
	float GetClampSkew() const;
};
uint32 GetTypeHash(const FSlateFontInfo& FontInfo);
