// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/Matrix.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Math/Plane.h"
#include "Math/Rotator.h"
#include "Math/Axis.h"

namespace UE::Math
{
	template <typename T>
	struct TMatrix
	{
		using FReal = T;
		alignas(16) T M[4][4];
		static const TMatrix Identity;
		TMatrix() = default;
		explicit TMatrix(EForceInit) UE_STUB_BODY
		TMatrix(const TPlane<T>& InX, const TPlane<T>& InY, const TPlane<T>& InZ, const TPlane<T>& InW) UE_STUB_BODY
		TMatrix(const TVector<T>& InX, const TVector<T>& InY, const TVector<T>& InZ, const TVector<T>& InW) UE_STUB_BODY
		template <typename FArg> requires (!std::is_same_v<T, FArg>)
		explicit TMatrix(const TMatrix<FArg>& From) UE_STUB_BODY
		void SetIdentity() UE_STUB_BODY
		TMatrix operator*(const TMatrix& Other) const UE_STUB_BODY
		void operator*=(const TMatrix& Other) UE_STUB_BODY
		TMatrix operator+(const TMatrix& Other) const UE_STUB_BODY
		void operator+=(const TMatrix& Other) UE_STUB_BODY
		TMatrix operator*(T Other) const UE_STUB_BODY
		void operator*=(T Other) UE_STUB_BODY
		bool operator==(const TMatrix& Other) const UE_STUB_BODY
		bool Equals(const TMatrix& Other, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		bool operator!=(const TMatrix& Other) const UE_STUB_BODY
		TVector4<T> TransformFVector4(const TVector4<T>& V) const UE_STUB_BODY
		TVector4<T> TransformPosition(const TVector<T>& V) const UE_STUB_BODY
		TVector<T> InverseTransformPosition(const TVector<T>& V) const UE_STUB_BODY
		TVector4<T> TransformVector(const TVector<T>& V) const UE_STUB_BODY
		TVector<T> InverseTransformVector(const TVector<T>& V) const UE_STUB_BODY
		TMatrix GetTransposed() const UE_STUB_BODY
		T Determinant() const UE_STUB_BODY
		T RotDeterminant() const UE_STUB_BODY
		TMatrix InverseFast() const UE_STUB_BODY
		TMatrix Inverse() const UE_STUB_BODY
		TMatrix TransposeAdjoint() const UE_STUB_BODY
		void RemoveScaling(T Tolerance = UE_SMALL_NUMBER) UE_STUB_BODY
		TMatrix GetMatrixWithoutScale(T Tolerance = UE_SMALL_NUMBER) const UE_STUB_BODY
		TVector<T> ExtractScaling(T Tolerance = UE_SMALL_NUMBER) UE_STUB_BODY
		TVector<T> GetScaleVector(T Tolerance = UE_SMALL_NUMBER) const UE_STUB_BODY
		TMatrix RemoveTranslation() const UE_STUB_BODY
		TMatrix ConcatTranslation(const TVector<T>& Translation) const UE_STUB_BODY
		bool ContainsNaN() const UE_STUB_BODY
		void ScaleTranslation(const TVector<T>& Scale3D) UE_STUB_BODY
		T GetMinimumAxisScale() const UE_STUB_BODY
		T GetMaximumAxisScale() const UE_STUB_BODY
		TMatrix ApplyScale(T Scale) const UE_STUB_BODY
		TVector<T> GetOrigin() const UE_STUB_BODY
		TVector<T> GetScaledAxis(EAxis::Type Axis) const UE_STUB_BODY
		void GetScaledAxes(TVector<T>& X, TVector<T>& Y, TVector<T>& Z) const UE_STUB_BODY
		TVector<T> GetUnitAxis(EAxis::Type Axis) const UE_STUB_BODY
		void GetUnitAxes(TVector<T>& X, TVector<T>& Y, TVector<T>& Z) const UE_STUB_BODY
		void SetAxis(int32 i, const TVector<T>& Axis) UE_STUB_BODY
		void SetOrigin(const TVector<T>& NewOrigin) UE_STUB_BODY
		void SetAxes(const TVector<T>* Axis0 = nullptr, const TVector<T>* Axis1 = nullptr, const TVector<T>* Axis2 = nullptr, const TVector<T>* Origin = nullptr) UE_STUB_BODY
		TVector<T> GetColumn(int32 i) const UE_STUB_BODY
		void SetColumn(int32 i, TVector<T> Value) UE_STUB_BODY
		TRotator<T> Rotator() const UE_STUB_BODY
		TQuat<T> ToQuat() const UE_STUB_BODY
		FString ToString() const UE_STUB_BODY
	};
	template <typename T>
	struct TRotationTranslationMatrix : public TMatrix<T>
	{
		TRotationTranslationMatrix(const TRotator<T>& Rot, const TVector<T>& Origin) UE_STUB_BODY
		static TMatrix<T> Make(const TRotator<T>& Rot, const TVector<T>& Origin) UE_STUB_BODY
	};
	template <typename T>
	struct TRotationMatrix : public TRotationTranslationMatrix<T>
	{
		TRotationMatrix(const TRotator<T>& Rot) : TRotationTranslationMatrix<T>(Rot, TVector<T>()) UE_STUB_BODY
		static TMatrix<T> Make(TRotator<T> const& Rot) UE_STUB_BODY
		static TMatrix<T> Make(TQuat<T> const& Rot) UE_STUB_BODY
		static TMatrix<T> MakeFromX(TVector<T> const& XAxis) UE_STUB_BODY
		static TMatrix<T> MakeFromY(TVector<T> const& YAxis) UE_STUB_BODY
		static TMatrix<T> MakeFromZ(TVector<T> const& ZAxis) UE_STUB_BODY
		static TMatrix<T> MakeFromXY(TVector<T> const& XAxis, TVector<T> const& YAxis) UE_STUB_BODY
		static TMatrix<T> MakeFromXZ(TVector<T> const& XAxis, TVector<T> const& ZAxis) UE_STUB_BODY
		static TMatrix<T> MakeFromYX(TVector<T> const& YAxis, TVector<T> const& XAxis) UE_STUB_BODY
		static TMatrix<T> MakeFromYZ(TVector<T> const& YAxis, TVector<T> const& ZAxis) UE_STUB_BODY
		static TMatrix<T> MakeFromZX(TVector<T> const& ZAxis, TVector<T> const& XAxis) UE_STUB_BODY
		static TMatrix<T> MakeFromZY(TVector<T> const& ZAxis, TVector<T> const& YAxis) UE_STUB_BODY
	};
	template <typename T> struct TTranslationMatrix : public TMatrix<T> { TTranslationMatrix(const TVector<T>& Delta) UE_STUB_BODY static TMatrix<T> Make(TVector<T> const& Delta) UE_STUB_BODY };
	template <typename T> struct TScaleMatrix : public TMatrix<T> { TScaleMatrix(T Scale) UE_STUB_BODY TScaleMatrix(const TVector<T>& Scale) UE_STUB_BODY static TMatrix<T> Make(T Scale) UE_STUB_BODY static TMatrix<T> Make(const TVector<T>& Scale) UE_STUB_BODY };
}
using FRotationTranslationMatrix = UE::Math::TRotationTranslationMatrix<double>;
using FRotationMatrix = UE::Math::TRotationMatrix<double>;
using FRotationMatrix44f = UE::Math::TRotationMatrix<float>;
using FRotationMatrix44d = UE::Math::TRotationMatrix<double>;
using FTranslationMatrix = UE::Math::TTranslationMatrix<double>;
using FScaleMatrix = UE::Math::TScaleMatrix<double>;
