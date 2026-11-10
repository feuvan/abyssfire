// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/Rendering/SkeletalMeshRenderData.h (UE 5.x source
// knowledge; not reflected). Strict side: FSkeletalMeshLODRenderData is only forward-declared here (whether the real
// header includes Rendering/SkeletalMeshLODRenderData.h was not confirmed), so indexing LODRenderData needs that
// header included explicitly, which is valid either way.
#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"

class FSkeletalMeshLODRenderData;

class FSkeletalMeshRenderData
{
public:
	/** Per-LOD render data. */
	TIndirectArray<FSkeletalMeshLODRenderData> LODRenderData;
	FSkeletalMeshRenderData();
	~FSkeletalMeshRenderData();
};
