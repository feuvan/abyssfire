// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/Rotator.h (from UE 5.x source knowledge).
// TRotator: (Pitch, Yaw, Roll) in degrees - note the constructor order.
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"

namespace UE::Math
{
	template <typename T>
	struct TRotator
	{
		using FReal = T;
		T Pitch;
		T Yaw;
		T Roll;
		static const TRotator<T> ZeroRotator;
		TRotator() = default;
		explicit constexpr TRotator(T InF) : Pitch(InF), Yaw(InF), Roll(InF) {}
		constexpr TRotator(T InPitch, T InYaw, T InRoll) : Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}
		explicit constexpr TRotator(EForceInit) : Pitch(0), Yaw(0), Roll(0) {}
		explicit TRotator(const TQuat<T>& Quat) UE_STUB_BODY
		template <typename FArg> requires (!std::is_same_v<T, FArg>)
		explicit TRotator(const TRotator<FArg>& From) UE_STUB_BODY
		TRotator operator+(const TRotator& R) const UE_STUB_BODY
		TRotator operator-(const TRotator& R) const UE_STUB_BODY
		template <typename FArg> requires std::is_arithmetic_v<FArg> TRotator operator*(FArg Scale) const UE_STUB_BODY
		template <typename FArg> requires std::is_arithmetic_v<FArg> TRotator operator*=(FArg Scale) UE_STUB_BODY
		bool operator==(const TRotator& R) const UE_STUB_BODY
		bool operator!=(const TRotator& V) const UE_STUB_BODY
		TRotator operator+=(const TRotator& R) UE_STUB_BODY
		TRotator operator-=(const TRotator& R) UE_STUB_BODY
		bool IsNearlyZero(T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		bool IsZero() const UE_STUB_BODY
		bool Equals(const TRotator& R, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		bool EqualsOrientation(const TRotator& R, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		TRotator Add(T DeltaPitch, T DeltaYaw, T DeltaRoll) UE_STUB_BODY
		TRotator GetInverse() const UE_STUB_BODY
		TRotator GridSnap(const TRotator& RotGrid) const UE_STUB_BODY
		TVector<T> Vector() const UE_STUB_BODY
		TQuat<T> Quaternion() const UE_STUB_BODY
		TVector<T> Euler() const UE_STUB_BODY
		TVector<T> RotateVector(const TVector<T>& V) const UE_STUB_BODY
		TVector<T> UnrotateVector(const TVector<T>& V) const UE_STUB_BODY
		TRotator Clamp() const UE_STUB_BODY
		TRotator GetNormalized() const UE_STUB_BODY
		TRotator GetDenormalized() const UE_STUB_BODY
		T GetComponentForAxis(EAxis::Type Axis) const UE_STUB_BODY
		void SetComponentForAxis(EAxis::Type Axis, T Component) UE_STUB_BODY
		void Normalize() UE_STUB_BODY
		void GetWindingAndRemainder(TRotator& Winding, TRotator& Remainder) const UE_STUB_BODY
		T GetManhattanDistance(const TRotator& Rotator) const UE_STUB_BODY
		TRotator GetEquivalentRotator() const UE_STUB_BODY
		FString ToString() const UE_STUB_BODY
		FString ToCompactString() const UE_STUB_BODY
		bool InitFromString(const FString& InSourceString) UE_STUB_BODY
		bool ContainsNaN() const UE_STUB_BODY
		static T ClampAxis(T Angle) UE_STUB_BODY
		static T NormalizeAxis(T Angle) UE_STUB_BODY
		static uint8 CompressAxisToByte(T Angle) UE_STUB_BODY
		static T DecompressAxisFromByte(uint8 Angle) UE_STUB_BODY
		static TRotator MakeFromEuler(const TVector<T>& Euler) UE_STUB_BODY
	};
	template <typename T, typename FArg> requires std::is_arithmetic_v<FArg>
	TRotator<T> operator*(FArg Scale, const TRotator<T>& R) UE_STUB_BODY
}
template <typename T> struct TCustomLerp<UE::Math::TRotator<T>> { enum { Value = true }; };
