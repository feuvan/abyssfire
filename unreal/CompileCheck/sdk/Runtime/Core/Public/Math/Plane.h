// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/Plane.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"

namespace UE::Math
{
	template <typename T>
	struct TPlane : public TVector<T>
	{
		using FReal = T;
		T W;
		TPlane() = default;
		TPlane(const TVector4<T>& V) UE_STUB_BODY
		TPlane(T InX, T InY, T InZ, T InW) UE_STUB_BODY
		TPlane(TVector<T> InNormal, T InW) UE_STUB_BODY
		TPlane(TVector<T> InBase, const TVector<T>& InNormal) UE_STUB_BODY
		TPlane(TVector<T> A, TVector<T> B, TVector<T> C) UE_STUB_BODY
		explicit TPlane(EForceInit) UE_STUB_BODY
		bool IsValid() const UE_STUB_BODY
		TVector<T> GetOrigin() const UE_STUB_BODY
		const TVector<T>& GetNormal() const UE_STUB_BODY
		T PlaneDot(const TVector<T>& P) const UE_STUB_BODY
		bool Normalize(T Tolerance = UE_SMALL_NUMBER) UE_STUB_BODY
		TPlane Flip() const UE_STUB_BODY
		TPlane TransformBy(const TMatrix<T>& M) const UE_STUB_BODY
		bool Equals(const TPlane& V, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
	};
}
