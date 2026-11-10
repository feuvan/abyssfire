// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/NumericLimits.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include <limits>

#define MIN_uint8 ((uint8)0x00)
#define MIN_uint16 ((uint16)0x0000)
#define MIN_uint32 ((uint32)0x00000000)
#define MIN_uint64 ((uint64)0x0000000000000000)
#define MIN_int8 ((int8)-128)
#define MIN_int16 ((int16)-32768)
#define MIN_int32 ((int32)0x80000000)
#define MIN_int64 ((int64)0x8000000000000000)
#define MAX_uint8 ((uint8)0xff)
#define MAX_uint16 ((uint16)0xffff)
#define MAX_uint32 ((uint32)0xffffffff)
#define MAX_uint64 ((uint64)0xffffffffffffffff)
#define MAX_int8 ((int8)0x7f)
#define MAX_int16 ((int16)0x7fff)
#define MAX_int32 ((int32)0x7fffffff)
#define MAX_int64 ((int64)0x7fffffffffffffff)
#define MIN_flt (1.175494351e-38F)
#define MAX_flt (3.402823466e+38F)
#define MIN_dbl (2.2250738585072014e-308)
#define MAX_dbl (1.7976931348623158e+308)

template <typename NumericType> struct TNumericLimits;
template <typename NumericType> struct TNumericLimits<const NumericType> : public TNumericLimits<NumericType> {};
namespace UE::Stub
{
	template <typename NumericType>
	struct TNumericLimitsImpl
	{
		using Type = NumericType;
		static constexpr NumericType Min() { return std::numeric_limits<NumericType>::min(); }
		static constexpr NumericType Max() { return std::numeric_limits<NumericType>::max(); }
		static constexpr NumericType Lowest() { return std::numeric_limits<NumericType>::lowest(); }
	};
}
template <> struct TNumericLimits<uint8> : UE::Stub::TNumericLimitsImpl<uint8> {};
template <> struct TNumericLimits<uint16> : UE::Stub::TNumericLimitsImpl<uint16> {};
template <> struct TNumericLimits<uint32> : UE::Stub::TNumericLimitsImpl<uint32> {};
template <> struct TNumericLimits<uint64> : UE::Stub::TNumericLimitsImpl<uint64> {};
template <> struct TNumericLimits<int8> : UE::Stub::TNumericLimitsImpl<int8> {};
template <> struct TNumericLimits<int16> : UE::Stub::TNumericLimitsImpl<int16> {};
template <> struct TNumericLimits<int32> : UE::Stub::TNumericLimitsImpl<int32> {};
template <> struct TNumericLimits<int64> : UE::Stub::TNumericLimitsImpl<int64> {};
template <> struct TNumericLimits<float> : UE::Stub::TNumericLimitsImpl<float> {};
template <> struct TNumericLimits<double> : UE::Stub::TNumericLimitsImpl<double> {};
