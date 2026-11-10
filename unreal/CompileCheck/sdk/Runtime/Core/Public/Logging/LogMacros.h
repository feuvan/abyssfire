// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Logging/LogMacros.h (from UE 5.x source knowledge).
// UE_LOG / UE_CLOG and the log category macros. As in the engine: the category must be a declared category object, the
// verbosity a bare ELogVerbosity name (Fatal, Error, Warning, Display, Log, Verbose, VeryVerbose), the format a TCHAR
// literal and the arguments printf-compatible (Misc/VarArgs.h). With NO_LOGGING (Shipping / Test) the category is an
// empty FNoLoggingCategory and only Fatal lines do anything, but the format and arguments are still compiled (dead
// branch), exactly like the engine.
#pragma once

#include "CoreTypes.h"
#include "Logging/LogCategory.h"
#include "Logging/LogVerbosity.h"
#include "Misc/AssertionMacros.h"
#include "Misc/VarArgs.h"

struct FMsg
{
	template <typename... Types>
	static void Logf(const ANSICHAR* File, int32 Line, const FLogCategoryName& Category, ELogVerbosity::Type Verbosity, UE::Core::TCheckedFormatString<TCHAR, Types...> Fmt, Types... Args) UE_STUB_BODY
	static void SendNotificationString(const TCHAR* Message);
};

namespace UE::Stub
{
	template <typename CategoryType>
	concept CLogCategory = std::is_base_of_v<FLogCategoryBase, std::remove_cvref_t<CategoryType>>;
}

#if NO_LOGGING
	#define UE_LOG_ACTIVE(Category, Verbosity) (false)
	#define UE_GET_LOG_VERBOSITY(Category) (ELogVerbosity::NoLogging)
	#define UE_LOG(CategoryName, Verbosity, Format, ...) \
		{ \
			if (ELogVerbosity::Verbosity == ELogVerbosity::Fatal) \
			{ \
				LowLevelFatalError(Format, ##__VA_ARGS__); \
			} \
		}
	#define UE_CLOG(Condition, CategoryName, Verbosity, Format, ...) \
		{ \
			if (ELogVerbosity::Verbosity == ELogVerbosity::Fatal) \
			{ \
				if (Condition) \
				{ \
					LowLevelFatalError(Format, ##__VA_ARGS__); \
				} \
			} \
		}
	#define UE_LOG_REF(Category, Verbosity, Format, ...) UE_LOG(Category, Verbosity, Format, ##__VA_ARGS__)
	#define UE_SUPPRESS(CategoryName, Verbosity, ExecuteIfUnsuppressed)
	struct FNoLoggingCategory {};
	#define DECLARE_LOG_CATEGORY_EXTERN(CategoryName, DefaultVerbosity, CompileTimeVerbosity) extern FNoLoggingCategory CategoryName;
	#define DEFINE_LOG_CATEGORY(CategoryName, ...) FNoLoggingCategory CategoryName;
	#define DEFINE_LOG_CATEGORY_STATIC(CategoryName, DefaultVerbosity, CompileTimeVerbosity) static FNoLoggingCategory CategoryName;
	#define DECLARE_LOG_CATEGORY_CLASS(CategoryName, DefaultVerbosity, CompileTimeVerbosity) DEFINE_LOG_CATEGORY_STATIC(CategoryName, DefaultVerbosity, CompileTimeVerbosity)
	#define DEFINE_LOG_CATEGORY_CLASS(Class, CategoryName) FNoLoggingCategory Class::CategoryName;
#else
	#define UE_LOG_ACTIVE(CategoryName, Verbosity) \
		(((ELogVerbosity::Verbosity & ELogVerbosity::VerbosityMask) <= ELogVerbosity::All) && !CategoryName.IsSuppressed(ELogVerbosity::Verbosity))
	#define UE_GET_LOG_VERBOSITY(CategoryName) CategoryName.GetVerbosity()
	#define UE_LOG(CategoryName, Verbosity, Format, ...) \
		{ \
			static_assert(UE::Stub::CLogCategory<decltype(CategoryName)>, "UE_LOG: the first argument must be a log category (DECLARE_LOG_CATEGORY_EXTERN / DEFINE_LOG_CATEGORY_STATIC)"); \
			static_assert(TIsArrayOrRefOfTypeByPredicate<decltype(Format), TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array."); \
			if (UE_LOG_ACTIVE(CategoryName, Verbosity)) \
			{ \
				FMsg::Logf(__FILE__, __LINE__, CategoryName.GetCategoryName(), ELogVerbosity::Verbosity, Format, ##__VA_ARGS__); \
			} \
		}
	#define UE_CLOG(Condition, CategoryName, Verbosity, Format, ...) \
		{ \
			static_assert(UE::Stub::CLogCategory<decltype(CategoryName)>, "UE_CLOG: the second argument must be a log category"); \
			static_assert(TIsArrayOrRefOfTypeByPredicate<decltype(Format), TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array."); \
			if (UE_LOG_ACTIVE(CategoryName, Verbosity)) \
			{ \
				if (Condition) \
				{ \
					FMsg::Logf(__FILE__, __LINE__, CategoryName.GetCategoryName(), ELogVerbosity::Verbosity, Format, ##__VA_ARGS__); \
				} \
			} \
		}
	#define UE_LOG_REF(CategoryRef, Verbosity, Format, ...) UE_LOG(CategoryRef, Verbosity, Format, ##__VA_ARGS__)
	#define UE_SUPPRESS(CategoryName, Verbosity, ExecuteIfUnsuppressed) { if (UE_LOG_ACTIVE(CategoryName, Verbosity)) { ExecuteIfUnsuppressed; } }
	#define DECLARE_LOG_CATEGORY_EXTERN(CategoryName, DefaultVerbosity, CompileTimeVerbosity) \
		extern struct FLogCategory##CategoryName : public FLogCategory<ELogVerbosity::DefaultVerbosity, ELogVerbosity::CompileTimeVerbosity> \
		{ \
			FLogCategory##CategoryName() : FLogCategory(TEXT(#CategoryName)) {} \
		} CategoryName;
	#define DEFINE_LOG_CATEGORY(CategoryName, ...) FLogCategory##CategoryName CategoryName;
	#define DEFINE_LOG_CATEGORY_STATIC(CategoryName, DefaultVerbosity, CompileTimeVerbosity) \
		static struct FLogCategory##CategoryName : public FLogCategory<ELogVerbosity::DefaultVerbosity, ELogVerbosity::CompileTimeVerbosity> \
		{ \
			FLogCategory##CategoryName() : FLogCategory(TEXT(#CategoryName)) {} \
		} CategoryName;
	#define DECLARE_LOG_CATEGORY_CLASS(CategoryName, DefaultVerbosity, CompileTimeVerbosity) DEFINE_LOG_CATEGORY_STATIC(CategoryName, DefaultVerbosity, CompileTimeVerbosity)
	#define DEFINE_LOG_CATEGORY_CLASS(Class, CategoryName) Class::FLogCategory##CategoryName Class::CategoryName;
#endif
