// Source scanning helpers shared by the content gates: content_ch1_test.cpp ("content isolation": no test file outside
// the content layer spells a Chapter 1 id) and content_hooks_test.cpp (K1 grep gate: no Chapter 1 id is hard-coded in
// the core). A C++ source's string literals (raw strings included; comments and character literals skipped) and a
// whole-token match inside one.
#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace abyss::test::src {

inline bool SrcIdChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

// One string literal of a C++ source: its contents (escapes kept as written) and whether it is a doctest title (the
// first argument of TEST_SUITE / TEST_CASE / SUBCASE), which names a test rather than pinning data.
struct SrcLiteral {
  std::string text;
  bool title = false;
};

// The string literals of a C++ source, raw strings included; comments and character literals are skipped.
inline std::vector<SrcLiteral> SrcStringLiterals(const std::string& src) {
  std::vector<SrcLiteral> out;
  const size_t n = src.size();
  const auto titled = [&src](size_t quote) {
    size_t k = quote;
    while (k > 0 && (src[k - 1] == ' ' || src[k - 1] == '\n')) --k;
    if (k == 0 || src[k - 1] != '(') return false;
    --k;
    for (const std::string_view macro : {std::string_view("TEST_SUITE"), std::string_view("TEST_CASE"),
                                         std::string_view("SUBCASE")}) {
      if (k >= macro.size() && src.compare(k - macro.size(), macro.size(), macro) == 0) return true;
    }
    return false;
  };
  size_t i = 0;
  while (i < n) {
    const char c = src[i];
    const char next = i + 1 < n ? src[i + 1] : '\0';
    if (c == '/' && next == '/') {
      i = src.find('\n', i);
      if (i == std::string::npos) break;
    } else if (c == '/' && next == '*') {
      i = src.find("*/", i + 2);
      if (i == std::string::npos) break;
      i += 2;
    } else if (c == '\'') {
      if (i > 0 && SrcIdChar(src[i - 1])) {  // a digit separator (1'000)
        ++i;
        continue;
      }
      size_t j = i + 1;
      while (j < n && src[j] != '\'') j += src[j] == '\\' ? 2 : 1;
      i = j + 1;
    } else if (c == 'R' && next == '"' && (i == 0 || !SrcIdChar(src[i - 1]))) {
      const size_t open = src.find('(', i + 2);
      if (open == std::string::npos) break;
      const std::string close = ")" + src.substr(i + 2, open - i - 2) + "\"";
      const size_t end = src.find(close, open + 1);
      if (end == std::string::npos) break;
      out.push_back(SrcLiteral{src.substr(open + 1, end - open - 1), titled(i)});
      i = end + close.size();
    } else if (c == '"') {
      size_t j = i + 1;
      while (j < n && src[j] != '"' && src[j] != '\n') j += src[j] == '\\' ? 2 : 1;
      out.push_back(SrcLiteral{src.substr(i + 1, (std::min)(j, n) - i - 1), titled(i)});
      i = j + 1;
    } else {
      ++i;
    }
  }
  return out;
}

// `id` stands in `text` as a whole token: "goblin", "kill:goblin", "data.monster.goblin" and "hidden_x#1" spell their
// ids; "monster_goblin" (an art key) and "goblin_chief" do not spell "goblin".
inline bool SrcHasToken(std::string_view text, std::string_view id) {
  for (size_t p = text.find(id); p != std::string_view::npos; p = text.find(id, p + 1)) {
    const size_t end = p + id.size();
    if ((p == 0 || !SrcIdChar(text[p - 1])) && (end == text.size() || !SrcIdChar(text[end]))) return true;
  }
  return false;
}

}  // namespace abyss::test::src
