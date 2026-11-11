// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/Vector.h (from UE 5.x source knowledge).
// TVector<T>: FVector = TVector<double>, FVector3f = TVector<float> (explicit conversion between them).
// Value-returning const / static members are [[nodiscard]] as in the engine (UE_NODISCARD since 5.1): GetSafeNormal()
// as a statement is an error in a UE build; Normalize() is the mutating form.
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Axis.h"
#include "Math/IntPoint.h"
#include "Math/IntVector.h"
#include "Math/Vector2D.h"
#include "Math/Color.h"
#include "Containers/UnrealString.h"
#include "Templates/TypeHash.h"

namespace UE::Math
{
	template <typename T>
	struct TVector
	{
		static_assert(std::is_floating_point_v<T>, "T must be floating point");
		using FReal = T;
		T X;
		T Y;
		T Z;
		static const TVector<T> ZeroVector;
		static const TVector<T> OneVector;
		static const TVector<T> UpVector;
		static const TVector<T> DownVector;
		static const TVector<T> ForwardVector;
		static const TVector<T> BackwardVector;
		static const TVector<T> RightVector;
		static const TVector<T> LeftVector;
		static const TVector<T> XAxisVector;
		static const TVector<T> YAxisVector;
		static const TVector<T> ZAxisVector;
		[[nodiscard]] static TVector<T> Zero() { return TVector((T)0, (T)0, (T)0); }
		[[nodiscard]] static TVector<T> One() { return TVector((T)1, (T)1, (T)1); }
		[[nodiscard]] static TVector<T> UnitX() { return TVector((T)1, (T)0, (T)0); }
		[[nodiscard]] static TVector<T> UnitY() { return TVector((T)0, (T)1, (T)0); }
		[[nodiscard]] static TVector<T> UnitZ() { return TVector((T)0, (T)0, (T)1); }

		TVector() = default;
		explicit constexpr TVector(T InF) : X(InF), Y(InF), Z(InF) {}
		constexpr TVector(T InX, T InY, T InZ) : X(InX), Y(InY), Z(InZ) {}
		explicit constexpr TVector(const TVector2<T> V, T InZ) : X(V.X), Y(V.Y), Z(InZ) {}
		TVector(const TVector4<T>& V) UE_STUB_BODY
		explicit TVector(const FLinearColor& InColor) UE_STUB_BODY
		template <typename IntType> explicit TVector(TIntVector3<IntType> InVector) UE_STUB_BODY
		template <typename IntType> explicit TVector(TIntPoint<IntType> A) UE_STUB_BODY
		explicit constexpr TVector(EForceInit) : X(0), Y(0), Z(0) {}
		template <typename FArg> requires (!std::is_same_v<T, FArg>)
		explicit constexpr TVector(const TVector<FArg>& From) : X((T)From.X), Y((T)From.Y), Z((T)From.Z) {}

