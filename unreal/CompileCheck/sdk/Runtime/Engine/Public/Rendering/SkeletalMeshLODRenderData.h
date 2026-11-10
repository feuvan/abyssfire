// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/Rendering/SkeletalMeshLODRenderData.h (UE 5.x source
// knowledge; not reflected). Render sections of one LOD: only the plain data members a game reads.
#pragma once

#include "CoreMinimal.h"
#include "BoneIndices.h"

/** A set of skeletal mesh triangles which use the same material. */
struct FSkelMeshRenderSection
{
	/** Material (texture) used for this section. */
	uint16 MaterialIndex;
	/** The offset of this section's indices in the LOD's index buffer. */
	uint32 BaseIndex;
	/** The number of triangles in this section. */
	uint32 NumTriangles;
	/** The offset into the LOD's vertex buffer of this section's vertices. */
	uint32 BaseVertexIndex;
	/** The number of vertices in this section. */
	uint32 NumVertices;
	/** Disabled sections will not be collected when rendering. */
	bool bDisabled;
	FSkelMeshRenderSection();
};

class FSkeletalMeshLODRenderData
{
public:
	/** Info about each section of this LOD for rendering. */
	TArray<FSkelMeshRenderSection> RenderSections;
	FSkeletalMeshLODRenderData(bool bAddRef = true);
	~FSkeletalMeshLODRenderData();
};
