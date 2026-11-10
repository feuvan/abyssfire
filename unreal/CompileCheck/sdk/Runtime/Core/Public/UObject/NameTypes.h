// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/UObject/NameTypes.h (from UE 5.x source knowledge).
// FName: case-insensitive, compared by index. Implicitly constructible from a character pointer (WIDECHAR* = TCHAR*,
// ANSICHAR*, UTF8CHAR*) and from EName (NAME_None), and only EXPLICITLY from a string view. There is no constructor
// from FString: `FName N = Str;` or passing an FString where an FName is expected does not compile; `FName(*Str)` and
// `FName(Str)` (the explicit view constructor, FString -> FStringView being implicit) do. Not from std::string.
// ToString() returns FString; there is no operator*.
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Containers/StringFwd.h"
#include "UObject/UnrealNames.h"

enum EFindName { FNAME_Find, FNAME_Add };
enum { NAME_SIZE = 1024 };
enum class ENameCase : uint8 { CaseSensitive, IgnoreCase };

struct FNameEntryId
{
	FNameEntryId() = default;
	bool IsNone() const;
	bool operator==(FNameEntryId Rhs) const;
	bool operator!=(FNameEntryId Rhs) const;
	uint32 ToUnstableInt() const;
};

class FName
{
public:
	constexpr FName() = default;
	constexpr FName(EName InName) {}
	FName(ENoInit) {}
	FName(const WIDECHAR* Name, EFindName FindType = FNAME_Add);
	FName(const ANSICHAR* Name, EFindName FindType = FNAME_Add);
	FName(const UTF8CHAR* Name, EFindName FindType = FNAME_Add);
	explicit FName(TStringView<ANSICHAR> View, EFindName FindType = FNAME_Add);
	explicit FName(TStringView<WIDECHAR> View, EFindName FindType = FNAME_Add);
	explicit FName(TStringView<UTF8CHAR> View, EFindName FindType = FNAME_Add);
	FName(const WIDECHAR* Name, int32 Number);
	FName(int32 Len, const WIDECHAR* Name, EFindName FindType = FNAME_Add);
	FName(FName Other, int32 InNumber);

	FNameEntryId GetComparisonIndex() const;
	FNameEntryId GetDisplayIndex() const;
	int32 GetNumber() const;
	void SetNumber(const int32 NewNumber);
	FString GetPlainNameString() const;
	FString ToString() const;
	void ToString(FString& Out) const;
	void AppendString(FString& Out) const;
	uint32 GetStringLength() const;
	bool IsEqual(const FName& Other, const ENameCase CompareMethod = ENameCase::IgnoreCase, const bool bCompareNumber = true) const;
	bool operator==(FName Other) const;
	bool operator!=(FName Other) const;
	bool operator==(EName Ename) const;
	bool operator!=(EName Ename) const;
	bool operator==(const TCHAR* Other) const;
	bool operator!=(const TCHAR* Other) const;
	bool operator==(const ANSICHAR* Other) const;
	bool operator!=(const ANSICHAR* Other) const;
	bool FastLess(const FName& Other) const;
	bool LexicalLess(const FName& Other) const;
	bool IsNone() const;
	bool IsValid() const;
	bool IsValidXName(const FString& InvalidChars, FText* Reason = nullptr) const;
	int32 Compare(const FName& Other) const;
	int32 CompareIndexes(const FName& Other) const;
};
uint32 GetTypeHash(FName Name);
FString LexToString(const FName& Name);
void LexFromString(FName& Name, const TCHAR* Str);
struct FNameFastLess { bool operator()(const FName& A, const FName& B) const; };
struct FNameLexicalLess { bool operator()(const FName& A, const FName& B) const; };
template <> struct TIsZeroConstructType<FName> { enum { Value = true }; };
