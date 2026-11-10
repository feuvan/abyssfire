// Stub SDK. Real header: Engine/Plugins/Runtime/ProceduralMeshComponent/Source/ProceduralMeshComponent/Public/
// ProceduralMeshComponent.h (UE 5.x source knowledge; UFUNCTION / UPROPERTY-marked members and FProcMeshTangent:
// checked against the 5.6 stub). The real class also implements IInterface_CollisionDataProvider (not modelled). The
// eight-argument CreateMeshSection_LinearColor is the engine's inline C++ shortcut for one UV channel.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"

class UBodySetup;

/** Struct used to specify a tangent vector for a vertex. The Y tangent is computed from the cross product of the vertex normal (Tangent Z) and the TangentX member. */
struct FProcMeshTangent
{
	UE_STUB_STRUCT_BODY(FProcMeshTangent)
	/** Direction of X tangent for this vertex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tangent)
	FVector TangentX;
	/** Bool that indicates whether we should flip the Y tangent when we compute it using cross product. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Tangent)
	bool bFlipTangentY;
	FProcMeshTangent() : TangentX(1.f, 0.f, 0.f), bFlipTangentY(false) {}
	FProcMeshTangent(float X, float Y, float Z) : TangentX(X, Y, Z), bFlipTangentY(false) {}
	FProcMeshTangent(FVector InTangentX, bool bInFlipTangentY) : TangentX(InTangentX), bFlipTangentY(bInFlipTangentY) {}
};

/** Component that allows you to specify custom triangle mesh geometry. */
class UProceduralMeshComponent : public UMeshComponent
{
	UE_STUB_CLASS_BODY(UProceduralMeshComponent, UMeshComponent)
public:
	/** Create/replace a section for this procedural mesh component (FColor vertex colours; deprecated for Blueprints). */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh", meta = (DisplayName = "Create Mesh Section FColor", AutoCreateRefTerm = "Normals,UV0,VertexColors,Tangents"))
	void CreateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0,
		const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision);
	void CreateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0,
		const TArray<FVector2D>& UV1, const TArray<FVector2D>& UV2, const TArray<FVector2D>& UV3, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision);
	/** Create/replace a section for this procedural mesh component. */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh", meta = (DisplayName = "Create Mesh Section", AutoCreateRefTerm = "Normals,UV0,UV1,UV2,UV3,VertexColors,Tangents", AdvancedDisplay = "UV1,UV2,UV3"))
	void CreateMeshSection_LinearColor(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0,
		const TArray<FVector2D>& UV1, const TArray<FVector2D>& UV2, const TArray<FVector2D>& UV3, const TArray<FLinearColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents,
		bool bCreateCollision, bool bSRGBConversion = false);
	// Shorter version of CreateMeshSection_LinearColor for use from C++ (one UV channel).
	void CreateMeshSection_LinearColor(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0,
		const TArray<FLinearColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision, bool bSRGBConversion = false)
	{
		TArray<FVector2D> EmptyArray;
		CreateMeshSection_LinearColor(SectionIndex, Vertices, Triangles, Normals, UV0, EmptyArray, EmptyArray, EmptyArray, VertexColors, Tangents, bCreateCollision, bSRGBConversion);
	}
	/** Updates a section of this procedural mesh component (faster than creating or replacing; the topology must not change). */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh", meta = (DisplayName = "Update Mesh Section FColor", AutoCreateRefTerm = "Normals,UV0,VertexColors,Tangents"))
	void UpdateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents);
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh", meta = (DisplayName = "Update Mesh Section", AutoCreateRefTerm = "Normals,UV0,UV1,UV2,UV3,VertexColors,Tangents", AdvancedDisplay = "UV1,UV2,UV3"))
	void UpdateMeshSection_LinearColor(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FVector2D>& UV1,
		const TArray<FVector2D>& UV2, const TArray<FVector2D>& UV3, const TArray<FLinearColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bSRGBConversion = true);
	/** Clear a section of the procedural mesh. Other sections do not change index. */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh")
	void ClearMeshSection(int32 SectionIndex);
	/** Clear all mesh sections and reset to empty state. */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh")
	void ClearAllMeshSections();
	/** Control visibility of a particular section. */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh")
	void SetMeshSectionVisible(int32 SectionIndex, bool bNewVisibility);
	/** Returns whether a particular section is currently visible. */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh")
	bool IsMeshSectionVisible(int32 SectionIndex) const;
	/** Returns number of sections currently created for this component. */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh")
	int32 GetNumSections() const;
	/** Add simple collision convex to this component. */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh")
	void AddCollisionConvexMesh(TArray<FVector> ConvexVerts);
	/** Remove collision meshes from this component. */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMesh")
	void ClearCollisionConvexMeshes();
	/**
	 * Controls whether the complex (Per poly) geometry should be treated as 'simple' collision. Should be set to false if
	 * this component is going to be given simple collision and simulated.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Mesh")
	bool bUseComplexAsSimpleCollision;
	/** Controls whether the physics cooking should be done off the game thread. This should be used when collision geometry doesn't have to be immediately up to date. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Mesh")
	bool bUseAsyncCooking;
	/** Collision data. */
	UPROPERTY(Instanced)
	TObjectPtr<class UBodySetup> ProcMeshBodySetup;
	virtual int32 GetNumMaterials() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void PostLoad() override;
};
