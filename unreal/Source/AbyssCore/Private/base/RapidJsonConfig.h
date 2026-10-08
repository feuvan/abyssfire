// Private: the ONLY place that includes RapidJSON (DECISIONS U3). Include from .cpp files of the core only.
//
// * Namespace renamed to abyss_rapidjson so it can never clash with another RapidJSON copy linked into UE/plugins.
// * Exceptions stay off (RapidJSON never throws unless RAPIDJSON_PARSE_ERROR_NORETURN is overridden to throw).
// * RAPIDJSON_ASSERT routes to the core's assert handler and then ABORTS: RapidJSON's Writer / Stack code assumes a failed
//   assert does not return and would otherwise run past its buffers (UE installs a returning, ensure-style handler).
//   JsonWriter validates every call itself (see Json.cpp), so a RapidJSON assert means a core bug, never bad input.
// * Warnings from the third-party headers are silenced locally; the core itself builds with -Werror.
#pragma once

#include <cstdlib>

#include "abyss/base/Assert.h"

#ifdef RAPIDJSON_NAMESPACE
#error "RapidJSON must only be configured by RapidJsonConfig.h"
#endif
#define RAPIDJSON_NAMESPACE abyss_rapidjson
#define RAPIDJSON_HAS_STDSTRING 1
#define RAPIDJSON_ASSERT(x) \
  ((x) ? static_cast<void>(0) : (::abyss::ReportAssertFailure(__FILE__, __LINE__, #x, "rapidjson"), std::abort()))

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wall"
#pragma clang diagnostic ignored "-Wextra"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#pragma clang diagnostic ignored "-Wdeprecated-copy"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#endif

#include "../../ThirdParty/rapidjson/rapidjson/error/en.h"
#include "../../ThirdParty/rapidjson/rapidjson/internal/dtoa.h"
#include "../../ThirdParty/rapidjson/rapidjson/prettywriter.h"
#include "../../ThirdParty/rapidjson/rapidjson/reader.h"
#include "../../ThirdParty/rapidjson/rapidjson/stringbuffer.h"
#include "../../ThirdParty/rapidjson/rapidjson/writer.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
