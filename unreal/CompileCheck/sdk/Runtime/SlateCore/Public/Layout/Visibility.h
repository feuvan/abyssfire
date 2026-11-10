// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Layout/Visibility.h (UE 5.x source knowledge).
// EVisibility is a struct with static instances (EVisibility::Visible, ...), not an enum.
#pragma once

#include "CoreMinimal.h"

struct EVisibility
{
	static const EVisibility Visible;
	static const EVisibility Collapsed;
	static const EVisibility Hidden;
	static const EVisibility HitTestInvisible;
	static const EVisibility SelfHitTestInvisible;
	static const EVisibility All;
	EVisibility() : Value(0) {}
	bool operator==(const EVisibility& Other) const { return Value == Other.Value; }
	bool operator!=(const EVisibility& Other) const { return Value != Other.Value; }
	bool AreChildrenHitTestVisible() const;
	bool IsHitTestVisible() const;
	bool IsVisible() const;
	FString ToString() const;
private:
	uint8 Value;
};
