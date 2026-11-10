// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/CString.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Misc/Char.h"
#include "Misc/VarArgs.h"

namespace ESearchCase { enum Type { CaseSensitive, IgnoreCase }; }
namespace ESearchDir { enum Type { FromStart, FromEnd }; }

template <typename T>
struct TCString
{
	using CharType = T;
	static bool IsPureAnsi(const CharType* Str) UE_STUB_BODY
	static bool IsNumeric(const CharType* Str) UE_STUB_BODY
	static CharType* Strcpy(CharType* Dest, SIZE_T DestCount, const CharType* Src) UE_STUB_BODY
	static CharType* Strncpy(CharType* Dest, const CharType* Src, SIZE_T MaxLen) UE_STUB_BODY
	static CharType* Strcat(CharType* Dest, SIZE_T DestCount, const CharType* Src) UE_STUB_BODY
	static CharType* Strupr(CharType* Dest, SIZE_T DestCount) UE_STUB_BODY
	static int32 Strcmp(const CharType* String1, const CharType* String2) UE_STUB_BODY
	static int32 Strncmp(const CharType* String1, const CharType* String2, SIZE_T Count) UE_STUB_BODY
	static int32 Stricmp(const CharType* String1, const CharType* String2) UE_STUB_BODY
	static int32 Strnicmp(const CharType* String1, const CharType* String2, SIZE_T Count) UE_STUB_BODY
	static const CharType* Spc(int32 NumSpaces) UE_STUB_BODY
	static const CharType* Tab(int32 NumTabs) UE_STUB_BODY
	static const CharType* Strfind(const CharType* Str, const CharType* Find, bool bSkipQuotedChars = false) UE_STUB_BODY
	static const CharType* Strifind(const CharType* Str, const CharType* Find, bool bSkipQuotedChars = false) UE_STUB_BODY
	static const CharType* StrfindDelim(const CharType* Str, const CharType* Find, const CharType* Delim = nullptr) UE_STUB_BODY
	static const CharType* Stristr(const CharType* Str, const CharType* Find) UE_STUB_BODY
	static CharType* Stristr(CharType* Str, const CharType* Find) UE_STUB_BODY
	static int32 Strlen(const CharType* String) UE_STUB_BODY
	static int32 Strnlen(const CharType* String, SIZE_T StringSize) UE_STUB_BODY
	static const CharType* Strstr(const CharType* String, const CharType* Find) UE_STUB_BODY
	static CharType* Strstr(CharType* String, const CharType* Find) UE_STUB_BODY
	static const CharType* Strchr(const CharType* String, CharType c) UE_STUB_BODY
	static CharType* Strchr(CharType* String, CharType c) UE_STUB_BODY
	static const CharType* Strrchr(const CharType* String, CharType c) UE_STUB_BODY
	static CharType* Strrchr(CharType* String, CharType c) UE_STUB_BODY
	static int32 Strspn(const CharType* String, const CharType* Mask) UE_STUB_BODY
	static int32 Strcspn(const CharType* String, const CharType* Mask) UE_STUB_BODY
	static int32 Atoi(const CharType* String) UE_STUB_BODY
	static int64 Atoi64(const CharType* String) UE_STUB_BODY
	static float Atof(const CharType* String) UE_STUB_BODY
	static double Atod(const CharType* String) UE_STUB_BODY
	static bool ToBool(const CharType* String) UE_STUB_BODY
	static int32 Strtoi(const CharType* Start, CharType** End, int32 Base) UE_STUB_BODY
	static int64 Strtoi64(const CharType* Start, CharType** End, int32 Base) UE_STUB_BODY
	static uint64 Strtoui64(const CharType* Start, CharType** End, int32 Base) UE_STUB_BODY
	static CharType* Strtok(CharType* TokenString, const CharType* Delim, CharType** Context) UE_STUB_BODY
	template <SIZE_T DestCount, typename... ArgTypes>
	static int32 Sprintf(CharType (&Dest)[DestCount], UE::Core::TCheckedFormatString<CharType, ArgTypes...> Fmt, ArgTypes... Args) UE_STUB_BODY
	template <typename... ArgTypes>
	static int32 Snprintf(CharType* Dest, int32 DestSize, UE::Core::TCheckedFormatString<CharType, ArgTypes...> Fmt, ArgTypes... Args) UE_STUB_BODY
};
typedef TCString<TCHAR> FCString;
typedef TCString<ANSICHAR> FCStringAnsi;
typedef TCString<WIDECHAR> FCStringWide;
typedef TCString<UTF8CHAR> FCStringUtf8;
