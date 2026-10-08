#include "abyss/base/Platform.h"

#include "abyss/base/StrUtil.h"

#include <charconv>

#include "abyss/base/Json.h"

namespace abyss {

std::string ToStrI64(int64_t v) {
  char buf[32];
  auto res = std::to_chars(buf, buf + sizeof(buf), v);
  return std::string(buf, res.ptr);
}

std::string ToStrU64(uint64_t v) {
  char buf[32];
  auto res = std::to_chars(buf, buf + sizeof(buf), v);
  return std::string(buf, res.ptr);
}

std::string ToStr(double v) { return FormatJsonNumber(v); }

bool StartsWith(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

bool EndsWith(std::string_view s, std::string_view suffix) {
  return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

bool Contains(std::string_view s, std::string_view needle) { return s.find(needle) != std::string_view::npos; }

std::vector<std::string_view> Split(std::string_view s, char sep) {
  std::vector<std::string_view> out;
  size_t start = 0;
  for (size_t i = 0; i <= s.size(); ++i) {
    if (i == s.size() || s[i] == sep) {
      out.push_back(s.substr(start, i - start));
      start = i + 1;
    }
  }
  return out;
}

std::string ToHex(uint64_t v) {
  char buf[32];
  auto res = std::to_chars(buf, buf + sizeof(buf), v, 16);
  return std::string(buf, res.ptr);
}

bool ParseInt(std::string_view s, int64_t& out) {
  if (s.empty()) return false;
  const char* b = s.data();
  const char* e = s.data() + s.size();
  if (*b == '+') ++b;
  auto res = std::from_chars(b, e, out);
  return res.ec == std::errc() && res.ptr == e;
}

}  // namespace abyss
