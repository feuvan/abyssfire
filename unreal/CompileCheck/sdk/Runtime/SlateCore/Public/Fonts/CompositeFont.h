// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Fonts/CompositeFont.h (UE 5.x source knowledge;
// reflected enums / structs: checked against the 5.6 stub). Runtime fonts from files: FCompositeFont with a default
// FTypeface (named FFontData entries) and optional sub-typefaces for character ranges.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Math/Range.h"
#include "UObject/GCObject.h"

UENUM()
enum class EFontHinting : uint8
{
	Default,
	Auto,
	AutoLight,
	Monochrome,
	None,
};
UENUM()
enum class EFontLoadingPolicy : uint8
{
	LazyLoad,
	Stream,
	Inline,
};
UENUM()
enum class EFontLayoutMethod : uint8
{
	Metrics,
	BoundingBox,
};

struct FFontData
{
	UE_STUB_STRUCT_BODY(FFontData)
	FFontData();
	explicit FFontData(const UObject* const InFontFaceAsset, const int32 InSubFaceIndex = 0);
	FFontData(FString InFontFilename, const EFontHinting InHinting, const EFontLoadingPolicy InLoadingPolicy, const int32 InSubFaceIndex = 0);
	bool HasFont() const;
	const FString& GetFontFilename() const;
	EFontHinting GetHinting() const;
	EFontLoadingPolicy GetLoadingPolicy() const;
	int32 GetSubFaceIndex() const;
	void SetSubFaceIndex(const int32 InSubFaceIndex);
	const UObject* GetFontFaceAsset() const;
	bool operator==(const FFontData& Other) const;
	bool operator!=(const FFontData& Other) const;
};
struct FTypefaceEntry
{
	UE_STUB_STRUCT_BODY(FTypefaceEntry)
	FTypefaceEntry();
	FTypefaceEntry(const FName& InFontName);
	FTypefaceEntry(const FName& InFontName, FString InFontFilename, const EFontHinting InHinting, const EFontLoadingPolicy InLoadingPolicy);
	UPROPERTY()
	FName Name;
	UPROPERTY()
	FFontData Font;
};
struct FTypeface
{
	UE_STUB_STRUCT_BODY(FTypeface)
	FTypeface();
	FTypeface(const FName& InFontName, FString InFontFilename, const EFontHinting InHinting, const EFontLoadingPolicy InLoadingPolicy);
	FTypeface& AppendFont(const FName& InFontName, FString InFontFilename, const EFontHinting InHinting, const EFontLoadingPolicy InLoadingPolicy);
	UPROPERTY()
	TArray<FTypefaceEntry> Fonts;
};
struct FCompositeFallbackFont
{
	UE_STUB_STRUCT_BODY(FCompositeFallbackFont)
	FCompositeFallbackFont();
	UPROPERTY()
	FTypeface Typeface;
	UPROPERTY()
	float ScalingFactor;
};
struct FCompositeSubFont : public FCompositeFallbackFont
{
	UE_STUB_STRUCT_BODY(FCompositeSubFont)
	UPROPERTY()
	TArray<FInt32Range> CharacterRanges;
	UPROPERTY()
	FString Cultures;
	UPROPERTY()
	FName EditorName;
};
struct FCompositeFont
{
	UE_STUB_STRUCT_BODY(FCompositeFont)
	FCompositeFont();
	FCompositeFont(const FName& InFontName, FString InFontFilename, const EFontHinting InHinting, const EFontLoadingPolicy InLoadingPolicy);
	void MakeDirty();
	UPROPERTY()
	FTypeface DefaultTypeface;
	UPROPERTY()
	FCompositeFallbackFont FallbackTypeface;
	UPROPERTY()
	TArray<FCompositeSubFont> SubTypefaces;
};
struct FStandaloneCompositeFont : public FCompositeFont, public FGCObject
{
	FStandaloneCompositeFont();
	FStandaloneCompositeFont(const FName& InFontName, FString InFontFilename, const EFontHinting InHinting, const EFontLoadingPolicy InLoadingPolicy);
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
};
