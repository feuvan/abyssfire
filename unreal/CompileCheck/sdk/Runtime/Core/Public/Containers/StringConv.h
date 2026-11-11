// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/StringConv.h (from UE 5.x source knowledge).
// Encoding conversion: StringCast<To>(Ptr[, Len]) and the TCHAR_TO_UTF8 / UTF8_TO_TCHAR macro family. The conversion
// object owns the buffer: Get() is the pointer (valid while the object lives), Length() the count without terminator.
#pragma once

#include "CoreTypes.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"

class FTCHARToUTF8_Convert;
class FUTF8ToTCHAR_Convert;

/** The engine's TStringConversion<Converter>: `Tag` stands for the converter class, so that the legacy converters below
 *  are types of their own (FTCHARToUTF8 is not what StringCast<ANSICHAR>(const TCHAR*) returns). */
template <typename ToType, typename FromType, typename Tag = void>
class TStringConversionStub
{
public:
	using ToCharType = ToType;
	using FromCharType = FromType;
	explicit TStringConversionStub(const FromType* Source) UE_STUB_BODY
	TStringConversionStub(const FromType* Source, int32 SourceLen) UE_STUB_BODY
	// FUTF8ToTCHAR has two source types in the engine (Converter::FromType is UTF8CHAR, Converter::LegacyFromType is
	// ANSICHAR, both constructor pairs exist): the UTF8CHAR pair here, the ANSICHAR pair above.
	template <typename SrcBufferType> requires std::is_same_v<Tag, FUTF8ToTCHAR_Convert> && std::is_same_v<SrcBufferType, UTF8CHAR>
	explicit TStringConversionStub(const SrcBufferType* Source) UE_STUB_BODY
	template <typename SrcBufferType> requires std::is_same_v<Tag, FUTF8ToTCHAR_Convert> && std::is_same_v<SrcBufferType, UTF8CHAR>
	TStringConversionStub(const SrcBufferType* Source, int32 SourceLen) UE_STUB_BODY
	TStringConversionStub(TStringConversionStub&&) = default;
	TStringConversionStub(const TStringConversionStub&) = delete;
	TStringConversionStub& operator=(const TStringConversionStub&) = delete;
	const ToType* Get() const UE_LIFETIMEBOUND UE_STUB_BODY
	int32 Length() const UE_STUB_BODY
	const ToType* begin() const UE_STUB_BODY
	const ToType* end() const UE_STUB_BODY
};
/**
 * No-op "conversion" between encodings with the same code units: a pointer wrapper (TStringPointer<FromType, ToType>).
 * It takes the source pointer and hands it back as the destination character type (TCHAR -> UTF16CHAR, a uint16).
 */
template <typename InFromType, typename InToType = InFromType>
class TStringPointerStub
{
	static_assert(sizeof(InFromType) == sizeof(InToType), "FromType must be the same size as ToType!");
public:
	using FromType = InFromType;
	using ToType = InToType;
	explicit TStringPointerStub(const FromType* InPtr) UE_STUB_BODY
	TStringPointerStub(const FromType* InPtr, int32 InLength) UE_STUB_BODY
	const ToType* Get() const UE_STUB_BODY
	int32 Length() const UE_STUB_BODY
};

template <typename To, typename From> requires (!std::is_same_v<To, From>)
TStringConversionStub<To, From> StringCast(const From* Str) UE_STUB_BODY
template <typename To, typename From> requires (!std::is_same_v<To, From>)
TStringConversionStub<To, From> StringCast(const From* Str, int32 Len) UE_STUB_BODY
template <typename To, typename From> requires std::is_same_v<To, From>
TStringPointerStub<To> StringCast(const From* Str) UE_STUB_BODY
template <typename To, typename From> requires std::is_same_v<To, From>
TStringPointerStub<To> StringCast(const From* Str, int32 Len) UE_STUB_BODY

// The legacy UTF-8 converters keep ANSICHAR on the UTF-8 side: FTCHARToUTF8::Get() is a `const ANSICHAR*`
// (FTCHARToUTF8_Convert::ToType; what std::string and the C library take) and FUTF8ToTCHAR is built from a
// `const ANSICHAR*` (its LegacyFromType) or a `const UTF8CHAR*` (its FromType). Only StringCast<UTF8CHAR> yields
// `const UTF8CHAR*`. From UE 5.x source knowledge; the module uses neither class (API_UNCERTAIN.md, lower-confidence rows).
using FTCHARToUTF8 = TStringConversionStub<ANSICHAR, TCHAR, FTCHARToUTF8_Convert>;
using FUTF8ToTCHAR = TStringConversionStub<TCHAR, ANSICHAR, FUTF8ToTCHAR_Convert>;
using FTCHARToUTF16 = TStringPointerStub<TCHAR, UTF16CHAR>;
using FUTF16ToTCHAR = TStringPointerStub<UTF16CHAR, TCHAR>;

/** The macro family yields raw pointers into a temporary that dies at the end of the full expression. */
namespace UE::Stub
{
	struct FAnsiCastResult { explicit FAnsiCastResult(const TCHAR* Str); const ANSICHAR* Get() const; int32 Length() const; };
	struct FUtf8CastResult { explicit FUtf8CastResult(const TCHAR* Str); const ANSICHAR* Get() const; int32 Length() const; };
	struct FTCharFromAnsi { explicit FTCharFromAnsi(const ANSICHAR* Str); const TCHAR* Get() const; int32 Length() const; };
	struct FTCharFromUtf8 { explicit FTCharFromUtf8(const ANSICHAR* Str); explicit FTCharFromUtf8(const UTF8CHAR* Str); const TCHAR* Get() const; int32 Length() const; };
}
#define TCHAR_TO_ANSI(str) (ANSICHAR*)UE::Stub::FAnsiCastResult(static_cast<const TCHAR*>(str)).Get()
#define ANSI_TO_TCHAR(str) (TCHAR*)UE::Stub::FTCharFromAnsi(static_cast<const ANSICHAR*>(str)).Get()
#define TCHAR_TO_UTF8(str) (ANSICHAR*)UE::Stub::FUtf8CastResult(static_cast<const TCHAR*>(str)).Get()
#define UTF8_TO_TCHAR(str) (TCHAR*)UE::Stub::FTCharFromUtf8(str).Get()
