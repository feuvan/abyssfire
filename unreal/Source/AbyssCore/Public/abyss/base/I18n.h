// Localisation (save-ui-input.md 10, ue58-platform.md 9.7).
//
// * Tables are the exported Data/i18n_<locale>.json files: {locale, fallback: [locales], placeholder: "{name}",
//   strings: {key: text}}. zh-CN is the source of truth, en complete, zh-TW generated at export time.
// * T(key, args) = web t(): look up the current locale, then each locale of its `fallback` chain, then return the key
//   itself (callers detect "missing" by comparing to the key). Every `{name}` placeholder whose name is in `args` is
//   replaced LITERALLY (no `$&` interpretation, fixing web quirk Q27); unknown placeholders stay as written.
// * All text out of the core is i18n keys + args (ARCHITECTURE 3.2); UE calls T() to render it.
#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"

namespace abyss {

// One `{name}` substitution. Numbers are passed pre-formatted (FormatI18nNumber or the caller's own format).
// isKey: `value` is itself an i18n key (zone / monster / item / quest names) that T() resolves in the current locale
// before substituting, so core events stay locale-free.
struct I18nArg {
  std::string name;
  std::string value;
  bool isKey = false;
  bool operator==(const I18nArg& o) const = default;
};

// An argument whose value is an i18n key (resolved at render time).
inline I18nArg KeyArg(std::string name, std::string key) { return I18nArg{std::move(name), std::move(key), true}; }

// A localisable message: key + args. Events and logs carry these; UE renders them with I18n::T.
struct LocText {
  std::string key;
  std::vector<I18nArg> args;
  bool Empty() const { return key.empty(); }
  bool operator==(const LocText& o) const = default;
};

ABYSS_API LocText MakeLoc(std::string key);
ABYSS_API LocText MakeLoc(std::string key, std::initializer_list<I18nArg> args);

// JS String(number) for the common cases: integral values without a fraction, others shortest round-trip.
ABYSS_API std::string FormatI18nNumber(double v);
// JS Number.prototype.toFixed(decimals) (decimals 0..6) while |v| * 10^decimals < 2^63; larger magnitudes (and NaN /
// Infinity) fall back to FormatJsonNumber.
ABYSS_API std::string FormatFixed(double v, int decimals);

class ABYSS_API I18n {
 public:
  I18n() = default;

  // Loads one locale table from the exported JSON bytes. Returns false (err filled) on parse/shape errors.
  bool LoadTable(std::string_view json, std::string* err = nullptr);
  bool HasLocale(LocaleId id) const;
  // Switches the current locale (ignored when that table is not loaded). Default: zh-CN.
  void SetLocale(LocaleId id);
  LocaleId Current() const { return current_; }

  // web t(key, params).
  std::string T(std::string_view key, std::span<const I18nArg> args = {}) const;
  std::string T(const LocText& text) const { return T(text.key, text.args); }
  // True when the key resolves in the current locale or its fallback chain.
  bool Has(std::string_view key) const;
  // Raw lookup in one table without fallback (nullptr when missing).
  const std::string* Lookup(LocaleId locale, std::string_view key) const;
  // "key or fallback" accessor pattern (save-ui-input.md 10.5): T(key) unless it is missing, else `fallback`.
  std::string NameOr(std::string_view key, std::string_view fallback) const;
  size_t KeyCount(LocaleId locale) const;

  // Literal `{name}` substitution used by T (exposed for tests and for data-side templates).
  static std::string Substitute(std::string_view text, std::span<const I18nArg> args);

 private:
  struct Table {
    bool loaded = false;
    std::vector<LocaleId> fallback;
    IdIndex index;                    // key -> position in `values`
    std::vector<std::string> values;
  };
  const std::string* Resolve(std::string_view key) const;

  std::array<Table, EnumCount<LocaleId>()> tables_{};
  LocaleId current_ = LocaleId::ZhCN;
};

}  // namespace abyss
