#include "abyss/base/Platform.h"

#include "abyss/base/I18n.h"

#include <cmath>

#include "abyss/base/Json.h"

namespace abyss {

LocText MakeLoc(std::string key) {
  LocText t;
  t.key = std::move(key);
  return t;
}

LocText MakeLoc(std::string key, std::initializer_list<I18nArg> args) {
  LocText t;
  t.key = std::move(key);
  t.args.assign(args.begin(), args.end());
  return t;
}

std::string FormatI18nNumber(double v) { return FormatJsonNumber(v); }

std::string FormatFixed(double v, int decimals) {
  if (decimals < 0) decimals = 0;
  if (decimals > 6) decimals = 6;
  if (!std::isfinite(v)) return FormatJsonNumber(v);
  static constexpr double kPow[7] = {1, 10, 100, 1000, 10000, 100000, 1000000};
  const bool neg = v < 0;
  const double a = std::fabs(v);
  // JS toFixed picks n with n/10^f - x closest to zero, larger n on a tie: floor(a*10^f + 0.5) on the double product.
  const double n = std::floor(a * kPow[decimals] + 0.5);
  // Magnitudes whose scaled value does not fit uint64 (>= 2^63 keeps a margin; the cast is UB from ~1.8e19) fall back to
  // the shortest round-trip text. JS would print every digit up to 1e21; no game number comes close.
  if (!(n < 9223372036854775808.0)) return FormatJsonNumber(v);
  const uint64_t ni = static_cast<uint64_t>(n);
  const uint64_t p = static_cast<uint64_t>(kPow[decimals]);
  std::string out;
  if (neg && ni != 0) out.push_back('-');
  out += std::to_string(ni / p);
  if (decimals > 0) {
    out.push_back('.');
    std::string frac = std::to_string(ni % p);
    while (static_cast<int>(frac.size()) < decimals) frac.insert(frac.begin(), '0');
    out += frac;
  }
  return out;
}

std::string I18n::Substitute(std::string_view text, std::span<const I18nArg> args) {
  if (args.empty() || text.find('{') == std::string_view::npos) return std::string(text);
  std::string out;
  out.reserve(text.size() + 16);
  size_t i = 0;
  while (i < text.size()) {
    const char c = text[i];
    if (c == '{') {
      const size_t close = text.find('}', i + 1);
      if (close != std::string_view::npos) {
        const std::string_view name = text.substr(i + 1, close - i - 1);
        const I18nArg* hit = nullptr;
        for (const I18nArg& a : args) {
          if (a.name == name) {
            hit = &a;
            break;
          }
        }
        if (hit) {
          out += hit->value;  // literal replacement (no $& patterns)
          i = close + 1;
          continue;
        }
      }
    }
    out.push_back(c);
    ++i;
  }
  return out;
}

bool I18n::LoadTable(std::string_view json, std::string* err) {
  JsonValue root;
  JsonParseError perr;
  if (!ParseJson(json, root, &perr)) {
    if (err) *err = "i18n: JSON parse error at " + std::to_string(perr.offset) + ": " + perr.message;
    return false;
  }
  LocaleId locale{};
  if (!ParseEnum(root.Get("locale").AsString(), locale)) {
    if (err) *err = "i18n: missing or unknown 'locale'";
    return false;
  }
  const JsonValue& strings = root.Get("strings");
  if (!strings.IsObject()) {
    if (err) *err = "i18n: 'strings' must be an object";
    return false;
  }
  Table t;
  t.loaded = true;
  for (const JsonValue& f : root.Get("fallback").Items()) {
    LocaleId fl{};
    if (ParseEnum(f.AsString(), fl) && fl != locale) t.fallback.push_back(fl);
  }
  t.values.reserve(strings.Size());
  for (const JsonMember& m : strings.Members()) {
    if (!m.value.IsString()) {
      if (err) *err = "i18n: value of '" + m.key + "' is not a string";
      return false;
    }
    if (t.index.Add(m.key)) t.values.emplace_back(m.value.AsString());
  }
  tables_[EnumIndex(locale)] = std::move(t);
  return true;
}

bool I18n::HasLocale(LocaleId id) const { return tables_[EnumIndex(id)].loaded; }

void I18n::SetLocale(LocaleId id) {
  if (HasLocale(id)) current_ = id;
}

const std::string* I18n::Lookup(LocaleId locale, std::string_view key) const {
  const Table& t = tables_[EnumIndex(locale)];
  if (!t.loaded) return nullptr;
  const int32_t i = t.index.Find(key);
  return i >= 0 ? &t.values[static_cast<size_t>(i)] : nullptr;
}

const std::string* I18n::Resolve(std::string_view key) const {
  if (const std::string* s = Lookup(current_, key)) return s;
  const Table& t = tables_[EnumIndex(current_)];
  if (t.loaded) {
    for (LocaleId f : t.fallback) {
      if (const std::string* s = Lookup(f, key)) return s;
    }
  } else {
    // Current table not loaded: web default chain zh-CN -> en.
    if (const std::string* s = Lookup(LocaleId::ZhCN, key)) return s;
    if (const std::string* s = Lookup(LocaleId::En, key)) return s;
  }
  return nullptr;
}

std::string I18n::T(std::string_view key, std::span<const I18nArg> args) const {
  const std::string* s = Resolve(key);
  if (!s) return std::string(key);
  bool anyKey = false;
  for (const I18nArg& a : args) anyKey = anyKey || a.isKey;
  if (!anyKey) return Substitute(*s, args);
  std::vector<I18nArg> resolved(args.begin(), args.end());
  for (I18nArg& a : resolved) {
    if (a.isKey) {
      a.value = T(a.value);
      a.isKey = false;
    }
  }
  return Substitute(*s, resolved);
}

bool I18n::Has(std::string_view key) const { return Resolve(key) != nullptr; }

std::string I18n::NameOr(std::string_view key, std::string_view fallback) const {
  const std::string* s = Resolve(key);
  return s ? *s : std::string(fallback);
}

size_t I18n::KeyCount(LocaleId locale) const { return tables_[EnumIndex(locale)].values.size(); }

}  // namespace abyss
