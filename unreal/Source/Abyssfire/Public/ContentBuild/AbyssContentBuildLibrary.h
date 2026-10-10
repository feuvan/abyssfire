// Editor-time helpers for the content build (unreal/Scripts/build_content.py, ue58-platform.md 11): the two things the
// editor Python API cannot do on its own. Exposed to Python as unreal.AbyssContentBuildLibrary; never called by the game.
//
// * Skeletal-mesh sockets: USkeletalMeshSocket::SocketName / BoneName are read-only to Python, so the build creates the
//   socket object in Python, names it here, then adds it with SkeletalMesh.add_socket (manifest sockets fx_feet,
//   fx_chest, fx_head, fx_overhead, fx_hand_l/r, visor; WorldContract.md 2.3).
// * Waiting for asynchronous asset builds and shader compilation before the build reads material statistics, saves and
//   quits the commandlet.
//
// Only public data members and long-standing exported entry points are used, so the file stays link-safe across 5.x
// engine versions. Both functions do nothing in non-editor builds.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "AbyssContentBuildLibrary.generated.h"

class USkeletalMesh;
class USkeletalMeshSocket;

UCLASS()
class ABYSSFIRE_API UAbyssContentBuildLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Names a skeletal-mesh socket and binds it to a bone of Mesh (Python: configure_skeletal_mesh_socket). The caller
	 * creates the socket (unreal.SkeletalMeshSocket(mesh)), sets its relative transform and adds it with
	 * SkeletalMesh.add_socket. Returns false (and changes nothing) when an argument is missing or the bone is not in the
	 * mesh's reference skeleton. Editor only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Abyssfire|Content Build")
	static bool ConfigureSkeletalMeshSocket(USkeletalMesh* Mesh, USkeletalMeshSocket* Socket, FName SocketName, FName BoneName);

	/** True when Mesh's reference skeleton has BoneName (Python: mesh_has_bone). */
	UFUNCTION(BlueprintCallable, Category = "Abyssfire|Content Build")
	static bool MeshHasBone(USkeletalMesh* Mesh, FName BoneName);

	/**
	 * Blocks until every pending asynchronous asset build (static / skeletal meshes, textures, ...) and every queued
	 * shader compilation job has finished (Python: finish_compilation). Editor only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Abyssfire|Content Build")
	static void FinishCompilation();
};
