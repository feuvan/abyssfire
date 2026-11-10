// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Materials/MaterialParameterCollection.h (UE 5.x source
// knowledge; UFUNCTION / UPROPERTY-marked members and the structs: checked against the 5.6 stub). The *ByName finders
// are C++-only.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"

/** Base struct for collection parameters. */
struct FCollectionParameterBase
{
	UE_STUB_STRUCT_BODY(FCollectionParameterBase)
	FCollectionParameterBase();
	/** The name of the parameter. Changing this name will break any blueprints that reference the parameter. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FName ParameterName;
	/** Uniquely identifies the parameter, used for fixing up materials that reference this parameter when renaming. */
	UPROPERTY()
	FGuid Id;
};
/** A scalar parameter. */
struct FCollectionScalarParameter : public FCollectionParameterBase
{
	UE_STUB_STRUCT_BODY(FCollectionScalarParameter)
	FCollectionScalarParameter();
	UPROPERTY(EditAnywhere, Category = Parameter)
	float DefaultValue;
};
/** A vector parameter. */
struct FCollectionVectorParameter : public FCollectionParameterBase
{
	UE_STUB_STRUCT_BODY(FCollectionVectorParameter)
	FCollectionVectorParameter();
	UPROPERTY(EditAnywhere, Category = Parameter)
	FLinearColor DefaultValue;
};

/** Asset class that contains a list of parameter names and their default values. Any number of materials can reference these parameters. */
class UMaterialParameterCollection : public UObject
{
	UE_STUB_CLASS_BODY(UMaterialParameterCollection, UObject)
public:
	/** Used by materials using this collection to know when to recompile. */
	UPROPERTY(duplicatetransient)
	FGuid StateId;
	UPROPERTY(EditAnywhere, Category = Material)
	TArray<FCollectionScalarParameter> ScalarParameters;
	UPROPERTY(EditAnywhere, Category = Material)
	TArray<FCollectionVectorParameter> VectorParameters;
	/** Gets an array of the names of all the scalar parameters in this Material Parameter Collection. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material", meta = (Keywords = "GetFloatParameterNames"))
	TArray<FName> GetScalarParameterNames() const;
	/** Gets an array of the names of all the vector parameters in this Material Parameter Collection. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material", meta = (Keywords = "GetVectorParameterNames"))
	TArray<FName> GetVectorParameterNames() const;
	/** Gets the default value of a scalar parameter from a material collection. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material", meta = (Keywords = "GetFloatParameterDefaultValue"))
	float GetScalarParameterDefaultValue(FName ParameterName, bool& bParameterFound) const;
	/** Gets the default value of a vector parameter from a material collection. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material", meta = (Keywords = "GetVectorParameterDefaultValue"))
	FLinearColor GetVectorParameterDefaultValue(FName ParameterName, bool& bParameterFound) const;
	/** Utility to find a scalar parameter struct given a parameter name. Returns NULL if not found. */
	const FCollectionScalarParameter* GetScalarParameterByName(FName ParameterName) const;
	/** Utility to find a vector parameter struct given a parameter name. Returns NULL if not found. */
	const FCollectionVectorParameter* GetVectorParameterByName(FName ParameterName) const;
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
};
