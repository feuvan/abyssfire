// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/UnrealString.h (+ UnrealString.h.inl).
// From UE 5.x source knowledge. FString: TCHAR string. No conversion from or to std::string; operator* yields the
// const TCHAR*; Printf is format-checked (Misc/VarArgs.h). WIDECHAR is TCHAR on the checked platforms, so the WIDECHAR
// overloads below are the TCHAR ones; the engine's UCS2CHAR (uint16) overloads are not declared. A single character is
// appended only as a character type (TIsCharType): `Str + 5` and `Str += Count` do not compile.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "Misc/CString.h"
#include "Misc/VarArgs.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"

struct FStringFormatArg;
template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs> class TMap;
using FStringFormatNamedArguments = TMap<FString, FStringFormatArg, FDefaultSetAllocator, TDefaultMapHashableKeyFuncs<FString, FStringFormatArg, false>>;
using FStringFormatOrderedArguments = TArray<FStringFormatArg>;


class FString
{
public:
	using AllocatorType = FDefaultAllocator;
	using ElementType = TCHAR;
	using FmtCharType = TCHAR;
	using DataType = TArray<TCHAR, AllocatorType>;

	FString() = default;
	FString(FString&&) = default;
	FString(const FString&) = default;
	FString& operator=(FString&&) = default;
	FString& operator=(const FString&) = default;
	FString(const FString& Other, int32 ExtraSlack);
	FString(FString&& Other, int32 ExtraSlack);
	FString(const ANSICHAR* Str);
	FString(const WIDECHAR* Str);
	FString(const UTF8CHAR* Str);
	FString(const ANSICHAR* Str, int32 ExtraSlack);
	FString(const WIDECHAR* Str, int32 ExtraSlack);
	explicit FString(TYPE_OF_NULLPTR) = delete;
	explicit FString(const FAnsiStringView& Other);
	explicit FString(const FWideStringView& Other);
	explicit FString(const FUtf8StringView& Other);
	FString& operator=(const TCHAR* Str);
	FString& operator=(const ANSICHAR* Str);
	FString& operator=(const UTF8CHAR* Str);

	static FString ConstructFromPtrSize(const ANSICHAR* Str, int32 Size);
	static FString ConstructFromPtrSize(const WIDECHAR* Str, int32 Size);
	static FString ConstructFromPtrSize(const UTF8CHAR* Str, int32 Size);
	static FString ConstructFromPtrSizeWithSlack(const ANSICHAR* Str, int32 Size, int32 ExtraSlack);
	static FString ConstructFromPtrSizeWithSlack(const WIDECHAR* Str, int32 Size, int32 ExtraSlack);
	static FString ConstructFromPtrSizeWithSlack(const UTF8CHAR* Str, int32 Size, int32 ExtraSlack);
	static FString ConstructWithSlack(const TCHAR* Str, int32 ExtraSlack);

	const TCHAR* operator*() const UE_LIFETIMEBOUND;
	DataType& GetCharArray() UE_LIFETIMEBOUND;
	const DataType& GetCharArray() const UE_LIFETIMEBOUND;
	TCHAR& operator[](int32 Index) UE_LIFETIMEBOUND;
	const TCHAR& operator[](int32 Index) const UE_LIFETIMEBOUND;
	const TCHAR* begin() const;
	const TCHAR* end() const;
	SIZE_T GetAllocatedSize() const;
	void CheckInvariants() const;
	void Empty();
	void Empty(int32 Slack);
	bool IsEmpty() const;
	void Reset(int32 NewReservedSize = 0);
	void Shrink();
	bool IsValidIndex(int32 Index) const;
	int32 Len() const;
	void Reserve(int32 CharacterCount);

	bool operator==(const FString& Rhs) const;
	bool operator!=(const FString& Rhs) const;
	bool operator<(const FString& Rhs) const;
	bool operator<=(const FString& Rhs) const;
	bool operator>(const FString& Rhs) const;
	bool operator>=(const FString& Rhs) const;
	bool operator==(const TCHAR* Rhs) const;
	bool operator!=(const TCHAR* Rhs) const;
	bool operator==(const ANSICHAR* Rhs) const;
	bool operator!=(const ANSICHAR* Rhs) const;
	friend bool operator==(const TCHAR* Lhs, const FString& Rhs);
	friend bool operator!=(const TCHAR* Lhs, const FString& Rhs);

