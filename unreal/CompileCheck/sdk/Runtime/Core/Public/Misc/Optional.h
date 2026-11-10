// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/Optional.h (+ Misc/OptionalFwd.h).
// From UE 5.x source knowledge.
#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"
#include "Misc/AssertionMacros.h"

struct FNullOpt { explicit constexpr FNullOpt(int) {} };
inline constexpr FNullOpt NullOpt{0};

template <typename OptionalType>
struct TOptional
{
public:
	using ElementType = OptionalType;
	constexpr TOptional() : bIsSet(false) {}
	constexpr TOptional(FNullOpt) : bIsSet(false) {}
	TOptional(const OptionalType& InValue) requires std::is_copy_constructible_v<OptionalType> UE_STUB_BODY
	TOptional(OptionalType&& InValue) UE_STUB_BODY
	template <typename... ArgTypes> requires std::is_constructible_v<OptionalType, ArgTypes&&...>
	explicit TOptional(EInPlace, ArgTypes&&... Args) UE_STUB_BODY
	TOptional(const TOptional& Other) : bIsSet(Other.bIsSet) { static_assert(std::is_copy_constructible_v<OptionalType>, "TOptional: copying an optional of a move-only type"); }
	TOptional(TOptional&& Other) : bIsSet(Other.bIsSet) {}
	~TOptional() { static_assert(sizeof(OptionalType) > 0, "TOptional: incomplete element type"); }
	TOptional& operator=(const TOptional& Other) { static_assert(std::is_copy_constructible_v<OptionalType>, "TOptional: copying an optional of a move-only type"); bIsSet = Other.bIsSet; return *this; }
	TOptional& operator=(TOptional&& Other) { bIsSet = Other.bIsSet; return *this; }
	TOptional& operator=(const OptionalType& InValue) requires std::is_copy_constructible_v<OptionalType> UE_STUB_BODY
	TOptional& operator=(OptionalType&& InValue) UE_STUB_BODY
	TOptional& operator=(FNullOpt) UE_STUB_BODY
	void Reset() UE_STUB_BODY
	template <typename... ArgsType> requires std::is_constructible_v<OptionalType, ArgsType&&...>
	OptionalType& Emplace(ArgsType&&... Args) UE_STUB_BODY
	bool IsSet() const { return bIsSet; }
	explicit operator bool() const { return bIsSet; }
	OptionalType& GetValue() UE_STUB_BODY
	const OptionalType& GetValue() const UE_STUB_BODY
	OptionalType* operator->() UE_STUB_BODY
	const OptionalType* operator->() const UE_STUB_BODY
	OptionalType& operator*() UE_STUB_BODY
	const OptionalType& operator*() const UE_STUB_BODY
	const OptionalType& Get(const OptionalType& DefaultValue UE_LIFETIMEBOUND) const UE_LIFETIMEBOUND UE_STUB_BODY
	OptionalType* GetPtrOrNull() UE_STUB_BODY
	const OptionalType* GetPtrOrNull() const UE_STUB_BODY
	bool operator==(const TOptional& Rhs) const UE_STUB_BODY
	bool operator!=(const TOptional& Rhs) const UE_STUB_BODY
	bool operator==(const OptionalType& Rhs) const UE_STUB_BODY
private:
	bool bIsSet;
	alignas(OptionalType) uint8 Storage[sizeof(OptionalType)];
};
template <typename OptionalType> uint32 GetTypeHash(const TOptional<OptionalType>& Optional) UE_STUB_BODY
