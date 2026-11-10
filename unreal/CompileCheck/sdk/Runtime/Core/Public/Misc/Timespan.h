// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/Timespan.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"

namespace ETimespan
{
	inline constexpr int64 MaxTicks = 9223372036854775807;
	inline constexpr int64 MinTicks = -9223372036854775807 - 1;
	inline constexpr int64 NanosecondsPerTick = 100;
	inline constexpr int64 TicksPerDay = 864000000000;
	inline constexpr int64 TicksPerHour = 36000000000;
	inline constexpr int64 TicksPerMicrosecond = 10;
	inline constexpr int64 TicksPerMillisecond = 10000;
	inline constexpr int64 TicksPerMinute = 600000000;
	inline constexpr int64 TicksPerSecond = 10000000;
	inline constexpr int64 TicksPerWeek = 6048000000000;
	inline constexpr int64 TicksPerYear = 365 * TicksPerDay;
}

struct FTimespan
{
public:
	constexpr FTimespan() : Ticks(0) {}
	constexpr FTimespan(int64 InTicks) : Ticks(InTicks) {}
	FTimespan(int32 Hours, int32 Minutes, int32 Seconds);
	FTimespan(int32 Days, int32 Hours, int32 Minutes, int32 Seconds);
	FTimespan(int32 Days, int32 Hours, int32 Minutes, int32 Seconds, int32 FractionNano);
	FTimespan operator+(const FTimespan& Other) const;
	FTimespan& operator+=(const FTimespan& Other);
	FTimespan operator-() const;
	FTimespan operator-(const FTimespan& Other) const;
	FTimespan& operator-=(const FTimespan& Other);
	FTimespan operator*(double Scalar) const;
	FTimespan& operator*=(double Scalar);
	FTimespan operator/(double Scalar) const;
	FTimespan& operator/=(double Scalar);
	bool operator==(const FTimespan& Other) const;
	bool operator!=(const FTimespan& Other) const;
	bool operator>(const FTimespan& Other) const;
	bool operator>=(const FTimespan& Other) const;
	bool operator<(const FTimespan& Other) const;
	bool operator<=(const FTimespan& Other) const;
	int32 GetDays() const;
	FTimespan GetDuration();
	int32 GetHours() const;
	int32 GetMinutes() const;
	int32 GetSeconds() const;
	int32 GetFractionMilli() const;
	int32 GetFractionMicro() const;
	int32 GetFractionNano() const;
	int64 GetTicks() const;
	double GetTotalDays() const;
	double GetTotalHours() const;
	double GetTotalMicroseconds() const;
	double GetTotalMilliseconds() const;
	double GetTotalMinutes() const;
	double GetTotalSeconds() const;
	bool IsZero() const;
	FString ToString() const;
	FString ToString(const TCHAR* Format) const;
	static FTimespan FromDays(double Days);
	static FTimespan FromHours(double Hours);
	static FTimespan FromMicroseconds(double Microseconds);
	static FTimespan FromMilliseconds(double Milliseconds);
	static FTimespan FromMinutes(double Minutes);
	static FTimespan FromSeconds(double Seconds);
	static FTimespan MaxValue();
	static FTimespan MinValue();
	static bool Parse(const FString& TimespanString, FTimespan& OutTimespan);
	static FTimespan Zero();
private:
	int64 Ticks;
};
FTimespan operator*(float Scalar, const FTimespan& Timespan);
uint32 GetTypeHash(const FTimespan& Timespan);
