// Stub SDK. Real headers: Engine/Source/Runtime/Core/Public/Misc/VarArgs.h, Templates/IsValidVariadicFunctionArg.h,
// Templates/IsArrayOrRefOfTypeByPredicate.h, Traits/IsCharEncodingCompatibleWith.h, Misc/FormatStringSan.h.
// From UE 5.x source knowledge.
//
// printf-style entry points of the engine (UE_LOG, FString::Printf, checkf, ensureMsgf, ...) refuse at compile time:
//   * a format that is not a TCHAR string literal / array ("Formatting string must be a TCHAR array");
//   * arguments that cannot go through C varargs (FString, FName, FText, any class type: TIsValidVariadicFunctionArg);
//   * since 5.4, a specifier that does not match its argument (UE_VALIDATE_FORMAT_STRING / FormatStringSan: '%s' needs
//     a TCHAR*, '%hs' / '%S' an ANSICHAR* / UTF8CHAR*, '%d' an integer or enum, '%f' a float / double, '%p' a pointer,
//     missing or extra arguments).
// UE::Core::Private::FormatStringSan below is a faithful-in-spirit reimplementation of the last check (the status names
// are the engine's); the first two are the engine's traits verbatim.
#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTypeTraits.h"

template <typename T, bool = TIsEnum<T>::Value>
struct TIsValidVariadicFunctionArg
{
private:
	static uint32 Tester(uint32) UE_STUB_BODY
	static uint32 Tester(uint8) UE_STUB_BODY
	static uint32 Tester(int32) UE_STUB_BODY
	static uint32 Tester(uint64) UE_STUB_BODY
	static uint32 Tester(int64) UE_STUB_BODY
	static uint32 Tester(double) UE_STUB_BODY
	static uint32 Tester(long) UE_STUB_BODY
	static uint32 Tester(unsigned long) UE_STUB_BODY
	static uint32 Tester(TCHAR) UE_STUB_BODY
	static uint32 Tester(bool) UE_STUB_BODY
	static uint32 Tester(const void*) UE_STUB_BODY
	static uint8 Tester(...) UE_STUB_BODY
	static T DeclValT() UE_STUB_BODY
public:
	enum { Value = sizeof(Tester(DeclValT())) == sizeof(uint32) };
};
template <typename T>
struct TIsValidVariadicFunctionArg<T, true>
{
	enum { Value = true };
};

template <typename SrcEncoding, typename DestEncoding>
struct TIsCharEncodingCompatibleWith { static constexpr bool Value = std::is_same_v<std::remove_cv_t<SrcEncoding>, std::remove_cv_t<DestEncoding>>; };
template <typename SrcEncoding> using TIsCharEncodingCompatibleWithTCHAR = TIsCharEncodingCompatibleWith<SrcEncoding, TCHAR>;

template <typename T, template <typename> class Predicate>
struct TIsArrayOrRefOfTypeByPredicate { enum { Value = false }; };
template <typename ArrType, template <typename> class Predicate>
struct TIsArrayOrRefOfTypeByPredicate<ArrType[], Predicate> { enum { Value = Predicate<ArrType>::Value }; };
template <typename ArrType, unsigned int N, template <typename> class Predicate>
struct TIsArrayOrRefOfTypeByPredicate<ArrType[N], Predicate> { enum { Value = Predicate<ArrType>::Value }; };
template <typename ArrType, unsigned int N, template <typename> class Predicate>
struct TIsArrayOrRefOfTypeByPredicate<ArrType (&)[N], Predicate> { enum { Value = Predicate<ArrType>::Value }; };
template <typename ArrType, unsigned int N, template <typename> class Predicate>
struct TIsArrayOrRefOfTypeByPredicate<const ArrType (&)[N], Predicate> { enum { Value = Predicate<ArrType>::Value }; };

namespace UE::Core::Private::FormatStringSan
{
	// Each of these is deliberately NOT constexpr: reaching one inside the consteval check is the compile error, and
	// its name is the message (the engine reports the same conditions through static_assert texts).
	void StatusIncompleteFormatSpecifierOrUnescapedPercent();
	void StatusSNeedsTCHARPtrVarArg_UseStarOnFString();
	void StatusSNeedsTCHARPtrButGotAnsiCharPtr_UseHS();
	void StatusHSNeedsAnsiCharPtrOrUtf8CharPtrVarArg();
	void StatusLSNeedsWideCharPtrVarArg();
	void StatusCNeedsTCHARVarArg();
	void StatusPNeedsPointerVarArg();
	void StatusFNeedsFloatOrDoubleVarArg();
	void StatusDNeedsIntegerArg();
	void StatusDynamicLengthSpecNeedsIntegerArg();
	void StatusInvalidFormatSpec();
	void StatusNotEnoughArguments();
	void StatusTooManyArguments();
	// Not engine statuses: the opt-in width lint (`./run.sh --define UE_STUB_STRICT_FORMAT_WIDTH=1`). The engine only
	// checks that '%d' gets an integer, so a 64-bit argument (FMath::RoundToInt(double), int64, SIZE_T) under '%d'
	// compiles; it is still a width mismatch for C varargs. The lint lists those places; a normal run accepts them.
	void StrictWidth_DGot64BitInteger_UseLLDOrA32BitValue();
	void StrictWidth_LLDGot32BitInteger();

