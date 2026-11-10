// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Templates/UnrealTemplate.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTypeTraits.h"
#include <initializer_list>
#include <utility>

#define UE_ARRAY_COUNT(array) (sizeof(UE::Core::Private::ArrayCountHelper(array)))
namespace UE::Core::Private
{
	template <typename T, uint32 N> char (&ArrayCountHelper(const T (&)[N]))[N + 1 - 1];
	template <typename T, uint32 N> char (&ArrayCountHelper(std::initializer_list<T>))[1];
}
#define STRUCT_OFFSET(struc, member) __builtin_offsetof(struc, member)

/** MoveTemp: like std::move, but refuses const objects and rvalues (both are compile errors in UE). */
template <typename T>
constexpr std::remove_reference_t<T>&& MoveTemp(T&& Obj) noexcept
{
	using CastType = std::remove_reference_t<T>;
	static_assert(std::is_lvalue_reference_v<T>, "MoveTemp called on an rvalue");
	static_assert(!std::is_same_v<CastType&, const CastType&>, "MoveTemp called on a const object");
	return static_cast<CastType&&>(Obj);
}
template <typename T>
constexpr std::remove_reference_t<T>&& MoveTempIfPossible(T&& Obj) noexcept
{
	return static_cast<std::remove_reference_t<T>&&>(Obj);
}
template <typename T>
constexpr T&& Forward(std::remove_reference_t<T>& Obj) noexcept { return static_cast<T&&>(Obj); }
template <typename T>
constexpr T&& Forward(std::remove_reference_t<T>&& Obj) noexcept { return static_cast<T&&>(Obj); }
template <typename T>
constexpr T CopyTemp(T& Val) { return const_cast<const T&>(Val); }
template <typename T>
constexpr T CopyTemp(const T& Val) { return Val; }
template <typename T>
constexpr T&& CopyTemp(T&& Val) { return static_cast<T&&>(Val); }
template <typename T>
constexpr void Swap(T& A, T& B) { T Temp = static_cast<T&&>(A); A = static_cast<T&&>(B); B = static_cast<T&&>(Temp); }
template <typename T>
constexpr void Exchange(T& A, T& B) { Swap(A, B); }
template <typename T, typename ArgType>
constexpr T StaticCast(ArgType&& Arg) { return static_cast<T>(Arg); }
template <typename T>
std::add_rvalue_reference_t<T> DeclVal() UE_STUB_BODY
template <typename T>
constexpr std::add_const_t<T>& AsConst(T& Ref) { return Ref; }
template <typename T>
void AsConst(const T&& Ref) = delete;

class FNoncopyable
{
protected:
	FNoncopyable() {}
	~FNoncopyable() {}
private:
	FNoncopyable(const FNoncopyable&);
	FNoncopyable& operator=(const FNoncopyable&);
};

/** Sets a value for the scope and restores the previous one. Real signature: TGuardValue<RefType, AssignedType>. */
template <typename RefType, typename AssignedType = RefType>
struct TGuardValue : private FNoncopyable
{
	[[nodiscard]] TGuardValue(RefType& ReferenceValue, const AssignedType& NewValue) UE_STUB_BODY
	~TGuardValue() UE_STUB_BODY
	const AssignedType& GetOriginalValue() const UE_STUB_BODY
private:
	RefType& RefValue;
	AssignedType OriginalValue;
};
template <typename RefType, typename AssignedType> TGuardValue(RefType&, const AssignedType&) -> TGuardValue<RefType, AssignedType>;

template <typename KeyType, typename ValueType>
struct TKeyValuePair
{
	TKeyValuePair(const KeyType& InKey, const ValueType& InValue) : Key(InKey), Value(InValue) {}
	TKeyValuePair(const KeyType& InKey) : Key(InKey) {}
	TKeyValuePair() {}
	bool operator==(const TKeyValuePair& Other) const { return Key == Other.Key; }
	bool operator!=(const TKeyValuePair& Other) const { return Key != Other.Key; }
	bool operator<(const TKeyValuePair& Other) const { return Key < Other.Key; }
	KeyType Key;
	ValueType Value;
};

template <typename T> struct TInPlaceType { explicit TInPlaceType() = default; };
template <typename T> constexpr auto GetData(T&& Container) -> decltype(Container.GetData()) { return Container.GetData(); }
template <typename T, SIZE_T N> constexpr T* GetData(T (&Container)[N]) { return Container; }
template <typename T> constexpr const T* GetData(std::initializer_list<T> List) { return List.begin(); }
template <typename T> constexpr auto GetNum(T&& Container) -> decltype(Container.Num()) { return Container.Num(); }
template <typename T, SIZE_T N> constexpr SIZE_T GetNum(T (&)[N]) { return N; }
template <typename T> constexpr int32 GetNum(std::initializer_list<T> List) { return static_cast<int32>(List.size()); }
namespace UE::Stub
{
	// GetData / GetNum the way the engine's range constructors find them: unqualified, so that overloads declared later
	// (FString's, in UnrealString.h) are found by argument-dependent lookup at instantiation. Used from inside classes
	// that have a GetData member of their own.
	template <typename RangeType> constexpr auto RangeData(RangeType&& Range) -> decltype(GetData(static_cast<RangeType&&>(Range)));
	template <typename RangeType> constexpr auto RangeNum(RangeType&& Range) -> decltype(GetNum(static_cast<RangeType&&>(Range)));
}
template <typename T> constexpr std::underlying_type_t<T> ToUnderlying(T Enum) requires std::is_enum_v<T> { return static_cast<std::underlying_type_t<T>>(Enum); }

template <typename ContainerType, typename ElementType, typename SizeType>
class TIndexedContainerIterator
{
public:
	TIndexedContainerIterator(ContainerType& InContainer, SizeType StartIndex = 0) UE_STUB_BODY
	TIndexedContainerIterator& operator++() UE_STUB_BODY
	TIndexedContainerIterator operator++(int) UE_STUB_BODY
	TIndexedContainerIterator& operator--() UE_STUB_BODY
	TIndexedContainerIterator operator--(int) UE_STUB_BODY
	ElementType& operator*() const UE_STUB_BODY
	ElementType* operator->() const UE_STUB_BODY
	explicit operator bool() const UE_STUB_BODY
	SizeType GetIndex() const UE_STUB_BODY
	void Reset() UE_STUB_BODY
	void SetToEnd() UE_STUB_BODY
	void RemoveCurrent() UE_STUB_BODY
	void RemoveCurrentSwap() UE_STUB_BODY
	bool operator==(const TIndexedContainerIterator& Rhs) const UE_STUB_BODY
	bool operator!=(const TIndexedContainerIterator& Rhs) const UE_STUB_BODY
};
