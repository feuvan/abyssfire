// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/StaticArray.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"

template <typename InElementType, uint32 NumElements, uint32 Alignment = alignof(InElementType)>
class TStaticArray
{
public:
	using ElementType = InElementType;
	TStaticArray() = default;
	explicit TStaticArray(const InElementType& DefaultElement) UE_STUB_BODY
	TStaticArray(TStaticArray&& Other) = default;
	TStaticArray(const TStaticArray& Other) = default;
	TStaticArray& operator=(TStaticArray&& Other) = default;
	TStaticArray& operator=(const TStaticArray& Other) = default;
	InElementType& operator[](uint32 Index) { return Storage[Index]; }
	const InElementType& operator[](uint32 Index) const { return Storage[Index]; }
	bool operator==(const TStaticArray&) const = default;
	bool IsEmpty() const { return NumElements == 0; }
	constexpr int32 Num() const { return static_cast<int32>(NumElements); }
	InElementType* GetData() { return Storage; }
	const InElementType* GetData() const { return Storage; }
	InElementType* begin() { return Storage; }
	const InElementType* begin() const { return Storage; }
	InElementType* end() { return Storage + NumElements; }
	const InElementType* end() const { return Storage + NumElements; }
private:
	alignas(Alignment) InElementType Storage[NumElements];
};
template <typename T, uint32 N> struct TIsContiguousContainer<TStaticArray<T, N>> { enum { Value = true }; };
