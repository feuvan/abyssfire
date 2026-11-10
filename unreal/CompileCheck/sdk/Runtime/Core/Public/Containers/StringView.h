// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/StringView.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/StringFwd.h"
#include "Misc/CString.h"
#include "Templates/UnrealTemplate.h"

template <typename CharType>
class TStringView
{
public:
	using ElementType = CharType;
	using ViewType = TStringView<CharType>;
	constexpr TStringView() = default;
	constexpr TStringView(const CharType* InData UE_LIFETIMEBOUND) : DataPtr(InData), Size(0) {}
	constexpr TStringView(const CharType* InData UE_LIFETIMEBOUND, int32 InSize) : DataPtr(InData), Size(InSize) {}
	template <typename CharRangeType>
		requires requires(const CharRangeType& Range) { { UE::Stub::RangeData(Range) } -> std::convertible_to<const CharType*>; UE::Stub::RangeNum(Range); }
			&& (!std::is_array_v<std::remove_reference_t<CharRangeType>>) && (!std::is_same_v<std::decay_t<CharRangeType>, TStringView>)
	constexpr TStringView(CharRangeType&& InRange UE_LIFETIMEBOUND) : DataPtr(nullptr), Size(0) {}
	const CharType& operator[](int32 Index) const UE_STUB_BODY
	const CharType* GetData() const { return DataPtr; }
	constexpr int32 Len() const { return Size; }
	constexpr bool IsEmpty() const { return Size == 0; }
	void RemovePrefix(int32 CharCount) UE_STUB_BODY
	void RemoveSuffix(int32 CharCount) UE_STUB_BODY
	void Reset() UE_STUB_BODY
	int32 CopyString(CharType* Dest, int32 CharCount, int32 Position = 0) const UE_STUB_BODY
	ViewType SubStr(int32 Position, int32 CharCount) const UE_STUB_BODY
	ViewType Left(int32 CharCount) const UE_STUB_BODY
	ViewType LeftChop(int32 CharCount) const UE_STUB_BODY
	ViewType Right(int32 CharCount) const UE_STUB_BODY
	ViewType RightChop(int32 CharCount) const UE_STUB_BODY
	ViewType Mid(int32 Position, int32 CharCount = 0x7fffffff) const UE_STUB_BODY
	ViewType TrimStartAndEnd() const UE_STUB_BODY
	ViewType TrimStart() const UE_STUB_BODY
	ViewType TrimEnd() const UE_STUB_BODY
	/** The *Inline forms modify this view (5.0+). */
	void LeftInline(int32 CharCount) UE_STUB_BODY
	void LeftChopInline(int32 CharCount) UE_STUB_BODY
	void RightInline(int32 CharCount) UE_STUB_BODY
	void RightChopInline(int32 CharCount) UE_STUB_BODY
	void MidInline(int32 Position, int32 CharCount = 0x7fffffff) UE_STUB_BODY
	void TrimStartAndEndInline() UE_STUB_BODY
	void TrimStartInline() UE_STUB_BODY
	void TrimEndInline() UE_STUB_BODY
	bool Equals(ViewType Other, ESearchCase::Type SearchCase = ESearchCase::CaseSensitive) const UE_STUB_BODY
	int32 Compare(ViewType Other, ESearchCase::Type SearchCase = ESearchCase::CaseSensitive) const UE_STUB_BODY
	bool StartsWith(CharType Prefix) const UE_STUB_BODY
	bool StartsWith(ViewType Prefix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const UE_STUB_BODY
	bool EndsWith(CharType Suffix) const UE_STUB_BODY
	bool EndsWith(ViewType Suffix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const UE_STUB_BODY
	int32 Find(ViewType Search, int32 StartPosition = 0, ESearchCase::Type SearchCase = ESearchCase::CaseSensitive) const UE_STUB_BODY
	bool Contains(ViewType Search, ESearchCase::Type SearchCase = ESearchCase::CaseSensitive) const UE_STUB_BODY
	bool FindChar(CharType Search, int32& OutIndex) const UE_STUB_BODY
	bool FindLastChar(CharType Search, int32& OutIndex) const UE_STUB_BODY
	const CharType* begin() const UE_STUB_BODY
	const CharType* end() const UE_STUB_BODY
	bool operator==(TStringView Rhs) const UE_STUB_BODY
	bool operator!=(TStringView Rhs) const UE_STUB_BODY
	bool operator<(TStringView Rhs) const UE_STUB_BODY
private:
	const CharType* DataPtr = nullptr;
	int32 Size = 0;
};
template <typename CharPtrOrRangeType> TStringView(CharPtrOrRangeType&&) -> TStringView<std::remove_cv_t<std::remove_pointer_t<std::decay_t<CharPtrOrRangeType>>>>;
template <typename CharType> uint32 GetTypeHash(TStringView<CharType> View) UE_STUB_BODY
template <typename CharType> struct TIsContiguousContainer<TStringView<CharType>> { enum { Value = true }; };
constexpr FStringView operator""_SV(const TCHAR* String, size_t Size) { return FStringView(String, static_cast<int32>(Size)); }
#define TEXTVIEW(str) (FStringView(TEXT(str), static_cast<int32>(UE_ARRAY_COUNT(TEXT(str)) - 1)))
