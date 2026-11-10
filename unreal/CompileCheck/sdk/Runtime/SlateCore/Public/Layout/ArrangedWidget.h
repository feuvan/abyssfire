// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Layout/ArrangedWidget.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"

class SWidget;

class FArrangedWidget
{
public:
	FArrangedWidget(TSharedRef<SWidget> InWidget, const FGeometry& InGeometry);
	static const FArrangedWidget& GetNullWidget();
	FString ToString() const;
	bool operator==(const FArrangedWidget& Other) const;
	FGeometry Geometry;
	TSharedRef<SWidget> Widget;
};
