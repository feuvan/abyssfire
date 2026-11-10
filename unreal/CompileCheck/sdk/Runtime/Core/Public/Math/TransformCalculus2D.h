// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/TransformCalculus2D.h (+ TransformCalculus.h).
// From UE 5.x source knowledge. 2D transform algebra used by Slate (float based: FScale2D = TScale2<float>, ...).
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/Vector2D.h"

template <typename T>
class TScale2
{
public:
	using FReal = T;
	TScale2() UE_STUB_BODY
	explicit TScale2(T InScale) UE_STUB_BODY
	explicit TScale2(T InScaleX, T InScaleY) UE_STUB_BODY
	template <typename ArgType> explicit TScale2(const UE::Math::TVector2<ArgType>& InScale) UE_STUB_BODY
	template <typename ArgType> UE::Math::TVector2<ArgType> TransformPoint(const UE::Math::TVector2<ArgType>& Point) const UE_STUB_BODY
	template <typename ArgType> UE::Math::TVector2<ArgType> TransformVector(const UE::Math::TVector2<ArgType>& Vector) const UE_STUB_BODY
	TScale2 Concatenate(const TScale2& RHS) const UE_STUB_BODY
	TScale2 Inverse() const UE_STUB_BODY
	bool operator==(const TScale2& Other) const UE_STUB_BODY
	bool operator!=(const TScale2& Other) const UE_STUB_BODY
	const UE::Math::TVector2<T>& GetVector() const UE_STUB_BODY
};
using FScale2f = TScale2<float>;
using FScale2d = TScale2<double>;
using FScale2D = FScale2f;

template <typename T>
class TShear2
{
public:
	TShear2() UE_STUB_BODY
	explicit TShear2(T ShearX, T ShearY) UE_STUB_BODY
	template <typename VType> explicit TShear2(const UE::Math::TVector2<VType>& InShear) UE_STUB_BODY
	static TShear2 FromShearAngles(const UE::Math::TVector2<T>& InShearAngles) UE_STUB_BODY
	const UE::Math::TVector2<T>& GetVector() const UE_STUB_BODY
};
using FShear2f = TShear2<float>;
using FShear2D = FShear2f;

template <typename T>
class TQuat2
{
public:
	TQuat2() UE_STUB_BODY
	explicit TQuat2(T RotRadians) UE_STUB_BODY
	template <typename VType> explicit TQuat2(const UE::Math::TVector2<VType>& InRot) UE_STUB_BODY
	template <typename ArgType> UE::Math::TVector2<ArgType> TransformPoint(const UE::Math::TVector2<ArgType>& Point) const UE_STUB_BODY
	TQuat2 Concatenate(const TQuat2& RHS) const UE_STUB_BODY
	TQuat2 Inverse() const UE_STUB_BODY
	const UE::Math::TVector2<T>& GetVector() const UE_STUB_BODY
};
using FQuat2f = TQuat2<float>;
using FQuat2D = FQuat2f;

template <typename T>
class TMatrix2x2
{
public:
	TMatrix2x2() UE_STUB_BODY
	TMatrix2x2(T m00, T m01, T m10, T m11) UE_STUB_BODY
	explicit TMatrix2x2(T UniformScale) UE_STUB_BODY
	explicit TMatrix2x2(const TScale2<T>& Scale) UE_STUB_BODY
	explicit TMatrix2x2(const TShear2<T>& Shear) UE_STUB_BODY
	explicit TMatrix2x2(const TQuat2<T>& Rotation) UE_STUB_BODY
	template <typename ArgType> UE::Math::TVector2<ArgType> TransformPoint(const UE::Math::TVector2<ArgType>& Point) const UE_STUB_BODY
	template <typename ArgType> UE::Math::TVector2<ArgType> TransformVector(const UE::Math::TVector2<ArgType>& Vector) const UE_STUB_BODY
	TMatrix2x2 Concatenate(const TMatrix2x2& RHS) const UE_STUB_BODY
	TMatrix2x2 Inverse() const UE_STUB_BODY
	T Determinant() const UE_STUB_BODY
	bool operator==(const TMatrix2x2& RHS) const UE_STUB_BODY
	void GetMatrix(float& A, float& B, float& C, float& D) const UE_STUB_BODY
	TScale2<T> GetScale() const UE_STUB_BODY
	bool IsIdentity() const UE_STUB_BODY
};
using FMatrix2x2f = TMatrix2x2<float>;
using FMatrix2x2 = FMatrix2x2f;

template <typename T>
class TTransform2
{
public:
	using Vector2Type = UE::Math::TVector2<T>;
	template <typename VType = float> TTransform2(const UE::Math::TVector2<VType>& Translation = UE::Math::TVector2<VType>(0.f, 0.f)) UE_STUB_BODY
	template <typename VType = float> explicit TTransform2(T UniformScale, const UE::Math::TVector2<VType>& Translation = UE::Math::TVector2<VType>(0.f, 0.f)) UE_STUB_BODY
	template <typename VType = float> explicit TTransform2(const TScale2<T>& Scale, const UE::Math::TVector2<VType>& Translation = UE::Math::TVector2<VType>(0.f, 0.f)) UE_STUB_BODY
	template <typename VType = float> explicit TTransform2(const TShear2<T>& Shear, const UE::Math::TVector2<VType>& Translation = UE::Math::TVector2<VType>(0.f, 0.f)) UE_STUB_BODY
	template <typename VType = float> explicit TTransform2(const TQuat2<T>& Rot, const UE::Math::TVector2<VType>& Translation = UE::Math::TVector2<VType>(0.f, 0.f)) UE_STUB_BODY
	template <typename VType = float> explicit TTransform2(const TMatrix2x2<T>& Transform, const UE::Math::TVector2<VType>& Translation = UE::Math::TVector2<VType>(0.f, 0.f)) UE_STUB_BODY
	template <typename VType> UE::Math::TVector2<VType> TransformPoint(const UE::Math::TVector2<VType>& Point) const UE_STUB_BODY
	template <typename VType> UE::Math::TVector2<VType> TransformVector(const UE::Math::TVector2<VType>& Vector) const UE_STUB_BODY
	TTransform2 Concatenate(const TTransform2& RHS) const UE_STUB_BODY
	TTransform2 Inverse() const UE_STUB_BODY
	bool operator==(const TTransform2& Other) const UE_STUB_BODY
	bool operator!=(const TTransform2& Other) const UE_STUB_BODY
	const TMatrix2x2<T>& GetMatrix() const UE_STUB_BODY
	const Vector2Type GetTranslation() const UE_STUB_BODY
	void SetTranslation(const Vector2Type& InTrans) UE_STUB_BODY
	bool IsIdentity() const UE_STUB_BODY
};
using FTransform2f = TTransform2<float>;
using FTransform2d = TTransform2<double>;
using FTransform2D = FTransform2f;

template <typename TransformTypeA, typename TransformTypeB>
auto Concatenate(const TransformTypeA& LHS, const TransformTypeB& RHS) -> decltype(LHS.Concatenate(RHS)) UE_STUB_BODY
template <typename TransformType>
auto Inverse(const TransformType& Transform) -> decltype(Transform.Inverse()) UE_STUB_BODY
/** The generic forms (Math/TransformCalculus.h): they use the member function of the transform type, with their real bodies. */
template <typename TransformType, typename PositionType>
PositionType TransformPoint(const TransformType& Transform, const PositionType& Point) { return Transform.TransformPoint(Point); }
template <typename TransformType, typename VectorType>
VectorType TransformVector(const TransformType& Transform, const VectorType& Vector) { return Transform.TransformVector(Vector); }
