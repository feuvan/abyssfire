// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/EnumAsByte.h (from UE 5.x source knowledge).
// TEnumAsByte is only for unscoped enums: an enum class is a compile error ("TEnumAsByte is not intended for use with
// enum classes").
#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTypeTraits.h"

template <class InEnumType>
class TEnumAsByte
{
	static_assert(std::is_enum_v<InEnumType> && std::is_convertible_v<InEnumType, int>, "TEnumAsByte is not intended for use with enum classes - please derive your enum class from uint8 instead.");
public:
	using EnumType = InEnumType;
	TEnumAsByte() = default;
	TEnumAsByte(const TEnumAsByte&) = default;
	TEnumAsByte& operator=(const TEnumAsByte&) = default;
	TEnumAsByte(EnumType InValue) : Value(static_cast<uint8>(InValue)) {}
	explicit TEnumAsByte(int32 InValue) : Value(static_cast<uint8>(InValue)) {}
	explicit TEnumAsByte(uint8 InValue) : Value(InValue) {}
	bool operator==(EnumType InValue) const { return static_cast<EnumType>(Value) == InValue; }
	bool operator==(TEnumAsByte InValue) const { return Value == InValue.Value; }
	operator EnumType() const { return (EnumType)Value; }
	EnumType GetValue() const { return (EnumType)Value; }
	uint8 GetIntValue() const { return Value; }
private:
	uint8 Value;
};
template <class T> uint32 GetTypeHash(const TEnumAsByte<T>& Enum) UE_STUB_BODY
