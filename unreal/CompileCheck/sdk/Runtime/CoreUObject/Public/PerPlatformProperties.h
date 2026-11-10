// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/PerPlatformProperties.h, reached as
// "PerPlatformProperties.h" through Engine/Classes (UE 5.x source knowledge; reflected structs: checked against the
// 5.6 stub). FPerPlatformFloat / Int / Bool: a Default value plus per-platform overrides (editor data).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

struct FPerPlatformInt
{
	UE_STUB_STRUCT_BODY(FPerPlatformInt)
	UPROPERTY(EditAnywhere, Category = PerPlatform)
	int32 Default;
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = PerPlatform)
	TMap<FName, int32> PerPlatform;
#endif
	FPerPlatformInt();
	FPerPlatformInt(int32 InDefaultValue);
	int32 GetDefault() const;
	int32 GetValue() const;
};
struct FPerPlatformFloat
{
	UE_STUB_STRUCT_BODY(FPerPlatformFloat)
	UPROPERTY(EditAnywhere, Category = PerPlatform)
	float Default;
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = PerPlatform)
	TMap<FName, float> PerPlatform;
#endif
	FPerPlatformFloat();
	FPerPlatformFloat(float InDefaultValue);
	float GetDefault() const;
	float GetValue() const;
};
struct FPerPlatformBool
{
	UE_STUB_STRUCT_BODY(FPerPlatformBool)
	UPROPERTY(EditAnywhere, Category = PerPlatform)
	bool Default;
	FPerPlatformBool();
	FPerPlatformBool(bool bInDefaultValue);
	bool GetDefault() const;
	bool GetValue() const;
};
