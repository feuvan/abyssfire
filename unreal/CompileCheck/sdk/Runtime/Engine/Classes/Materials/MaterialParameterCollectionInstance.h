// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Materials/MaterialParameterCollectionInstance.h (UE 5.x
// source knowledge; not exposed to Python, so nothing here could be cross-checked). The per-world value overrides of a
// UMaterialParameterCollection (UWorld::GetParameterCollectionInstance). SetVectorParameterValue has two overloads
// (FLinearColor, and FVector4 since the 5.0 large-world change; the second is from memory: API_UNCERTAIN.md,
// lower-confidence rows). Pass a typed FLinearColor: what a braced list resolves to depends on the compiler.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

class UMaterialParameterCollection;
class UWorld;

class UMaterialParameterCollectionInstance : public UObject
{
	UE_STUB_CLASS_BODY(UMaterialParameterCollectionInstance, UObject)
public:
	/** Sets parameter value overrides, returns false if the parameter was not found. */
	bool SetScalarParameterValue(FName ParameterName, float ParameterValue);
	bool SetVectorParameterValue(FName ParameterName, const FLinearColor& ParameterValue);
	bool SetVectorParameterValue(FName ParameterName, const FVector4& ParameterValue);
	virtual void PostInitProperties() override;
	virtual void FinishDestroy() override;
};
