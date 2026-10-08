// Small string helpers (no iostreams, no <format>; ue58-platform.md 3.3 "Formatting").
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "abyss/base/Platform.h"

namespace abyss {

ABYSS_API std::string ToStrI64(int64_t v);
ABYSS_API std::string ToStrU64(uint64_t v);
// Every integer type (int, long, long long, size_t, ptrdiff_t, int8_t, ...). One constrained template instead of fixed
// overloads: on Apple (and Windows) int64_t is `long long` while size_t is `unsigned long`, so fixed int64/uint64
// overloads make `ToStr(vec.size())` ambiguous there while Linux (int64_t == long) compiles. bool and char are not
// numbers here.
template <class T>
  requires(std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char>)
std::string ToStr(T v) {
  if constexpr (std::is_signed_v<T>) {
    return ToStrI64(static_cast<int64_t>(v));
  } else {
    return ToStrU64(static_cast<uint64_t>(v));
  }
}
// Shortest round-trip double text (same rules as FormatJsonNumber).
ABYSS_API std::string ToStr(double v);
inline std::string ToStr(std::string_view v) { return std::string(v); }
inline std::string ToStr(const char* v) { return std::string(v); }
inline std::string ToStr(const std::string& v) { return v; }

// Concatenates the textual form of every argument.
template <class... Args>
std::string StrCat(const Args&... args) {
  std::string out;
  (out.append(ToStr(args)), ...);
  return out;
}

ABYSS_API bool StartsWith(std::string_view s, std::string_view prefix);
ABYSS_API bool EndsWith(std::string_view s, std::string_view suffix);
ABYSS_API bool Contains(std::string_view s, std::string_view needle);
ABYSS_API std::vector<std::string_view> Split(std::string_view s, char sep);
// Lower-case hex of a 64-bit value without leading zeros ("0" for 0).
ABYSS_API std::string ToHex(uint64_t v);
// Parses a decimal integer (std::from_chars); false on any trailing garbage.
ABYSS_API bool ParseInt(std::string_view s, int64_t& out);

}  // namespace abyss
