// Stub SDK. Real header: Engine/Plugins/EnhancedInput/Source/EnhancedInput/Public/InputModifiers.h (UE 5.x source
// knowledge; reflected classes / enums: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InputActionValue.h"

UENUM()
enum class EDeadZoneType : uint8
{
	Axial,
	Radial,
	UnscaledRadial,
};
UENUM()
enum class EInputAxisSwizzle : uint8
{
	YXZ,
	ZYX,
	XZY,
	YZX,
	ZXY,
};

class UInputModifier : public UObject
{
	UE_STUB_CLASS_BODY(UInputModifier, UObject)
};
class UInputModifierDeadZone : public UInputModifier
{
	UE_STUB_CLASS_BODY(UInputModifierDeadZone, UInputModifier)
public:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings, Config)
	float LowerThreshold;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings, Config)
	float UpperThreshold;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings, Config)
	EDeadZoneType Type;
};
class UInputModifierScalar : public UInputModifier
{
	UE_STUB_CLASS_BODY(UInputModifierScalar, UInputModifier)
public:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings)
	FVector Scalar;
};
class UInputModifierNegate : public UInputModifier
{
	UE_STUB_CLASS_BODY(UInputModifierNegate, UInputModifier)
public:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings)
	bool bX;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings)
	bool bY;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings)
	bool bZ;
};
class UInputModifierSwizzleAxis : public UInputModifier
{
	UE_STUB_CLASS_BODY(UInputModifierSwizzleAxis, UInputModifier)
public:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = Settings)
	EInputAxisSwizzle Order;
};
