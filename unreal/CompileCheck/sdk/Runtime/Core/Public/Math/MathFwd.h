// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/MathFwd.h (from UE 5.x source knowledge).
// Large-world-coordinate aliases: the unsuffixed names are the double variants.
#pragma once

#include "HAL/Platform.h"

namespace UE::Math
{
	template <typename T> struct TVector;
	template <typename T> struct TVector2;
	template <typename T> struct TVector4;
	template <typename T> struct TQuat;
	template <typename T> struct TMatrix;
	template <typename T> struct TPlane;
	template <typename T> struct TTransform;
	template <typename T> struct TSphere;
	template <typename T> struct TBox;
	template <typename T> struct TBox2;
	template <typename T> struct TRotator;
	template <typename T> struct TBoxSphereBounds;
	template <typename IntType> struct TIntPoint;
	template <typename IntType> struct TIntVector3;
	template <typename IntType> struct TIntVector4;
	template <typename IntType> struct TIntRect;
	template <typename T> struct TRay;
}

using FVector = UE::Math::TVector<double>;
using FVector2D = UE::Math::TVector2<double>;
using FVector4 = UE::Math::TVector4<double>;
using FQuat = UE::Math::TQuat<double>;
using FMatrix = UE::Math::TMatrix<double>;
using FPlane = UE::Math::TPlane<double>;
using FTransform = UE::Math::TTransform<double>;
using FSphere = UE::Math::TSphere<double>;
using FBox = UE::Math::TBox<double>;
using FBox2D = UE::Math::TBox2<double>;
using FRotator = UE::Math::TRotator<double>;
using FRay = UE::Math::TRay<double>;
using FBoxSphereBounds = UE::Math::TBoxSphereBounds<double>;

using FVector3d = UE::Math::TVector<double>;
using FVector2d = UE::Math::TVector2<double>;
using FVector4d = UE::Math::TVector4<double>;
using FQuat4d = UE::Math::TQuat<double>;
using FMatrix44d = UE::Math::TMatrix<double>;
using FPlane4d = UE::Math::TPlane<double>;
using FTransform3d = UE::Math::TTransform<double>;
using FSphere3d = UE::Math::TSphere<double>;
using FBox3d = UE::Math::TBox<double>;
using FBox2d = UE::Math::TBox2<double>;
using FRotator3d = UE::Math::TRotator<double>;
using FBoxSphereBounds3d = UE::Math::TBoxSphereBounds<double>;

using FVector3f = UE::Math::TVector<float>;
using FVector2f = UE::Math::TVector2<float>;
using FVector4f = UE::Math::TVector4<float>;
using FQuat4f = UE::Math::TQuat<float>;
using FMatrix44f = UE::Math::TMatrix<float>;
using FPlane4f = UE::Math::TPlane<float>;
using FTransform3f = UE::Math::TTransform<float>;
using FSphere3f = UE::Math::TSphere<float>;
using FBox3f = UE::Math::TBox<float>;
using FBox2f = UE::Math::TBox2<float>;
using FRotator3f = UE::Math::TRotator<float>;
using FBoxSphereBounds3f = UE::Math::TBoxSphereBounds<float>;

using FIntPoint = UE::Math::TIntPoint<int32>;
using FInt32Point = UE::Math::TIntPoint<int32>;
using FInt64Point = UE::Math::TIntPoint<int64>;
using FUintPoint = UE::Math::TIntPoint<uint32>;
using FIntVector = UE::Math::TIntVector3<int32>;
using FIntVector3 = UE::Math::TIntVector3<int32>;
using FIntVector4 = UE::Math::TIntVector4<int32>;
using FIntRect = UE::Math::TIntRect<int32>;

struct FColor;
struct FLinearColor;
struct FRandomStream;
struct FMath;
