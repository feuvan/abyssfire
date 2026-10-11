#include "ContentBuild/AbyssContentBuildLibrary.h"

#include "Abyssfire.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "ReferenceSkeleton.h"

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "MaterialShared.h"
#include "RHI.h"
#include "ShaderCompiler.h"

namespace
{
	// UMaterialInterface::GetMaterialResource takes the shader platform in recent engines and the feature level in older
	// ones: call whichever this engine declares (resolved at compile time; the other branch is never instantiated).
	template <typename MaterialType>
	FMaterialResource* AbyssEditorMaterialResource(MaterialType* Material)
	{
		if constexpr (requires(MaterialType* M) { M->GetMaterialResource(GMaxRHIShaderPlatform); })
		{
			return Material->GetMaterialResource(GMaxRHIShaderPlatform);
		}
		else
		{
			return Material->GetMaterialResource(GMaxRHIFeatureLevel);
		}
	}
}
#endif

bool UAbyssContentBuildLibrary::MeshHasBone(USkeletalMesh* Mesh, FName BoneName)
{
	if (Mesh == nullptr || BoneName.IsNone())
	{
		return false;
	}
	return Mesh->GetRefSkeleton().FindBoneIndex(BoneName) != INDEX_NONE;
}

bool UAbyssContentBuildLibrary::ConfigureSkeletalMeshSocket(USkeletalMesh* Mesh, USkeletalMeshSocket* Socket, FName SocketName,
	FName BoneName)
{
#if WITH_EDITOR
	if (Mesh == nullptr || Socket == nullptr || SocketName.IsNone())
	{
		UE_LOG(LogAbyss, Warning, TEXT("ConfigureSkeletalMeshSocket: missing mesh, socket or name"));
		return false;
	}
	if (!MeshHasBone(Mesh, BoneName))
	{
		UE_LOG(LogAbyss, Warning, TEXT("ConfigureSkeletalMeshSocket: %s has no bone %s (socket %s)"), *Mesh->GetName(),
			*BoneName.ToString(), *SocketName.ToString());
		return false;
	}
	Socket->Modify();
	Socket->SocketName = SocketName;
	Socket->BoneName = BoneName;
	Mesh->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

void UAbyssContentBuildLibrary::FinishCompilation()
{
#if WITH_EDITOR
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager != nullptr)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
#endif
}

bool UAbyssContentBuildLibrary::SetSkeletalMeshLodScreenSize(USkeletalMesh* Mesh, int32 LodIndex, float ScreenSize)
{
#if WITH_EDITOR
	if (Mesh == nullptr)
	{
		return false;
	}
	FSkeletalMeshLODInfo* Info = Mesh->GetLODInfo(LodIndex);
	if (Info == nullptr)
	{
		return false;
	}
	Mesh->Modify();
	Info->ScreenSize = FPerPlatformFloat(ScreenSize);
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

float UAbyssContentBuildLibrary::GetSkeletalMeshLodScreenSize(USkeletalMesh* Mesh, int32 LodIndex)
{
	if (Mesh == nullptr)
	{
		return -1.f;
	}
	const FSkeletalMeshLODInfo* Info = Mesh->GetLODInfo(LodIndex);
	return Info != nullptr ? Info->ScreenSize.Default : -1.f;
}

bool UAbyssContentBuildLibrary::SetMaterialInstanceTextureParameter(UMaterialInstanceConstant* Instance, FName ParameterName,
	UTexture* Value)
{
#if WITH_EDITOR
	if (Instance == nullptr || ParameterName.IsNone())
	{
		return false;
	}
	Instance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(ParameterName), Value);
	Instance->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UAbyssContentBuildLibrary::SetMaterialInstanceScalarParameter(UMaterialInstanceConstant* Instance, FName ParameterName,
	float Value)
{
#if WITH_EDITOR
	if (Instance == nullptr || ParameterName.IsNone())
	{
		return false;
	}
	Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ParameterName), Value);
	Instance->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UAbyssContentBuildLibrary::SetMaterialInstanceVectorParameter(UMaterialInstanceConstant* Instance, FName ParameterName,
	FLinearColor Value)
{
#if WITH_EDITOR
	if (Instance == nullptr || ParameterName.IsNone())
	{
		return false;
	}
	Instance->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(ParameterName), Value);
	Instance->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

TArray<FString> UAbyssContentBuildLibrary::GetMaterialCompileErrors(UMaterialInterface* Material)
{
	TArray<FString> Errors;
#if WITH_EDITOR
	if (Material == nullptr)
	{
		return Errors;
	}
	FMaterialResource* Resource = AbyssEditorMaterialResource(Material);
	if (Resource == nullptr)
	{
		return Errors;
	}
	Resource->FinishCompilation();
	Errors = Resource->GetCompileErrors();
#endif
	return Errors;
}

TArray<FName> UAbyssContentBuildLibrary::GetTextureParameterInfos(UMaterialInterface* Material)
{
	TArray<FName> Names;
	if (Material == nullptr)
	{
		return Names;
	}
	TArray<FMaterialParameterInfo> Infos;
	TArray<FGuid> Ids;
	Material->GetAllParameterInfoOfType(EMaterialParameterType::Texture, Infos, Ids);
	for (const FMaterialParameterInfo& Info : Infos)
	{
		Names.Add(Info.Name);
	}
	return Names;
}