	enum class EArgKind : uint8 { Integer, Floating, TCharPtr, AnsiCharPtr, WideCharPtr, OtherPtr, Other };
	template <typename T>
	consteval EArgKind KindOf()
	{
		using D = std::decay_t<T>;
		if (std::is_pointer_v<D>)
		{
			using P = std::remove_cv_t<std::remove_pointer_t<D>>;
			if (std::is_same_v<P, TCHAR>) { return EArgKind::TCharPtr; }
			if (std::is_same_v<P, ANSICHAR> || std::is_same_v<P, UTF8CHAR>) { return EArgKind::AnsiCharPtr; }
			if (std::is_same_v<P, WIDECHAR>) { return EArgKind::WideCharPtr; }
			return EArgKind::OtherPtr;
		}
		if (std::is_null_pointer_v<D>) { return EArgKind::OtherPtr; }
		if (std::is_integral_v<D> || std::is_enum_v<D>) { return EArgKind::Integer; }
		if (std::is_floating_point_v<D>) { return EArgKind::Floating; }
		return EArgKind::Other;
	}

	template <typename CharType, typename... Ts>
	consteval void Check(const CharType* Fmt)
	{
		const EArgKind Kinds[sizeof...(Ts) + 1] = { KindOf<Ts>()..., EArgKind::Other };
#if defined(UE_STUB_STRICT_FORMAT_WIDTH) && UE_STUB_STRICT_FORMAT_WIDTH
		const SIZE_T Sizes[sizeof...(Ts) + 1] = { sizeof(std::decay_t<Ts>)..., 0 };
#endif
		const int32 NumArgs = static_cast<int32>(sizeof...(Ts));
		int32 Arg = 0;
		for (; *Fmt != 0; ++Fmt)
		{
			if (*Fmt != CharType('%')) { continue; }
			++Fmt;
			if (*Fmt == CharType('%')) { continue; }
			// flags
			while (*Fmt == CharType('-') || *Fmt == CharType('+') || *Fmt == CharType(' ') || *Fmt == CharType('#') || *Fmt == CharType('0')) { ++Fmt; }
			// width
			if (*Fmt == CharType('*'))
			{
				if (Arg >= NumArgs) { StatusNotEnoughArguments(); }
				if (Kinds[Arg] != EArgKind::Integer) { StatusDynamicLengthSpecNeedsIntegerArg(); }
				++Arg; ++Fmt;
			}
			while (*Fmt >= CharType('0') && *Fmt <= CharType('9')) { ++Fmt; }
			// precision
			if (*Fmt == CharType('.'))
			{
				++Fmt;
				if (*Fmt == CharType('*'))
				{
					if (Arg >= NumArgs) { StatusNotEnoughArguments(); }
					if (Kinds[Arg] != EArgKind::Integer) { StatusDynamicLengthSpecNeedsIntegerArg(); }
					++Arg; ++Fmt;
				}
				while (*Fmt >= CharType('0') && *Fmt <= CharType('9')) { ++Fmt; }
			}
			// length
			bool bH = false, bL = false, bIntOnly = false;
			[[maybe_unused]] bool b64 = false;
			if (*Fmt == CharType('h')) { bH = true; ++Fmt; if (*Fmt == CharType('h')) { bIntOnly = true; ++Fmt; } }
			else if (*Fmt == CharType('l')) { bL = true; ++Fmt; if (*Fmt == CharType('l')) { bIntOnly = true; b64 = true; ++Fmt; } }
			else if (*Fmt == CharType('z') || *Fmt == CharType('t') || *Fmt == CharType('j')) { bIntOnly = true; b64 = true; ++Fmt; }
			else if (*Fmt == CharType('I') && Fmt[1] == CharType('6') && Fmt[2] == CharType('4')) { bIntOnly = true; b64 = true; Fmt += 3; }
			if (*Fmt == 0) { StatusIncompleteFormatSpecifierOrUnescapedPercent(); return; }
			if (Arg >= NumArgs) { StatusNotEnoughArguments(); return; }
			const EArgKind Kind = Kinds[Arg];
			switch (*Fmt)
			{
			case CharType('s'):
				if (bIntOnly) { StatusInvalidFormatSpec(); }
				else if (bH) { if (Kind != EArgKind::AnsiCharPtr) { StatusHSNeedsAnsiCharPtrOrUtf8CharPtrVarArg(); } }
				else if (bL) { if (Kind != EArgKind::WideCharPtr && Kind != EArgKind::TCharPtr) { StatusLSNeedsWideCharPtrVarArg(); } }
				else if (Kind == EArgKind::AnsiCharPtr) { StatusSNeedsTCHARPtrButGotAnsiCharPtr_UseHS(); }
				else if (Kind != EArgKind::TCharPtr) { StatusSNeedsTCHARPtrVarArg_UseStarOnFString(); }
				break;
			case CharType('S'):
				if (Kind != EArgKind::AnsiCharPtr) { StatusHSNeedsAnsiCharPtrOrUtf8CharPtrVarArg(); }
				break;
			case CharType('c'):
				if (Kind != EArgKind::Integer) { StatusCNeedsTCHARVarArg(); }
				break;
			case CharType('d'): case CharType('i'): case CharType('u'): case CharType('x'): case CharType('X'): case CharType('o'):
				if (Kind != EArgKind::Integer) { StatusDNeedsIntegerArg(); }
#if defined(UE_STUB_STRICT_FORMAT_WIDTH) && UE_STUB_STRICT_FORMAT_WIDTH
				else if (!b64 && !bL && Sizes[Arg] > 4) { StrictWidth_DGot64BitInteger_UseLLDOrA32BitValue(); }
				else if (b64 && Sizes[Arg] < 8) { StrictWidth_LLDGot32BitInteger(); }
#endif
				break;
			case CharType('f'): case CharType('F'): case CharType('e'): case CharType('E'): case CharType('g'): case CharType('G'): case CharType('a'): case CharType('A'):
				if (bIntOnly || bH) { StatusInvalidFormatSpec(); }
				else if (Kind != EArgKind::Floating) { StatusFNeedsFloatOrDoubleVarArg(); }
				break;
			case CharType('p'):
				if (Kind != EArgKind::OtherPtr && Kind != EArgKind::TCharPtr && Kind != EArgKind::AnsiCharPtr && Kind != EArgKind::WideCharPtr) { StatusPNeedsPointerVarArg(); }
				break;
			default:
				StatusInvalidFormatSpec();
				break;
			}
			++Arg;
		}
		if (Arg < NumArgs) { StatusTooManyArguments(); }
	}
}

