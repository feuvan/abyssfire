// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/BoxSphereBounds.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/Vector.h"
#include "Math/Box.h"
#include "Math/Sphere.h"

namespace UE::Math
{
	template <typename T, typename TExtent = T>
	struct TBoxSphereBoundsStub;
	template <typename T>
	struct TBoxSphereBounds
	{
		TVector<T> Origin;
		TVector<T> BoxExtent;
		T SphereRadius;
		TBoxSphereBounds() = default;
		explicit TBoxSphereBounds(EForceInit) UE_STUB_BODY
		TBoxSphereBounds(const TVector<T>& InOrigin, const TVector<T>& InBoxExtent, T InSphereRadius) UE_STUB_BODY
		TBoxSphereBounds(const TBox<T>& Box, const TSphere<T>& Sphere) UE_STUB_BODY
		TBoxSphereBounds(const TBox<T>& Box) UE_STUB_BODY
		TBoxSphereBounds(const TSphere<T>& Sphere) UE_STUB_BODY
		TBoxSphereBounds(const TVector<T>* Points, uint32 NumPoints) UE_STUB_BODY
		TBoxSphereBounds operator+(const TBoxSphereBounds& Other) const UE_STUB_BODY
		bool operator==(const TBoxSphereBounds& Other) const UE_STUB_BODY
		bool operator!=(const TBoxSphereBounds& Other) const UE_STUB_BODY
		T ComputeSquaredDistanceFromBoxToPoint(const TVector<T>& Point) const UE_STUB_BODY
		static bool SpheresIntersect(const TBoxSphereBounds& A, const TBoxSphereBounds& B, T Tolerance = UE_KINDA_SMALL_NUMBER) UE_STUB_BODY
		static bool BoxesIntersect(const TBoxSphereBounds& A, const TBoxSphereBounds& B) UE_STUB_BODY
		TBox<T> GetBox() const UE_STUB_BODY
		TVector<T> GetBoxExtrema(uint32 Extrema) const UE_STUB_BODY
		TSphere<T> GetSphere() const UE_STUB_BODY
		TBoxSphereBounds ExpandBy(T ExpandAmount) const UE_STUB_BODY
		TBoxSphereBounds TransformBy(const TMatrix<T>& M) const UE_STUB_BODY
		TBoxSphereBounds TransformBy(const TTransform<T>& M) const UE_STUB_BODY
		FString ToString() const UE_STUB_BODY
		bool ContainsNaN() const UE_STUB_BODY
	};
}