	FString& Append(const TCHAR* Str);
	FString& Append(const ANSICHAR* Str);
	FString& Append(const TCHAR* Str, int32 Count);
	FString& Append(const FString& Str);
	FString& Append(FStringView Str);
	// 5.4+ (UnrealString.h.inl): a template over the character types; earlier engines took a TCHAR (any integer converted).
	template <typename AppendedCharType> requires (static_cast<bool>(TIsCharType<AppendedCharType>::Value))
	FString& AppendChar(AppendedCharType Char) UE_STUB_BODY
	FString& AppendChars(const TCHAR* Str, int32 Count);
	void AppendInt(int32 InNum);
	template <typename... Types>
	FString& Appendf(UE::Core::TCheckedFormatString<FmtCharType, Types...> Fmt, Types... Args) UE_STUB_BODY
	FString& operator+=(const FString& Str);
	FString& operator+=(FString&& Str);
	FString& operator+=(const TCHAR* Str);
	FString& operator+=(const ANSICHAR* Str);
	FString& operator+=(FStringView Str);
	template <typename AppendedCharType> requires (static_cast<bool>(TIsCharType<AppendedCharType>::Value))
	FString& operator+=(AppendedCharType Char) UE_STUB_BODY
	void InsertAt(int32 Index, TCHAR Character);
	void InsertAt(int32 Index, const FString& Characters);
	void RemoveAt(int32 Index, EAllowShrinking AllowShrinking = EAllowShrinking::Yes);
	template <typename CountType> requires std::is_integral_v<CountType> && (!std::is_same_v<CountType, bool>)
	void RemoveAt(int32 Index, CountType Count, EAllowShrinking AllowShrinking = EAllowShrinking::Yes) UE_STUB_BODY
	bool RemoveFromStart(const TCHAR* InPrefix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase);
	bool RemoveFromStart(const FString& InPrefix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase);
	bool RemoveFromEnd(const TCHAR* InSuffix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase);
	bool RemoveFromEnd(const FString& InSuffix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase);
	void PathAppend(const TCHAR* Str, int32 StrLength);

	friend FString operator+(const FString& Lhs, const FString& Rhs);
	friend FString operator+(FString&& Lhs, const FString& Rhs);
	friend FString operator+(const FString& Lhs, FString&& Rhs);
	friend FString operator+(FString&& Lhs, FString&& Rhs);
	friend FString operator+(const TCHAR* Lhs, const FString& Rhs);
	friend FString operator+(const TCHAR* Lhs, FString&& Rhs);
	friend FString operator+(const FString& Lhs, const TCHAR* Rhs);
	friend FString operator+(FString&& Lhs, const TCHAR* Rhs);
	// String + character: templates over the character types only, as in the engine (no integer converts to a
	// character). A character + string form is not declared: it could not be confirmed, and the module does not use one.
	template <typename CharType> requires (static_cast<bool>(TIsCharType<CharType>::Value))
	friend FString operator+(const FString& Lhs, CharType Rhs) UE_STUB_BODY
	template <typename CharType> requires (static_cast<bool>(TIsCharType<CharType>::Value))
	friend FString operator+(FString&& Lhs, CharType Rhs) UE_STUB_BODY
	FString& operator/=(const TCHAR* Str);
	FString& operator/=(const FString& Str);
	FString& operator/=(FStringView Str);
	friend FString operator/(const FString& Lhs, const TCHAR* Rhs);
	friend FString operator/(FString&& Lhs, const TCHAR* Rhs);
	friend FString operator/(const FString& Lhs, const FString& Rhs);
	friend FString operator/(FString&& Lhs, const FString& Rhs);
	friend FString operator/(const TCHAR* Lhs, const FString& Rhs);

