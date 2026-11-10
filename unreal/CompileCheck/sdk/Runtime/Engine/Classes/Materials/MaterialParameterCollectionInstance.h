// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Materials/MaterialParameterCollectionInstance.h (UE 5.x
// source knowledge; not exposed to Python, so nothing here could be cross-checked). The per-world value overrides of a
// UMaterialParameterCollection (UWorld::GetParameterCollectionInstance). Only the FLinearColor form of
// SetVectorParameterValue is declared: an FVector4 overload may exist in 5.x and was not confirmed.
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
	virtual void PostInitProperties() override;
	virtual void FinishDestroy() override;
};
