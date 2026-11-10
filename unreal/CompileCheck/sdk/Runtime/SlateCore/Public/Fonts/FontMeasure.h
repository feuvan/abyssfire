// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Fonts/FontMeasure.h (UE 5.x source knowledge).
// FSlateFontMeasure: text extents in Slate units (get it from FSlateRenderer::GetFontMeasureService()).
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Types/SlateVector2.h"

class FSlateFontMeasure
{
public:
	UE::Slate::FDeprecateVector2DResult Measure(FStringView Text, const FSlateFontInfo& InFontInfo, float FontScale = 1.0f) const;
	UE::Slate::FDeprecateVector2DResult Measure(const FText& Text, const FSlateFontInfo& InFontInfo, float FontScale = 1.0f) const;
	UE::Slate::FDeprecateVector2DResult Measure(FStringView Text, int32 StartIndex, int32 EndIndex, const FSlateFontInfo& InFontInfo, bool IncludeKerningWithPrecedingChar = true, float FontScale = 1.0f) const;
	int32 FindLastWholeCharacterIndexBeforeOffset(FStringView Text, const FSlateFontInfo& InFontInfo, const int32 HorizontalOffset, float FontScale = 1.0f) const;
	int32 FindCharacterIndexAtOffset(FStringView Text, const FSlateFontInfo& InFontInfo, const int32 HorizontalOffset, float FontScale = 1.0f) const;
	uint16 GetMaxCharacterHeight(const FSlateFontInfo& InFontInfo, float FontScale = 1.0f) const;
	int16 GetBaseline(const FSlateFontInfo& InFontInfo, float FontScale = 1.0f) const;
	void FlushCache();
};