	bool Equals(const FString& Other, ESearchCase::Type SearchCase = ESearchCase::CaseSensitive) const;
	int32 Compare(const FString& Other, ESearchCase::Type SearchCase = ESearchCase::CaseSensitive) const;
	bool Contains(const TCHAR* SubStr, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase, ESearchDir::Type SearchDir = ESearchDir::FromStart) const;
	bool Contains(const FString& SubStr, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase, ESearchDir::Type SearchDir = ESearchDir::FromStart) const;
	bool Contains(FStringView SubStr, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase, ESearchDir::Type SearchDir = ESearchDir::FromStart) const;
	int32 Find(const TCHAR* SubStr, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase, ESearchDir::Type SearchDir = ESearchDir::FromStart, int32 StartPosition = INDEX_NONE) const;
	int32 Find(const FString& SubStr, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase, ESearchDir::Type SearchDir = ESearchDir::FromStart, int32 StartPosition = INDEX_NONE) const;
	int32 Find(FStringView SubStr, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase, ESearchDir::Type SearchDir = ESearchDir::FromStart, int32 StartPosition = INDEX_NONE) const;
	bool FindChar(TCHAR InChar, int32& OutIndex) const;
	bool FindLastChar(TCHAR InChar, int32& OutIndex) const;
	template <typename Predicate> int32 FindLastCharByPredicate(Predicate Pred, int32 Count) const requires std::is_invocable_r_v<bool, Predicate&, const TCHAR&> UE_STUB_BODY
	template <typename Predicate> int32 FindLastCharByPredicate(Predicate Pred) const requires std::is_invocable_r_v<bool, Predicate&, const TCHAR&> UE_STUB_BODY
	bool StartsWith(const TCHAR* InPrefix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;
	bool StartsWith(const FString& InPrefix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;
	bool StartsWith(FStringView InPrefix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;
	bool EndsWith(const TCHAR* InSuffix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;
	bool EndsWith(const FString& InSuffix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;
	bool EndsWith(FStringView InSuffix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;
	bool MatchesWildcard(const TCHAR* Wildcard, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;
	bool MatchesWildcard(const FString& Wildcard, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;

	[[nodiscard]] FString ToUpper() const&;
	[[nodiscard]] FString ToUpper() &&;
	void ToUpperInline();
	[[nodiscard]] FString ToLower() const&;
	[[nodiscard]] FString ToLower() &&;
	void ToLowerInline();
	[[nodiscard]] FString LeftPad(int32 ChCount) const;
	[[nodiscard]] FString RightPad(int32 ChCount) const;
	[[nodiscard]] FString Left(int32 Count) const&;
	[[nodiscard]] FString Left(int32 Count) &&;
	void LeftInline(int32 Count, EAllowShrinking AllowShrinking = EAllowShrinking::Yes);
	[[nodiscard]] FString LeftChop(int32 Count) const&;
	[[nodiscard]] FString LeftChop(int32 Count) &&;
	void LeftChopInline(int32 Count, EAllowShrinking AllowShrinking = EAllowShrinking::Yes);
	[[nodiscard]] FString Right(int32 Count) const&;
	[[nodiscard]] FString Right(int32 Count) &&;
	void RightInline(int32 Count, EAllowShrinking AllowShrinking = EAllowShrinking::Yes);
	[[nodiscard]] FString RightChop(int32 Count) const&;
	[[nodiscard]] FString RightChop(int32 Count) &&;
	void RightChopInline(int32 Count, EAllowShrinking AllowShrinking = EAllowShrinking::Yes);
	[[nodiscard]] FString Mid(int32 Start) const&;
	[[nodiscard]] FString Mid(int32 Start) &&;
	[[nodiscard]] FString Mid(int32 Start, int32 Count) const&;
	[[nodiscard]] FString Mid(int32 Start, int32 Count) &&;
	void MidInline(int32 Start, int32 Count = 0x7fffffff, EAllowShrinking AllowShrinking = EAllowShrinking::Yes);
	[[nodiscard]] FString TrimStartAndEnd() const&;
	[[nodiscard]] FString TrimStartAndEnd() &&;
	void TrimStartAndEndInline();
	[[nodiscard]] FString TrimStart() const&;
	[[nodiscard]] FString TrimStart() &&;
	void TrimStartInline();
	[[nodiscard]] FString TrimEnd() const&;
	[[nodiscard]] FString TrimEnd() &&;
	void TrimEndInline();
	[[nodiscard]] FString TrimQuotes(bool* bQuotesRemoved = nullptr) const&;
	[[nodiscard]] FString TrimChar(const TCHAR CharacterToTrim, bool* bCharRemoved = nullptr) const&;
	[[nodiscard]] FString Reverse() const&;
	void ReverseString();
	[[nodiscard]] FString Replace(const TCHAR* From, const TCHAR* To, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const&;
	[[nodiscard]] FString Replace(const TCHAR* From, const TCHAR* To, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) &&;
	int32 ReplaceInline(const TCHAR* SearchText, const TCHAR* ReplacementText, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase);
	void ReplaceCharInline(const TCHAR SearchChar, const TCHAR ReplacementChar, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase);
	[[nodiscard]] FString ReplaceQuotesWithEscapedQuotes() const&;
	[[nodiscard]] FString ReplaceCharWithEscapedChar(const TArray<TCHAR>* Chars = nullptr) const&;
	[[nodiscard]] FString ReplaceEscapedCharWithChar(const TArray<TCHAR>* Chars = nullptr) const&;
	[[nodiscard]] FString ConvertTabsToSpaces(const int32 InSpacesPerTab) const&;
	bool Split(const FString& InS, FString* LeftS, FString* RightS, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase, ESearchDir::Type SearchDir = ESearchDir::FromStart) const;
	int32 ParseIntoArray(TArray<FString>& OutArray, const TCHAR* pchDelim, bool InCullEmpty = true) const;
	int32 ParseIntoArray(TArray<FString>& OutArray, const TCHAR* const* DelimArray, int32 NumDelims, bool InCullEmpty = true) const;
	int32 ParseIntoArrayWS(TArray<FString>& OutArray, const TCHAR* pchExtraDelim = nullptr, bool InCullEmpty = true) const;
	int32 ParseIntoArrayLines(TArray<FString>& OutArray, bool InCullEmpty = true) const;
	static int32 CullArray(TArray<FString>* InArray);
	bool IsNumeric() const;
	bool ToBool() const;
	static FString Chr(TCHAR Ch);
	static FString ChrN(int32 NumCharacters, TCHAR Char);
	static FString FromInt(int32 Num);
	static FString FromHexBlob(const uint8* SrcBuffer, const uint32 SrcSize);
	static FString SanitizeFloat(double InFloat, const int32 InMinFractionalDigits = 1);
	static FString FormatAsNumber(int32 InNumber);
	static FString Format(const TCHAR* InFormatString, const FStringFormatNamedArguments& InNamedArguments);
	static FString Format(const TCHAR* InFormatString, const FStringFormatOrderedArguments& InOrderedArguments);
	template <typename... Types>
	[[nodiscard]] static FString Printf(UE::Core::TCheckedFormatString<FmtCharType, Types...> Fmt, Types... Args) UE_STUB_BODY
	// Join does `Result += Element` for each element of a ranged-for over Range, JoinBy `Result += Invoke(Proj, Element)`.
	template <typename RangeType> requires requires(const RangeType& Range, FString& Result) { Range.end(); Result += *Range.begin(); }
	[[nodiscard]] static FString Join(const RangeType& Range, const TCHAR* Separator) UE_STUB_BODY
	template <typename RangeType, typename ProjectionType> requires requires(const RangeType& Range, FString& Result) { Range.end(); Result += std::declval<std::invoke_result_t<ProjectionType&, decltype(*Range.begin())>>(); }
	[[nodiscard]] static FString JoinBy(const RangeType& Range, const TCHAR* Separator, ProjectionType Proj) UE_STUB_BODY
};
template <> struct TIsContiguousContainer<FString> { enum { Value = true }; };
uint32 GetTypeHash(const FString& S);
const TCHAR* GetData(const FString& String);
int32 GetNum(const FString& String);

/** LexToString / LexFromString for the built-in types (the real ones are templates constrained on the type). */
FString LexToString(int32 Value);
FString LexToString(uint32 Value);
FString LexToString(int64 Value);
FString LexToString(uint64 Value);
FString LexToString(float Value);
FString LexToString(double Value);
FString LexToString(bool Value);
FString LexToString(const FString& Str);
FString LexToString(FString&& Str);
FString LexToString(const TCHAR* Str);
FString LexToSanitizedString(float Value);
FString LexToSanitizedString(double Value);
void LexFromString(int32& OutValue, const TCHAR* Buffer);
void LexFromString(uint32& OutValue, const TCHAR* Buffer);
void LexFromString(int64& OutValue, const TCHAR* Buffer);
void LexFromString(uint64& OutValue, const TCHAR* Buffer);
void LexFromString(float& OutValue, const TCHAR* Buffer);
void LexFromString(double& OutValue, const TCHAR* Buffer);
void LexFromString(bool& OutValue, const TCHAR* Buffer);
void LexFromString(FString& OutValue, const TCHAR* Buffer);
template <typename T> bool LexTryParseString(T& OutValue, const TCHAR* Buffer) requires std::is_arithmetic_v<T> UE_STUB_BODY

inline const TCHAR* ToCStr(const TCHAR* Ptr) { return Ptr; }
inline const TCHAR* ToCStr(const FString& Str) { return *Str; }
int32 HexToBytes(const FString& HexString, uint8* OutBytes);
FString BytesToHex(const uint8* Bytes, int32 NumBytes);
int32 StringToBytes(const FString& String, uint8* OutBytes, int32 MaxBufferSize);
FString BytesToString(const uint8* In, int32 Count);
