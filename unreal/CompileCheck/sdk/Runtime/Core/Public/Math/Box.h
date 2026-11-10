// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/Box.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/Vector.h"
#include "Math/Sphere.h"
#include "Containers/Array.h"

namespace UE::Math
{
	template <typename T>
	struct TBox
	{
		using FReal = T;
		TVector<T> Min;
		TVector<T> Max;
		uint8 IsValid;
		TBox() = default;
		explicit TBox(EForceInit) UE_STUB_BODY
		template <typename FArg> requires std::is_arithmetic_v<FArg>
		TBox(const TVector<FArg>& InMin, const TVector<FArg>& InMax) UE_STUB_BODY
		TBox(const TVector4<T>& InMin, const TVector4<T>& InMax) UE_STUB_BODY
		TBox(const TVector<T>* Points, int32 Count) UE_STUB_BODY
		explicit TBox(const TArray<TVector<T>>& Points) UE_STUB_BODY
		template <typename FArg> requires (!std::is_same_v<T, FArg>)
		explicit TBox(const TBox<FArg>& From) UE_STUB_BODY
		bool operator==(const TBox& Other) const UE_STUB_BODY
		bool operator!=(const TBox& Other) const UE_STUB_BODY
		bool Equals(const TBox& Other, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		TBox& operator+=(const TVector<T>& Other) UE_STUB_BODY
		TBox operator+(const TVector<T>& Other) const UE_STUB_BODY
		TBox& operator+=(const TBox& Other) UE_STUB_BODY
		TBox operator+(const TBox& Other) const UE_STUB_BODY
		TVector<T>& operator[](int32 Index) UE_STUB_BODY
		const TVector<T>& operator[](int32 Index) const UE_STUB_BODY
		T ComputeSquaredDistanceToPoint(const TVector<T>& Point) const UE_STUB_BODY
		T ComputeSquaredDistanceToBox(const TBox& Box) const UE_STUB_BODY
		TBox ExpandBy(T W) const UE_STUB_BODY
		TBox ExpandBy(const TVector<T>& V) const UE_STUB_BODY
		TBox ExpandBy(const TVector<T>& Neg, const TVector<T>& Pos) const UE_STUB_BODY
		TBox ShiftBy(const TVector<T>& Offset) const UE_STUB_BODY
		TBox MoveTo(const TVector<T>& Destination) const UE_STUB_BODY
		TVector<T> GetCenter() const UE_STUB_BODY
		void GetCenterAndExtents(TVector<T>& Center, TVector<T>& Extents) const UE_STUB_BODY
		TVector<T> GetClosestPointTo(const TVector<T>& Point) const UE_STUB_BODY
		TVector<T> GetExtent() const UE_STUB_BODY
		TVector<T> GetSize() const UE_STUB_BODY
		T GetVolume() const UE_STUB_BODY
		void Init() UE_STUB_BODY
		bool Intersect(const TBox& Other) const UE_STUB_BODY
		bool IntersectXY(const TBox& Other) const UE_STUB_BODY
		TBox Overlap(const TBox& Other) const UE_STUB_BODY
		TBox InverseTransformBy(const TTransform<T>& M) const UE_STUB_BODY
		bool IsInside(const TVector<T>& In) const UE_STUB_BODY
		bool IsInsideOrOn(const TVector<T>& In) const UE_STUB_BODY
		bool IsInside(const TBox& Other) const UE_STUB_BODY
		bool IsInsideOrOn(const TBox& Other) const UE_STUB_BODY
		bool IsInsideXY(const TVector<T>& In) const UE_STUB_BODY
		bool IsInsideXY(const TBox& Other) const UE_STUB_BODY
		TBox TransformBy(const TMatrix<T>& M) const UE_STUB_BODY
		TBox TransformBy(const TTransform<T>& M) const UE_STUB_BODY
		TBox TransformProjectBy(const TMatrix<T>& ProjM) const UE_STUB_BODY
		FString ToString() const UE_STUB_BODY
		void GetVertices(TVector<T> Vertices[8]) const UE_STUB_BODY
		static TBox BuildAABB(const TVector<T>& Origin, const TVector<T>& Extent) UE_STUB_BODY
	};
}
