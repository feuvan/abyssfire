// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/Quat.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

namespace UE::Math
{
	template <typename T>
	struct TQuat
	{
		using FReal = T;
		T X, Y, Z, W;
		static const TQuat<T> Identity;
		TQuat() = default;
		explicit TQuat(EForceInit) UE_STUB_BODY
		TQuat(T InX, T InY, T InZ, T InW) UE_STUB_BODY
		explicit TQuat(const TMatrix<T>& M) UE_STUB_BODY
		explicit TQuat(const TRotator<T>& R) UE_STUB_BODY
		TQuat(TVector<T> Axis, T AngleRad) UE_STUB_BODY
		template <typename FArg> requires (!std::is_same_v<T, FArg>)
		explicit TQuat(const TQuat<FArg>& From) UE_STUB_BODY
		[[nodiscard]] TQuat operator+(const TQuat& Q) const UE_STUB_BODY
		TQuat operator+=(const TQuat& Q) UE_STUB_BODY
		[[nodiscard]] TQuat operator-(const TQuat& Q) const UE_STUB_BODY
		[[nodiscard]] TQuat operator-() const UE_STUB_BODY
		[[nodiscard]] bool Equals(const TQuat& Q, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		[[nodiscard]] bool IsIdentity(T Tolerance = UE_SMALL_NUMBER) const UE_STUB_BODY
		TQuat operator-=(const TQuat& Q) UE_STUB_BODY
		[[nodiscard]] TQuat operator*(const TQuat& Q) const UE_STUB_BODY
		TQuat operator*=(const TQuat& Q) UE_STUB_BODY
		[[nodiscard]] TVector<T> operator*(const TVector<T>& V) const UE_STUB_BODY
		[[nodiscard]] TMatrix<T> operator*(const TMatrix<T>& M) const UE_STUB_BODY
		template <typename FArg> requires std::is_arithmetic_v<FArg> TQuat operator*=(const FArg Scale) UE_STUB_BODY
		template <typename FArg> requires std::is_arithmetic_v<FArg> [[nodiscard]] TQuat operator*(const FArg Scale) const UE_STUB_BODY
		template <typename FArg> requires std::is_arithmetic_v<FArg> TQuat operator/=(const FArg Scale) UE_STUB_BODY
		template <typename FArg> requires std::is_arithmetic_v<FArg> [[nodiscard]] TQuat operator/(const FArg Scale) const UE_STUB_BODY
		[[nodiscard]] bool Identical(const TQuat* Q, const uint32 PortFlags) const UE_STUB_BODY
		[[nodiscard]] bool operator==(const TQuat& Q) const UE_STUB_BODY
		[[nodiscard]] bool operator!=(const TQuat& Q) const UE_STUB_BODY
		[[nodiscard]] T operator|(const TQuat& Q) const UE_STUB_BODY
		[[nodiscard]] static TQuat MakeFromEuler(const TVector<T>& Euler) UE_STUB_BODY
		[[nodiscard]] static TQuat MakeFromRotator(const TRotator<T>& R) UE_STUB_BODY
		[[nodiscard]] TVector<T> Euler() const UE_STUB_BODY
		void Normalize(T Tolerance = UE_SMALL_NUMBER) UE_STUB_BODY
		[[nodiscard]] TQuat GetNormalized(T Tolerance = UE_SMALL_NUMBER) const UE_STUB_BODY
		[[nodiscard]] bool IsNormalized() const UE_STUB_BODY
		[[nodiscard]] T Size() const UE_STUB_BODY
		[[nodiscard]] T SizeSquared() const UE_STUB_BODY
		[[nodiscard]] T GetAngle() const UE_STUB_BODY
		void ToAxisAndAngle(TVector<T>& Axis, float& Angle) const UE_STUB_BODY
		void ToAxisAndAngle(TVector<T>& Axis, double& Angle) const UE_STUB_BODY
		[[nodiscard]] TVector<T> ToRotationVector() const UE_STUB_BODY
		[[nodiscard]] static TQuat MakeFromRotationVector(const TVector<T>& RotationVector) UE_STUB_BODY
		void ToSwingTwist(const TVector<T>& InTwistAxis, TQuat& OutSwing, TQuat& OutTwist) const UE_STUB_BODY
		[[nodiscard]] T GetTwistAngle(const TVector<T>& TwistAxis) const UE_STUB_BODY
		[[nodiscard]] TVector<T> RotateVector(TVector<T> V) const UE_STUB_BODY
		[[nodiscard]] TVector<T> UnrotateVector(TVector<T> V) const UE_STUB_BODY
		[[nodiscard]] TQuat Log() const UE_STUB_BODY
		[[nodiscard]] TQuat Exp() const UE_STUB_BODY
		[[nodiscard]] TQuat Inverse() const UE_STUB_BODY
		void EnforceShortestArcWith(const TQuat& OtherQuat) UE_STUB_BODY
		[[nodiscard]] TVector<T> GetAxisX() const UE_STUB_BODY
		[[nodiscard]] TVector<T> GetAxisY() const UE_STUB_BODY
		[[nodiscard]] TVector<T> GetAxisZ() const UE_STUB_BODY
		[[nodiscard]] TVector<T> GetForwardVector() const UE_STUB_BODY
		[[nodiscard]] TVector<T> GetRightVector() const UE_STUB_BODY
		[[nodiscard]] TVector<T> GetUpVector() const UE_STUB_BODY
		[[nodiscard]] TVector<T> Vector() const UE_STUB_BODY
		[[nodiscard]] TRotator<T> Rotator() const UE_STUB_BODY
		[[nodiscard]] TMatrix<T> ToMatrix() const UE_STUB_BODY
		[[nodiscard]] TVector<T> GetRotationAxis() const UE_STUB_BODY
		[[nodiscard]] T AngularDistance(const TQuat& Q) const UE_STUB_BODY
		[[nodiscard]] bool ContainsNaN() const UE_STUB_BODY
		[[nodiscard]] FString ToString() const UE_STUB_BODY
		[[nodiscard]] static TQuat FindBetween(const TVector<T>& Vector1, const TVector<T>& Vector2) UE_STUB_BODY
		[[nodiscard]] static TQuat FindBetweenNormals(const TVector<T>& Normal1, const TVector<T>& Normal2) UE_STUB_BODY
		[[nodiscard]] static TQuat FindBetweenVectors(const TVector<T>& Vector1, const TVector<T>& Vector2) UE_STUB_BODY
		[[nodiscard]] static T Error(const TQuat& Q1, const TQuat& Q2) UE_STUB_BODY
		[[nodiscard]] static TQuat FastLerp(const TQuat& A, const TQuat& B, const T Alpha) UE_STUB_BODY
		[[nodiscard]] static TQuat FastBilerp(const TQuat& P00, const TQuat& P10, const TQuat& P01, const TQuat& P11, T FracX, T FracY) UE_STUB_BODY
		[[nodiscard]] static TQuat Slerp_NotNormalized(const TQuat& Quat1, const TQuat& Quat2, T Slerp) UE_STUB_BODY
		[[nodiscard]] static TQuat Slerp(const TQuat& Quat1, const TQuat& Quat2, T Slerp) UE_STUB_BODY
		[[nodiscard]] static TQuat SlerpFullPath_NotNormalized(const TQuat& quat1, const TQuat& quat2, T Alpha) UE_STUB_BODY
		[[nodiscard]] static TQuat SlerpFullPath(const TQuat& quat1, const TQuat& quat2, T Alpha) UE_STUB_BODY
		[[nodiscard]] static TQuat Squad(const TQuat& quat1, const TQuat& tang1, const TQuat& quat2, const TQuat& tang2, T Alpha) UE_STUB_BODY
	};
	template <typename T, typename FArg> requires std::is_arithmetic_v<FArg>
	TQuat<T> operator*(const FArg Scale, const TQuat<T>& Q) UE_STUB_BODY
}
template <typename T> struct TCustomLerp<UE::Math::TQuat<T>> { enum { Value = true }; };