		[[nodiscard]] constexpr TVector operator^(const TVector& V) const { return TVector(Y * V.Z - Z * V.Y, Z * V.X - X * V.Z, X * V.Y - Y * V.X); }
		[[nodiscard]] TVector Cross(const TVector& V2) const UE_STUB_BODY
		[[nodiscard]] static TVector CrossProduct(const TVector& A, const TVector& B) UE_STUB_BODY
		[[nodiscard]] constexpr T operator|(const TVector& V) const { return X * V.X + Y * V.Y + Z * V.Z; }
		[[nodiscard]] T Dot(const TVector& V) const UE_STUB_BODY
		[[nodiscard]] static T DotProduct(const TVector& A, const TVector& B) UE_STUB_BODY
		[[nodiscard]] constexpr TVector operator+(const TVector& V) const { return TVector(X + V.X, Y + V.Y, Z + V.Z); }
		[[nodiscard]] constexpr TVector operator-(const TVector& V) const { return TVector(X - V.X, Y - V.Y, Z - V.Z); }
		template <typename FArg> requires std::is_arithmetic_v<FArg>
		[[nodiscard]] constexpr TVector operator-(FArg Bias) const { return TVector(X - (T)Bias, Y - (T)Bias, Z - (T)Bias); }
		template <typename FArg> requires std::is_arithmetic_v<FArg>
		[[nodiscard]] constexpr TVector operator+(FArg Bias) const { return TVector(X + (T)Bias, Y + (T)Bias, Z + (T)Bias); }
		template <typename FArg> requires std::is_arithmetic_v<FArg>
		[[nodiscard]] constexpr TVector operator*(FArg Scale) const { return TVector(X * (T)Scale, Y * (T)Scale, Z * (T)Scale); }
		template <typename FArg> requires std::is_arithmetic_v<FArg>
		[[nodiscard]] constexpr TVector operator/(FArg Scale) const { return TVector(X / (T)Scale, Y / (T)Scale, Z / (T)Scale); }
		[[nodiscard]] constexpr TVector operator*(const TVector& V) const { return TVector(X * V.X, Y * V.Y, Z * V.Z); }
		[[nodiscard]] constexpr TVector operator/(const TVector& V) const { return TVector(X / V.X, Y / V.Y, Z / V.Z); }
		[[nodiscard]] constexpr bool operator==(const TVector& V) const { return X == V.X && Y == V.Y && Z == V.Z; }
		[[nodiscard]] constexpr bool operator!=(const TVector& V) const { return X != V.X || Y != V.Y || Z != V.Z; }
		[[nodiscard]] bool Equals(const TVector& V, T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		[[nodiscard]] bool AllComponentsEqual(T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		[[nodiscard]] constexpr TVector operator-() const { return TVector(-X, -Y, -Z); }
		TVector& operator+=(const TVector& V) { X += V.X; Y += V.Y; Z += V.Z; return *this; }
		TVector& operator-=(const TVector& V) { X -= V.X; Y -= V.Y; Z -= V.Z; return *this; }
		template <typename FArg> requires std::is_arithmetic_v<FArg>
		TVector& operator*=(FArg Scale) { X *= (T)Scale; Y *= (T)Scale; Z *= (T)Scale; return *this; }
		template <typename FArg> requires std::is_arithmetic_v<FArg>
		TVector& operator/=(FArg Scale) { X /= (T)Scale; Y /= (T)Scale; Z /= (T)Scale; return *this; }
		TVector& operator*=(const TVector& V) { X *= V.X; Y *= V.Y; Z *= V.Z; return *this; }
		TVector& operator/=(const TVector& V) { X /= V.X; Y /= V.Y; Z /= V.Z; return *this; }
		T& operator[](int32 Index) UE_STUB_BODY
		T operator[](int32 Index) const UE_STUB_BODY
		T& Component(int32 Index) UE_STUB_BODY
		[[nodiscard]] T Component(int32 Index) const UE_STUB_BODY
		[[nodiscard]] T GetComponentForAxis(EAxis::Type Axis) const UE_STUB_BODY
		void SetComponentForAxis(EAxis::Type Axis, T Component) UE_STUB_BODY
		void Set(T InX, T InY, T InZ) UE_STUB_BODY
		[[nodiscard]] T GetMax() const UE_STUB_BODY
		[[nodiscard]] T GetAbsMax() const UE_STUB_BODY
		[[nodiscard]] T GetMin() const UE_STUB_BODY
		[[nodiscard]] T GetAbsMin() const UE_STUB_BODY
		[[nodiscard]] TVector ComponentMin(const TVector& Other) const UE_STUB_BODY
		[[nodiscard]] TVector ComponentMax(const TVector& Other) const UE_STUB_BODY
		[[nodiscard]] TVector GetAbs() const UE_STUB_BODY
		[[nodiscard]] static TVector Max(const TVector& A, const TVector& B) UE_STUB_BODY
		[[nodiscard]] static TVector Min(const TVector& A, const TVector& B) UE_STUB_BODY
		[[nodiscard]] T Size() const UE_STUB_BODY
		[[nodiscard]] T Length() const UE_STUB_BODY
		[[nodiscard]] T SizeSquared() const UE_STUB_BODY
		[[nodiscard]] T SquaredLength() const UE_STUB_BODY
		[[nodiscard]] T Size2D() const UE_STUB_BODY
		[[nodiscard]] T SizeSquared2D() const UE_STUB_BODY
		[[nodiscard]] bool IsNearlyZero(T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		[[nodiscard]] bool IsZero() const UE_STUB_BODY
		[[nodiscard]] bool IsUnit(T LengthSquaredTolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		[[nodiscard]] bool IsNormalized() const UE_STUB_BODY
		bool Normalize(T Tolerance = UE_SMALL_NUMBER) UE_STUB_BODY
		[[nodiscard]] TVector GetUnsafeNormal() const UE_STUB_BODY
		[[nodiscard]] TVector GetSafeNormal(T Tolerance = UE_SMALL_NUMBER, const TVector& ResultIfZero = ZeroVector) const UE_STUB_BODY
		[[nodiscard]] TVector GetSafeNormal2D(T Tolerance = UE_SMALL_NUMBER, const TVector& ResultIfZero = ZeroVector) const UE_STUB_BODY
		[[nodiscard]] TVector GetUnsafeNormal2D() const UE_STUB_BODY
		void ToDirectionAndLength(TVector& OutDir, double& OutLength) const UE_STUB_BODY
		void ToDirectionAndLength(TVector& OutDir, float& OutLength) const UE_STUB_BODY
		[[nodiscard]] TVector GetSignVector() const UE_STUB_BODY
		[[nodiscard]] TVector Projection() const UE_STUB_BODY
		[[nodiscard]] TVector GridSnap(const T& GridSz) const UE_STUB_BODY
		[[nodiscard]] TVector BoundToCube(T Radius) const UE_STUB_BODY
		[[nodiscard]] TVector BoundToBox(const TVector& Min, const TVector& Max) const UE_STUB_BODY
		[[nodiscard]] TVector GetClampedToSize(T Min, T Max) const UE_STUB_BODY
		[[nodiscard]] TVector GetClampedToSize2D(T Min, T Max) const UE_STUB_BODY
		[[nodiscard]] TVector GetClampedToMaxSize(T MaxSize) const UE_STUB_BODY
		[[nodiscard]] TVector GetClampedToMaxSize2D(T MaxSize) const UE_STUB_BODY
		void AddBounded(const TVector& V, T Radius = MAX_int16) UE_STUB_BODY
		[[nodiscard]] TVector Reciprocal() const UE_STUB_BODY
		[[nodiscard]] bool IsUniform(T Tolerance = UE_KINDA_SMALL_NUMBER) const UE_STUB_BODY
		[[nodiscard]] TVector MirrorByVector(const TVector& MirrorNormal) const UE_STUB_BODY
		[[nodiscard]] TVector MirrorByPlane(const TPlane<T>& Plane) const UE_STUB_BODY
		[[nodiscard]] TVector RotateAngleAxis(const T AngleDeg, const TVector& Axis) const UE_STUB_BODY
		[[nodiscard]] TVector RotateAngleAxisRad(const T AngleRad, const TVector& Axis) const UE_STUB_BODY
		[[nodiscard]] T CosineAngle2D(TVector B) const UE_STUB_BODY
		[[nodiscard]] TVector ProjectOnTo(const TVector& A) const UE_STUB_BODY
		[[nodiscard]] TVector ProjectOnToNormal(const TVector& Normal) const UE_STUB_BODY
		[[nodiscard]] TRotator<T> ToOrientationRotator() const UE_STUB_BODY
		[[nodiscard]] TQuat<T> ToOrientationQuat() const UE_STUB_BODY
		[[nodiscard]] TRotator<T> Rotation() const UE_STUB_BODY
		void FindBestAxisVectors(TVector& Axis1, TVector& Axis2) const UE_STUB_BODY
		void UnwindEuler() UE_STUB_BODY
		[[nodiscard]] bool ContainsNaN() const UE_STUB_BODY
		[[nodiscard]] FString ToString() const UE_STUB_BODY
		[[nodiscard]] FText ToText() const UE_STUB_BODY
		[[nodiscard]] FString ToCompactString() const UE_STUB_BODY
		bool InitFromString(const FString& InSourceString) UE_STUB_BODY
		[[nodiscard]] TVector2<T> UnitCartesianToSpherical() const UE_STUB_BODY
		[[nodiscard]] T HeadingAngle() const UE_STUB_BODY
		static void CreateOrthonormalBasis(TVector& XAxis, TVector& YAxis, TVector& ZAxis) UE_STUB_BODY
		[[nodiscard]] static bool PointsAreSame(const TVector& P, const TVector& Q) UE_STUB_BODY
		[[nodiscard]] static bool PointsAreNear(const TVector& Point1, const TVector& Point2, T Dist) UE_STUB_BODY
		[[nodiscard]] static T PointPlaneDist(const TVector& Point, const TVector& PlaneBase, const TVector& PlaneNormal) UE_STUB_BODY
		[[nodiscard]] static TVector PointPlaneProject(const TVector& Point, const TPlane<T>& Plane) UE_STUB_BODY
		[[nodiscard]] static TVector PointPlaneProject(const TVector& Point, const TVector& PlaneBase, const TVector& PlaneNormal) UE_STUB_BODY
		[[nodiscard]] static TVector VectorPlaneProject(const TVector& V, const TVector& PlaneNormal) UE_STUB_BODY
		[[nodiscard]] static T Dist(const TVector& V1, const TVector& V2) UE_STUB_BODY
		[[nodiscard]] static T Distance(const TVector& V1, const TVector& V2) { return Dist(V1, V2); }
		[[nodiscard]] static T DistXY(const TVector& V1, const TVector& V2) UE_STUB_BODY
		[[nodiscard]] static T Dist2D(const TVector& V1, const TVector& V2) { return DistXY(V1, V2); }
		[[nodiscard]] static T DistSquared(const TVector& V1, const TVector& V2) UE_STUB_BODY
		[[nodiscard]] static T DistSquaredXY(const TVector& V1, const TVector& V2) UE_STUB_BODY
		[[nodiscard]] static T DistSquared2D(const TVector& V1, const TVector& V2) { return DistSquaredXY(V1, V2); }
		[[nodiscard]] static T BoxPushOut(const TVector& Normal, const TVector& Size) UE_STUB_BODY
		[[nodiscard]] static bool Parallel(const TVector& Normal1, const TVector& Normal2, T ParallelCosineThreshold = UE_THRESH_NORMALS_ARE_PARALLEL_STUB) UE_STUB_BODY
		[[nodiscard]] static bool Coincident(const TVector& Normal1, const TVector& Normal2, T ParallelCosineThreshold = UE_THRESH_NORMALS_ARE_PARALLEL_STUB) UE_STUB_BODY
		[[nodiscard]] static bool Orthogonal(const TVector& Normal1, const TVector& Normal2, T OrthogonalCosineThreshold = UE_THRESH_NORMALS_ARE_ORTHOGONAL_STUB) UE_STUB_BODY
		[[nodiscard]] static T Triple(const TVector& X, const TVector& Y, const TVector& Z) UE_STUB_BODY
		[[nodiscard]] static TVector RadiansToDegrees(const TVector& RadVector) UE_STUB_BODY
		[[nodiscard]] static TVector DegreesToRadians(const TVector& DegVector) UE_STUB_BODY
		[[nodiscard]] static TVector SlerpVectorToDirection(TVector& V, TVector& Direction, T Alpha) UE_STUB_BODY
		[[nodiscard]] static TVector SlerpNormals(TVector& NormalA, TVector& NormalB, T Alpha) UE_STUB_BODY
	private:
		static constexpr T UE_THRESH_NORMALS_ARE_PARALLEL_STUB = (T)0.999845;
		static constexpr T UE_THRESH_NORMALS_ARE_ORTHOGONAL_STUB = (T)0.017455;
	};
	template <typename T, typename T2> requires std::is_arithmetic_v<T2>
	constexpr TVector<T> operator*(T2 Scale, const TVector<T>& V) { return V.operator*(Scale); }
	template <typename T> uint32 GetTypeHash(const TVector<T>& Vector) UE_STUB_BODY
}
template <> struct TIsPODType<FVector3f> { enum { Value = true }; };
template <> struct TIsPODType<FVector3d> { enum { Value = true }; };
