// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/Sphere.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/Vector.h"

namespace UE::Math
{
	template <typename T>
	struct TSphere
	{
		using FReal = T;
		TVector<T> Center;
		T W;
		TSphere() = default;
		TSphere(int32) UE_STUB_BODY
		TSphere(TVector<T> InV, T InW) UE_STUB_BODY
		explicit TSphere(EForceInit) UE_STUB_BODY
		TSphere(const TVector<T>* Points, int32 Num) UE_STUB_BODY
		bool Equals(const TSphere& Sphere, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		bool IsInside(const TSphere& Other, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		bool IsInside(const TVector<T>& In, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		bool Intersects(const TSphere& Other, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		TSphere TransformBy(const TMatrix<T>& M) const UE_STUB_BODY
		TSphere TransformBy(const TTransform<T>& M) const UE_STUB_BODY
		T GetVolume() const UE_STUB_BODY
		TSphere& operator+=(const TSphere& Other) UE_STUB_BODY
		TSphere operator+(const TSphere& Other) const UE_STUB_BODY
	};
}
