// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/MaterialShared.h (UE 5.x source knowledge). Only the part of
// FMaterial / FMaterialResource the content-build helper uses; the compile-state functions exist only WITH_EDITOR.
#pragma once

#include "CoreMinimal.h"
#include "MaterialTypes.h"
#include "SceneTypes.h"
#include "RHIDefinitions.h"

class UMaterial;
class UMaterialInterface;
class UMaterialInstance;

class FMaterial
{
public:
	FMaterial();
	virtual ~FMaterial();
#if WITH_EDITOR
	/** Blocks until compilation has completed. Returns immediately if a compilation is not outstanding. */
	void FinishCompilation();
	/** Checks if the compilation for this shader is finished. */
	bool IsCompilationFinished() const;
	const TArray<FString>& GetCompileErrors() const;
	void SetCompileErrors(const TArray<FString>& InCompileErrors);
#endif
	ERHIFeatureLevel::Type GetFeatureLevel() const;
	EMaterialQualityLevel::Type GetQualityLevel() const;
	virtual FString GetFriendlyName() const = 0;
	virtual bool IsDefaultMaterial() const;
};

class FMaterialResource : public FMaterial
{
public:
	FMaterialResource();
	virtual ~FMaterialResource();
	virtual FString GetFriendlyName() const override;
	virtual bool IsDefaultMaterial() const override;
	UMaterial* GetMaterial() const;
	UMaterialInstance* GetMaterialInstance() const;
};
