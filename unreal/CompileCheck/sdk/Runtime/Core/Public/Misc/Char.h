// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/Char.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

template <typename CharType>
struct TChar
{
	static constexpr CharType LineFeed = (CharType)0xa;
	static constexpr CharType VerticalTab = (CharType)0xb;
	static constexpr CharType FormFeed = (CharType)0xc;
	static constexpr CharType CarriageReturn = (CharType)0xd;
	static constexpr CharType NextLine = (CharType)0x85;
	static CharType ToUpper(CharType Char) UE_STUB_BODY
	static CharType ToLower(CharType Char) UE_STUB_BODY
	static bool IsUpper(CharType Char) UE_STUB_BODY
	static bool IsLower(CharType Char) UE_STUB_BODY
	static bool IsAlpha(CharType Char) UE_STUB_BODY
	static bool IsGraph(CharType Char) UE_STUB_BODY
	static bool IsPrint(CharType Char) UE_STUB_BODY
	static bool IsPunct(CharType Char) UE_STUB_BODY
	static bool IsAlnum(CharType Char) UE_STUB_BODY
	static bool IsDigit(CharType Char) UE_STUB_BODY
	static bool IsHexDigit(CharType Char) UE_STUB_BODY
	static bool IsWhitespace(CharType Char) UE_STUB_BODY
	static bool IsControl(CharType Char) UE_STUB_BODY
	static bool IsOctDigit(CharType Char) UE_STUB_BODY
	static int32 ConvertCharDigitToInt(CharType Char) UE_STUB_BODY
	static bool IsIdentifier(CharType Char) UE_STUB_BODY
	static bool IsUnderscore(CharType Char) UE_STUB_BODY
	static bool IsLinebreak(CharType Char) UE_STUB_BODY
};
typedef TChar<TCHAR> FChar;
typedef TChar<WIDECHAR> FCharWide;
typedef TChar<ANSICHAR> FCharAnsi;
