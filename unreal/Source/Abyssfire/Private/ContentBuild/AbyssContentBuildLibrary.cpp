#include "ContentBuild/AbyssContentBuildLibrary.h"

#include "Abyssfire.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "ReferenceSkeleton.h"

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
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
