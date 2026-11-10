// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/DateTime.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/Timespan.h"

enum class EDayOfWeek { Monday = 0, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday };
enum class EMonthOfYear { January = 1, February, March, April, May, June, July, August, September, October, November, December };

struct FDateTime
{
public:
	FDateTime() : Ticks(0) {}
	FDateTime(int64 InTicks) : Ticks(InTicks) {}
	FDateTime(int32 Year, int32 Month, int32 Day, int32 Hour = 0, int32 Minute = 0, int32 Second = 0, int32 Millisecond = 0);
	FDateTime operator+(const FTimespan& Other) const;
	FDateTime& operator+=(const FTimespan& Other);
	FDateTime operator+(const FDateTime& Other) const;
	FTimespan operator-(const FDateTime& Other) const;
	FDateTime operator-(const FTimespan& Other) const;
	FDateTime& operator-=(const FTimespan& Other);
	bool operator==(const FDateTime& Other) const;
	bool operator!=(const FDateTime& Other) const;
	bool operator>(const FDateTime& Other) const;
	bool operator>=(const FDateTime& Other) const;
	bool operator<(const FDateTime& Other) const;
	bool operator<=(const FDateTime& Other) const;
	FDateTime GetDate() const;
	void GetDate(int32& OutYear, int32& OutMonth, int32& OutDay) const;
	int32 GetDay() const;
	EDayOfWeek GetDayOfWeek() const;
	int32 GetDayOfYear() const;
	int32 GetHour() const;
	int32 GetHour12() const;
	double GetJulianDay() const;
	int32 GetMillisecond() const;
	int32 GetMinute() const;
	int32 GetMonth() const;
	EMonthOfYear GetMonthOfYear() const;
	int32 GetSecond() const;
	int64 GetTicks() const;
	FTimespan GetTimeOfDay() const;
	int32 GetYear() const;
	bool IsAfternoon() const;
	bool IsMorning() const;
	FString ToHttpDate() const;
	FString ToIso8601() const;
	FString ToString() const;
	FString ToString(const TCHAR* Format) const;
	FString ToFormattedString(const TCHAR* Format) const;
	int64 ToUnixTimestamp() const;
	double ToUnixTimestampDecimal() const;
	static int32 DaysInMonth(int32 Year, int32 Month);
	static int32 DaysInYear(int32 Year);
	static FDateTime FromJulianDay(double JulianDay);
	static FDateTime FromUnixTimestamp(int64 UnixTime);
	static FDateTime FromUnixTimestampDecimal(double UnixTime);
	static bool IsLeapYear(int32 Year);
	static FDateTime MaxValue();
	static FDateTime MinValue();
	static FDateTime Now();
	static bool Parse(const FString& DateTimeString, FDateTime& OutDateTime);
	static bool ParseHttpDate(const FString& HttpDate, FDateTime& OutDateTime);
	static bool ParseIso8601(const TCHAR* DateTimeString, FDateTime& OutDateTime);
	static FDateTime Today();
	static FDateTime UtcNow();
	static bool Validate(int32 Year, int32 Month, int32 Day, int32 Hour, int32 Minute, int32 Second, int32 Millisecond);
private:
	int64 Ticks;
};
uint32 GetTypeHash(const FDateTime& DateTime);
