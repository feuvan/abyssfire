// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/StringFwd.h (from UE 5.x source knowledge).
#pragma once

#include "HAL/Platform.h"

class FString;
class FAnsiString;
class FUtf8String;
template <typename CharType> class TStringView;
template <typename CharType> class TStringBuilderBase;
template <typename CharType, int32 BufferSize> class TStringBuilderWithBuffer;
using FStringView = TStringView<TCHAR>;
using FAnsiStringView = TStringView<ANSICHAR>;
using FWideStringView = TStringView<WIDECHAR>;
using FUtf8StringView = TStringView<UTF8CHAR>;
using FStringBuilderBase = TStringBuilderBase<TCHAR>;
using FAnsiStringBuilderBase = TStringBuilderBase<ANSICHAR>;
using FUtf8StringBuilderBase = TStringBuilderBase<UTF8CHAR>;
template <int32 BufferSize> using TStringBuilder = TStringBuilderWithBuffer<TCHAR, BufferSize>;
template <int32 BufferSize> using TAnsiStringBuilder = TStringBuilderWithBuffer<ANSICHAR, BufferSize>;
template <int32 BufferSize> using TUtf8StringBuilder = TStringBuilderWithBuffer<UTF8CHAR, BufferSize>;
