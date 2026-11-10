// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/AssertionMacros.h (from UE 5.x source knowledge).
// check / verify / ensure. As in the engine: with DO_CHECK 0 (Shipping, Test) check() and checkf() do not evaluate or
// even compile their arguments; verify() still evaluates its expression; ensure() always yields the bool.
#pragma once

#include "CoreTypes.h"
#include "Misc/VarArgs.h"

struct FDebug
{
	static void AssertFailed(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Format = TEXT(""), ...);
	static void DumpStackTraceToLog(const int32 Verbosity);
	static bool HasAsserted();
	static bool IsEnsuring();
};
namespace UE::Stub
{
	[[noreturn]] void CheckFailed(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line);
	bool EnsureFailed(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line);
}

#define CA_ASSUME(Expr)

#if DO_CHECK
	#define checkCode(Code) do { Code; } while (false);
	#define verify(expr) UE_CHECK_IMPL(expr)
	#define check(expr) UE_CHECK_IMPL(expr)
	#define UE_CHECK_IMPL(expr) { if (UNLIKELY(!(expr))) { UE::Stub::CheckFailed(#expr, __FILE__, __LINE__); } }
	#define verifyf(expr, format, ...) UE_CHECK_F_IMPL(expr, format, ##__VA_ARGS__)
	#define checkf(expr, format, ...) UE_CHECK_F_IMPL(expr, format, ##__VA_ARGS__)
	#define UE_CHECK_F_IMPL(expr, format, ...) \
		{ \
			static_assert(TIsArrayOrRefOfTypeByPredicate<decltype(format), TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array."); \
			UE_STUB_FORMAT_CHECK(format, ##__VA_ARGS__); \
			if (UNLIKELY(!(expr))) { UE::Stub::CheckFailed(#expr, __FILE__, __LINE__); } \
		}
	#define checkNoEntry() { UE::Stub::CheckFailed("Enclosing block should never be called", __FILE__, __LINE__); }
	#define checkNoReentry() { static bool s_beenHere##__LINE__ = false; check(!s_beenHere##__LINE__); s_beenHere##__LINE__ = true; }
	#define checkNoRecursion()
	#define unimplemented() { UE::Stub::CheckFailed("Unimplemented function called", __FILE__, __LINE__); }
#else
	#define checkCode(...)
	#define check(expr) { CA_ASSUME(expr); }
	#define checkf(expr, format, ...) { CA_ASSUME(expr); }
	#define checkNoEntry()
	#define checkNoReentry()
	#define checkNoRecursion()
	#define verify(expr) { if (UNLIKELY(!(expr))) { CA_ASSUME(false); } }
	#define verifyf(expr, format, ...) { if (UNLIKELY(!(expr))) { CA_ASSUME(false); } }
	#define unimplemented()
#endif

#if DO_GUARD_SLOW
	#define checkSlow(expr) check(expr)
	#define checkfSlow(expr, format, ...) checkf(expr, format, ##__VA_ARGS__)
	#define verifySlow(expr) check(expr)
#else
	#define checkSlow(expr) { CA_ASSUME(expr); }
	#define checkfSlow(expr, format, ...) { CA_ASSUME(expr); }
	#define verifySlow(expr) { if (UNLIKELY(!(expr))) { CA_ASSUME(false); } }
#endif

#if DO_ENSURE
	#define UE_ENSURE_IMPL(InExpression) (LIKELY(!!(InExpression)) || UE::Stub::EnsureFailed(#InExpression, __FILE__, __LINE__))
	#define UE_ENSURE_F_IMPL(InExpression, InFormat, ...) \
		(LIKELY(!!(InExpression)) || ([&]() -> bool \
		{ \
			static_assert(TIsArrayOrRefOfTypeByPredicate<decltype(InFormat), TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array."); \
			UE_STUB_FORMAT_CHECK(InFormat, ##__VA_ARGS__); \
			return UE::Stub::EnsureFailed(#InExpression, __FILE__, __LINE__); \
		})())
	#define ensure(InExpression) UE_ENSURE_IMPL(InExpression)
	#define ensureMsgf(InExpression, InFormat, ...) UE_ENSURE_F_IMPL(InExpression, InFormat, ##__VA_ARGS__)
	#define ensureAlways(InExpression) UE_ENSURE_IMPL(InExpression)
	#define ensureAlwaysMsgf(InExpression, InFormat, ...) UE_ENSURE_F_IMPL(InExpression, InFormat, ##__VA_ARGS__)
#else
	#define ensure(InExpression) (LIKELY(!!(InExpression)))
	#define ensureMsgf(InExpression, InFormat, ...) (LIKELY(!!(InExpression)))
	#define ensureAlways(InExpression) (LIKELY(!!(InExpression)))
	#define ensureAlwaysMsgf(InExpression, InFormat, ...) (LIKELY(!!(InExpression)))
#endif

#define LowLevelFatalError(Format, ...) \
	{ \
		static_assert(TIsArrayOrRefOfTypeByPredicate<decltype(Format), TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array."); \
		UE_STUB_FORMAT_CHECK(Format, ##__VA_ARGS__); \
		UE::Stub::CheckFailed("LowLevelFatalError", __FILE__, __LINE__); \
	}

#define UE_STATIC_ASSERT_COMPLETE_TYPE(TypeToCheck, ...) static_assert(sizeof(TypeToCheck), ##__VA_ARGS__)
