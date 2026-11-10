// Stub SDK. Real header: Engine/Plugins/EnhancedInput/Source/EnhancedInput/Public/InputActionValue.h (UE 5.x source
// knowledge; reflected struct / enum: checked against the 5.6 stub). Get<T>() exists for bool, float (Axis1D),
// FVector2D (Axis2D) and FVector (Axis3D) only.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

UENUM(BlueprintType)
enum class EInputActionValueType : uint8
{
	Boolean UMETA(DisplayName = "Digital (bool)"),
	Axis1D UMETA(DisplayName = "Axis1D (float)"),
	Axis2D UMETA(DisplayName = "Axis2D (Vector2D)"),
	Axis3D UMETA(DisplayName = "Axis3D (Vector)"),
};

struct FInputActionValue
{
	UE_STUB_STRUCT_BODY(FInputActionValue)
	using Axis1D = float;
	using Axis2D = FVector2D;
	using Axis3D = FVector;
	FInputActionValue() = default;
	FInputActionValue(const FInputActionValue&) = default;
	FInputActionValue& operator=(const FInputActionValue&) = default;
	explicit FInputActionValue(bool bInValue);
	FInputActionValue(Axis1D InValue);
	FInputActionValue(Axis2D InValue);
	FInputActionValue(Axis3D InValue);
	FInputActionValue(EInputActionValueType InValueType, Axis3D InValue);
	void Reset();
	FInputActionValue ConvertToType(const FInputActionValue& Other) const;
	FInputActionValue ConvertToType(EInputActionValueType Type) const;
	bool IsNonZero(float Tolerance = UE_KINDA_SMALL_NUMBER) const;
	template <typename T> requires std::is_same_v<T, bool> || std::is_same_v<T, Axis1D> || std::is_same_v<T, Axis2D> || std::is_same_v<T, Axis3D>
	T Get() const UE_STUB_BODY
	float GetMagnitudeSq() const;
	float GetMagnitude() const;
	EInputActionValueType GetValueType() const;
	FString ToString() const;
};