namespace UE::Core
{
	/**
	 * A format string checked against its argument types when it is constructed (consteval): only a string literal /
	 * constant array of the right character type converts, and its specifiers must match Types.
	 */
	template <typename CharType, typename... Types>
	struct TCheckedFormatStringPrivate
	{
		template <SIZE_T N>
		consteval TCheckedFormatStringPrivate(const CharType (&Fmt)[N]) : FormatString(Fmt)
		{
			static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to a printf-style function (FString, FName, FText and other class types cannot go through varargs: use *String / *Name.ToString())");
			UE::Core::Private::FormatStringSan::Check<CharType, Types...>(Fmt);
		}
		const CharType* FormatString;
	};
	template <typename CharType, typename... Types>
	using TCheckedFormatString = TCheckedFormatStringPrivate<CharType, std::type_identity_t<Types>...>;
}

/** What UE_VALIDATE_FORMAT_STRING does for the macro entry points (UE_LOG, checkf, ensureMsgf, ...). */
#define UE_VALIDATE_FORMAT_STRING(Format, ...) \
	static_assert(TIsArrayOrRefOfTypeByPredicate<decltype(Format), TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array."); \
	(void)UE::Core::Private::FormatStringSan::Validate(Format, ##__VA_ARGS__)
namespace UE::Core::Private::FormatStringSan
{
	template <typename... Types>
	void ValidateImpl(UE::Core::TCheckedFormatString<TCHAR, Types...> Fmt, const Types&... Args) UE_STUB_BODY
	// Never evaluated (sizeof operand): only the conversion of the literal to TCheckedFormatString is checked.
	#define UE_STUB_FORMAT_CHECK(Format, ...) ((void)sizeof((UE::Core::Private::FormatStringSan::ValidateImpl(Format, ##__VA_ARGS__), 0)))
}
