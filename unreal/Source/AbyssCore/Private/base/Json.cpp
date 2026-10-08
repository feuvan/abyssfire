#include "abyss/base/Platform.h"

#include "abyss/base/Json.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "RapidJsonConfig.h"
#include "abyss/base/Assert.h"

namespace abyss {

namespace rj = ::abyss_rapidjson;

// =====================================================================================================================
// JsonValue
// =====================================================================================================================

JsonValue::JsonValue() = default;
JsonValue::~JsonValue() = default;
JsonValue::JsonValue(const JsonValue&) = default;
JsonValue::JsonValue(JsonValue&&) noexcept = default;
JsonValue& JsonValue::operator=(const JsonValue&) = default;
JsonValue& JsonValue::operator=(JsonValue&&) noexcept = default;

bool JsonMember::operator==(const JsonMember& o) const { return key == o.key && value == o.value; }

namespace {

const JsonValue& NullValue() {
  static const JsonValue v;
  return v;
}

bool IntegralInInt64(double d) {
  return std::isfinite(d) && std::trunc(d) == d && d >= -9223372036854775808.0 && d < 9223372036854775808.0;
}

}  // namespace

JsonValue JsonValue::Null() { return JsonValue(); }

JsonValue JsonValue::Bool(bool b) {
  JsonValue v;
  v.type_ = JsonType::Bool;
  v.bool_ = b;
  return v;
}

JsonValue JsonValue::Number(double d) {
  JsonValue v;
  v.type_ = JsonType::Number;
  v.num_ = d;
  if (IntegralInInt64(d)) {
    v.isInt_ = true;
    v.int_ = static_cast<int64_t>(d);
  }
  return v;
}

JsonValue JsonValue::Integer(int64_t i) {
  JsonValue v;
  v.type_ = JsonType::Number;
  v.num_ = static_cast<double>(i);
  v.isInt_ = true;
  v.int_ = i;
  return v;
}

JsonValue JsonValue::String(std::string_view s) {
  JsonValue v;
  v.type_ = JsonType::String;
  v.str_.assign(s);
  return v;
}

JsonValue JsonValue::Array() {
  JsonValue v;
  v.type_ = JsonType::Array;
  return v;
}

JsonValue JsonValue::Object() {
  JsonValue v;
  v.type_ = JsonType::Object;
  return v;
}

bool JsonValue::IsInteger() const { return type_ == JsonType::Number && isInt_; }

bool JsonValue::AsBool(bool def) const { return type_ == JsonType::Bool ? bool_ : def; }

double JsonValue::AsDouble(double def) const { return type_ == JsonType::Number ? num_ : def; }

int64_t JsonValue::AsInt64(int64_t def) const { return IsInteger() ? int_ : def; }

int32_t JsonValue::AsInt(int32_t def) const {
  if (!IsInteger()) return def;
  if (int_ < std::numeric_limits<int32_t>::min() || int_ > std::numeric_limits<int32_t>::max()) return def;
  return static_cast<int32_t>(int_);
}

std::string_view JsonValue::AsString(std::string_view def) const {
  return type_ == JsonType::String ? std::string_view(str_) : def;
}

std::string_view JsonValue::RawNumberText() const {
  return type_ == JsonType::Number ? std::string_view(str_) : std::string_view{};
}

size_t JsonValue::Size() const {
  if (type_ == JsonType::Array) return arr_.size();
  if (type_ == JsonType::Object) return obj_.size();
  return 0;
}

const JsonValue& JsonValue::At(size_t i) const {
  if (type_ != JsonType::Array || i >= arr_.size()) return NullValue();
  return arr_[i];
}

std::span<const JsonValue> JsonValue::Items() const {
  if (type_ != JsonType::Array) return {};
  return arr_;
}

std::span<JsonValue> JsonValue::MutableItems() {
  if (type_ != JsonType::Array) return {};
  return arr_;
}

void JsonValue::Append(JsonValue v) {
  if (type_ == JsonType::Null) type_ = JsonType::Array;
  if (type_ != JsonType::Array) return;
  arr_.push_back(std::move(v));
}

const JsonValue* JsonValue::Find(std::string_view key) const {
  if (type_ != JsonType::Object) return nullptr;
  for (const JsonMember& m : obj_) {
    if (m.key == key) return &m.value;
  }
  return nullptr;
}

JsonValue* JsonValue::FindMutable(std::string_view key) {
  if (type_ != JsonType::Object) return nullptr;
  for (JsonMember& m : obj_) {
    if (m.key == key) return &m.value;
  }
  return nullptr;
}

const JsonValue& JsonValue::Get(std::string_view key) const {
  const JsonValue* v = Find(key);
  return v ? *v : NullValue();
}

std::span<const JsonMember> JsonValue::Members() const {
  if (type_ != JsonType::Object) return {};
  return obj_;
}

std::span<JsonMember> JsonValue::MutableMembers() {
  if (type_ != JsonType::Object) return {};
  return obj_;
}

void JsonValue::Set(std::string_view key, JsonValue v) {
  if (type_ == JsonType::Null) type_ = JsonType::Object;
  if (type_ != JsonType::Object) return;
  for (JsonMember& m : obj_) {
    if (m.key == key) {
      m.value = std::move(v);
      return;
    }
  }
  obj_.push_back(JsonMember{std::string(key), std::move(v)});
}

bool JsonValue::Remove(std::string_view key) {
  if (type_ != JsonType::Object) return false;
  for (size_t i = 0; i < obj_.size(); ++i) {
    if (obj_[i].key == key) {
      obj_.erase(obj_.begin() + static_cast<std::ptrdiff_t>(i));
      return true;
    }
  }
  return false;
}

bool JsonValue::operator==(const JsonValue& o) const {
  if (type_ != o.type_) return false;
  switch (type_) {
    case JsonType::Null: return true;
    case JsonType::Bool: return bool_ == o.bool_;
    case JsonType::Number: return num_ == o.num_;
    case JsonType::String: return str_ == o.str_;
    case JsonType::Array: return arr_ == o.arr_;
    case JsonType::Object: return obj_ == o.obj_;
  }
  return false;
}

// =====================================================================================================================
// Number parsing (in-house, locale independent; ue58-platform.md 3.3 "Number parsing")
// =====================================================================================================================

namespace {

// Minimal unsigned big integer (little-endian 32-bit limbs) for the exact slow path.
class BigUint {
 public:
  BigUint() = default;
  explicit BigUint(uint64_t v) {
    while (v) {
      limbs_.push_back(static_cast<uint32_t>(v));
      v >>= 32;
    }
  }
  void MulSmall(uint32_t m) {
    uint64_t carry = 0;
    for (uint32_t& l : limbs_) {
      const uint64_t p = static_cast<uint64_t>(l) * m + carry;
      l = static_cast<uint32_t>(p);
      carry = p >> 32;
    }
    if (carry) limbs_.push_back(static_cast<uint32_t>(carry));
  }
  void AddSmall(uint32_t a) {
    uint64_t carry = a;
    for (size_t i = 0; i < limbs_.size() && carry; ++i) {
      const uint64_t s = static_cast<uint64_t>(limbs_[i]) + carry;
      limbs_[i] = static_cast<uint32_t>(s);
      carry = s >> 32;
    }
    if (carry) limbs_.push_back(static_cast<uint32_t>(carry));
  }
  void MulPow5(int n) {
    while (n >= 13) {
      MulSmall(1220703125u);  // 5^13
      n -= 13;
    }
    static constexpr uint32_t kPow5[13] = {1u, 5u, 25u, 125u, 625u, 3125u, 15625u, 78125u, 390625u, 1953125u,
                                           9765625u, 48828125u, 244140625u};
    if (n > 0) MulSmall(kPow5[n]);
  }
  void ShiftLeft(int bits) {
    if (bits <= 0 || limbs_.empty()) return;
    const int words = bits / 32;
    const int rem = bits % 32;
    if (rem) {
      uint32_t carry = 0;
      for (uint32_t& l : limbs_) {
        const uint32_t nc = l >> (32 - rem);
        l = (l << rem) | carry;
        carry = nc;
      }
      if (carry) limbs_.push_back(carry);
    }
    if (words) limbs_.insert(limbs_.begin(), static_cast<size_t>(words), 0u);
  }
  static int Compare(const BigUint& a, const BigUint& b) {
    if (a.limbs_.size() != b.limbs_.size()) return a.limbs_.size() < b.limbs_.size() ? -1 : 1;
    for (size_t i = a.limbs_.size(); i-- > 0;) {
      if (a.limbs_[i] != b.limbs_[i]) return a.limbs_[i] < b.limbs_[i] ? -1 : 1;
    }
    return 0;
  }

 private:
  std::vector<uint32_t> limbs_;  // no leading zero limbs (zero = empty)
};

constexpr double kExactPow10[23] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
                                    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

// x * 10^e by chunks of exact powers (approximation for the slow path; refined afterwards).
double ScalePow10(double x, int e) {
  while (e > 22) {
    x *= 1e22;
    e -= 22;
  }
  while (e < -22) {
    x /= 1e22;
    e += 22;
  }
  return e >= 0 ? x * kExactPow10[e] : x / kExactPow10[-e];
}

// Compares D*10^E with (num * 2^e2) where num is a positive integer. Returns -1, 0, 1.
int CompareDecimalWithBinary(const std::string& digits, int E, uint64_t num, int e2) {
  BigUint lhs;
  for (char c : digits) {
    lhs.MulSmall(10);
    lhs.AddSmall(static_cast<uint32_t>(c - '0'));
  }
  BigUint rhs(num);
  const int ePos = E > 0 ? E : 0;
  const int eNeg = E < 0 ? -E : 0;
  lhs.MulPow5(ePos);
  rhs.MulPow5(eNeg);
  // powers of two: lhs has 2^ePos, rhs has 2^(eNeg + e2)
  const int t = ePos - (eNeg + e2);
  if (t >= 0) {
    lhs.ShiftLeft(t);
  } else {
    rhs.ShiftLeft(-t);
  }
  return BigUint::Compare(lhs, rhs);
}

struct Decomposed {
  uint64_t mant;  // integer significand
  int e2;         // value = mant * 2^e2
};

Decomposed Decompose(double x) {
  const uint64_t bits = std::bit_cast<uint64_t>(x);
  const int be = static_cast<int>((bits >> 52) & 0x7FF);
  const uint64_t frac = bits & ((uint64_t{1} << 52) - 1);
  if (be == 0) return {frac, -1074};
  return {frac | (uint64_t{1} << 52), be - 1075};
}

// Exact correction of an estimate x (positive, finite or 0) of D*10^E. Returns false when out of double range.
bool RefineDecimal(const std::string& digits, int E, double x, double& out) {
  const double maxD = std::numeric_limits<double>::max();
  const double minSub = std::numeric_limits<double>::denorm_min();
  if (!(x > 0)) x = minSub;
  if (std::isinf(x)) x = maxD;
  for (int iter = 0; iter < 4000; ++iter) {
    const Decomposed d = Decompose(x);
    // Upper halfway point between x and its successor: (2m+1) * 2^(e2-1).
    const int cUp = CompareDecimalWithBinary(digits, E, 2 * d.mant + 1, d.e2 - 1);
    if (cUp > 0) {
      if (x == maxD) return false;  // rounds to infinity
      x = std::nextafter(x, std::numeric_limits<double>::infinity());
      continue;
    }
    if (cUp == 0) {
      // Tie with the successor: round half to even.
      if (d.mant & 1u) {
        if (x == maxD) return false;
        x = std::nextafter(x, std::numeric_limits<double>::infinity());
      }
      out = x;
      return true;
    }
    // Lower halfway point between the predecessor and x.
    uint64_t lowNum;
    int lowE2;
    const bool powerOfTwoBoundary = (d.mant == (uint64_t{1} << 52)) && d.e2 > -1074;
    if (powerOfTwoBoundary) {
      lowNum = 4 * d.mant - 1;  // predecessor spacing is half the ulp
      lowE2 = d.e2 - 2;
    } else {
      lowNum = 2 * d.mant - 1;
      lowE2 = d.e2 - 1;
    }
    const int cLow = CompareDecimalWithBinary(digits, E, lowNum, lowE2);
    if (cLow > 0) {
      out = x;
      return true;
    }
    if (cLow == 0) {
      // Tie with the predecessor: pick the even one.
      if (d.mant & 1u) {
        x = std::nextafter(x, 0.0);
      }
      out = x;
      return true;
    }
    if (x == minSub) {
      out = 0.0;  // below half of the smallest subnormal
      return true;
    }
    x = std::nextafter(x, 0.0);
  }
  return false;
}

}  // namespace

bool ParseJsonNumber(std::string_view text, double& out) {
  size_t i = 0;
  const size_t n = text.size();
  bool neg = false;
  if (i < n && text[i] == '-') {
    neg = true;
    ++i;
  }
  if (i >= n) return false;
  std::string digits;  // significant digits (leading zeros stripped)
  int fracDigits = 0;
  bool any = false;
  // integer part: "0" or [1-9][0-9]*
  if (text[i] == '0') {
    any = true;
    ++i;
  } else if (text[i] >= '1' && text[i] <= '9') {
    while (i < n && text[i] >= '0' && text[i] <= '9') {
      digits.push_back(text[i]);
      any = true;
      ++i;
    }
  } else {
    return false;
  }
  if (i < n && text[i] == '.') {
    ++i;
    bool fracAny = false;
    while (i < n && text[i] >= '0' && text[i] <= '9') {
      if (!(digits.empty() && text[i] == '0')) digits.push_back(text[i]);
      ++fracDigits;
      fracAny = true;
      ++i;
    }
    if (!fracAny) return false;
    // Leading zeros of the fraction were skipped, but they still count as fraction digits.
  }
  if (!any) return false;
  int64_t exp = 0;
  if (i < n && (text[i] == 'e' || text[i] == 'E')) {
    ++i;
    bool eneg = false;
    if (i < n && (text[i] == '+' || text[i] == '-')) {
      eneg = text[i] == '-';
      ++i;
    }
    bool expAny = false;
    while (i < n && text[i] >= '0' && text[i] <= '9') {
      if (exp < 100000000) exp = exp * 10 + (text[i] - '0');
      expAny = true;
      ++i;
    }
    if (!expAny) return false;
    if (eneg) exp = -exp;
  }
  if (i != n) return false;

  // digits holds the significant digits; when zeros were skipped inside the fraction (e.g. 0.005) the count of
  // fraction digits still positions the decimal point correctly because only LEADING zeros are dropped.
  // Strip trailing zeros.
  int64_t E = exp - fracDigits;
  while (!digits.empty() && digits.back() == '0') {
    digits.pop_back();
    ++E;
  }
  if (digits.empty()) {
    out = neg ? -0.0 : 0.0;
    return true;
  }
  if (digits.size() > 800) return false;  // absurdly long literal (never produced by our data or saves)
  const int64_t totalExp = E + static_cast<int64_t>(digits.size());  // value = 0.digits * 10^totalExp
  if (totalExp > 310) return false;                                  // > ~1e309: out of range
  if (totalExp < -330) {
    out = neg ? -0.0 : 0.0;  // below the smallest subnormal
    return true;
  }
  const int e = static_cast<int>(E);

  // Clinger fast path: exact significand (< 2^53) and an exact power of ten.
  if (digits.size() <= 19) {
    uint64_t m = 0;
    for (char c : digits) m = m * 10 + static_cast<uint64_t>(c - '0');
    if (m < (uint64_t{1} << 53)) {
      const double dm = static_cast<double>(m);
      if (e >= 0 && e <= 22) {
        out = neg ? -(dm * kExactPow10[e]) : dm * kExactPow10[e];
        return true;
      }
      if (e < 0 && e >= -22) {
        out = neg ? -(dm / kExactPow10[-e]) : dm / kExactPow10[-e];
        return true;
      }
      if (e > 22 && e <= 22 + 15) {
        // Move part of the exponent into the significand when it stays exact.
        uint64_t mm = m;
        int k = e - 22;
        bool exact = true;
        while (k-- > 0) {
          if (mm > ((uint64_t{1} << 53) - 1) / 10) {
            exact = false;
            break;
          }
          mm *= 10;
        }
        if (exact) {
          const double v = static_cast<double>(mm) * 1e22;
          out = neg ? -v : v;
          return true;
        }
      }
    }
  }

  // Slow path: estimate from the first 19 digits, then correct exactly.
  uint64_t m19 = 0;
  const size_t take = std::min<size_t>(digits.size(), 19);
  for (size_t k = 0; k < take; ++k) m19 = m19 * 10 + static_cast<uint64_t>(digits[k] - '0');
  const int eEst = e + static_cast<int>(digits.size() - take);
  const double est = ScalePow10(static_cast<double>(m19), eEst);
  double v = 0.0;
  if (!RefineDecimal(digits, e, est, v)) return false;
  out = neg ? -v : v;
  return true;
}

bool ParseJsonInteger(std::string_view text, int64_t& out) {
  if (text.empty()) return false;
  const char* b = text.data();
  const char* e = text.data() + text.size();
  auto res = std::from_chars(b, e, out);
  return res.ec == std::errc() && res.ptr == e;
}

std::string FormatJsonNumber(double v) {
  if (!std::isfinite(v)) return "null";
  if (v == 0.0) return "0";  // JSON.stringify(-0) === "0"
  if (std::trunc(v) == v && std::fabs(v) < 9007199254740992.0) {
    char buf[32];
    auto res = std::to_chars(buf, buf + sizeof(buf), static_cast<int64_t>(v));
    return std::string(buf, res.ptr);
  }
  char buf[64];
  char* end = rj::internal::dtoa(v, buf, 324);
  return std::string(buf, end);
}

// =====================================================================================================================
// Parsing (RapidJSON SAX -> JsonValue)
// =====================================================================================================================

namespace {

struct DomBuilder {
  JsonValue root;
  bool haveRoot = false;
  std::vector<JsonValue*> stack;
  std::string pendingKey;
  std::string error;

  bool Add(JsonValue v) {
    if (stack.empty()) {
      if (haveRoot) return false;
      root = std::move(v);
      haveRoot = true;
      if (root.IsArray() || root.IsObject()) stack.push_back(&root);
      return true;
    }
    JsonValue* top = stack.back();
    const bool container = v.IsArray() || v.IsObject();
    if (container && stack.size() >= kMaxJsonDepth) {
      // Depth limit (kMaxJsonDepth): the tree is destroyed / copied / written recursively, so reject deep input here.
      // Returning false stops RapidJSON with kParseErrorTermination; ParseJson reports `error`.
      error = "nesting too deep (more than " + std::to_string(kMaxJsonDepth) + " levels)";
      return false;
    }
    JsonValue* added = nullptr;
    if (top->IsArray()) {
      top->Append(std::move(v));
      added = &top->MutableItems().back();
    } else {
      // No duplicate-key check while parsing (the exporter never writes duplicates; Find returns the first one).
      top->AppendMember(pendingKey, std::move(v));
      added = &top->MutableMembers().back().value;
    }
    if (container) stack.push_back(added);
    return true;
  }

  bool Null() { return Add(JsonValue::Null()); }
  bool Bool(bool b) { return Add(JsonValue::Bool(b)); }
  bool Int(int i) { return Add(JsonValue::Integer(i)); }
  bool Uint(unsigned u) { return Add(JsonValue::Integer(static_cast<int64_t>(u))); }
  bool Int64(int64_t i) { return Add(JsonValue::Integer(i)); }
  bool Uint64(uint64_t u) { return Add(JsonValue::Number(static_cast<double>(u))); }
  bool Double(double d) { return Add(JsonValue::Number(d)); }
  bool RawNumber(const char* str, rj::SizeType len, bool /*copy*/) {
    const std::string_view text(str, len);
    double d = 0.0;
    if (!ParseJsonNumber(text, d)) {
      error = "number out of range or malformed: ";
      error.append(text);
      return false;
    }
    JsonValue v = JsonValue::Number(d);
    int64_t iv = 0;
    const bool looksInt = text.find_first_of(".eE") == std::string_view::npos;
    if (looksInt && ParseJsonInteger(text, iv)) v = JsonValue::Integer(iv);
    v.SetRawNumberText(text);
    return Add(std::move(v));
  }
  bool String(const char* str, rj::SizeType len, bool /*copy*/) {
    return Add(JsonValue::String(std::string_view(str, len)));
  }
  bool StartObject() { return Add(JsonValue::Object()); }
  bool Key(const char* str, rj::SizeType len, bool /*copy*/) {
    pendingKey.assign(str, len);
    return true;
  }
  bool EndObject(rj::SizeType) {
    if (stack.empty()) return false;
    stack.pop_back();
    return true;
  }
  bool StartArray() { return Add(JsonValue::Array()); }
  bool EndArray(rj::SizeType) {
    if (stack.empty()) return false;
    stack.pop_back();
    return true;
  }
};

}  // namespace

void JsonValue::SetRawNumberText(std::string_view text) {
  if (type_ == JsonType::Number) str_.assign(text);
}

void JsonValue::AppendMember(std::string_view key, JsonValue v) {
  if (type_ == JsonType::Null) type_ = JsonType::Object;
  if (type_ != JsonType::Object) return;
  obj_.push_back(JsonMember{std::string(key), std::move(v)});
}

bool ParseJson(std::string_view text, JsonValue& out, JsonParseError* err) {
  DomBuilder builder;
  rj::Reader reader;
  rj::MemoryStream ms(text.data(), text.size());
  rj::EncodedInputStream<rj::UTF8<>, rj::MemoryStream> is(ms);
  constexpr unsigned kFlags =
      rj::kParseIterativeFlag | rj::kParseNumbersAsStringsFlag | rj::kParseValidateEncodingFlag;
  const rj::ParseResult res = reader.Parse<kFlags>(is, builder);
  if (!res) {
    if (err) {
      err->offset = res.Offset();
      err->message = builder.error.empty() ? std::string(rj::GetParseError_En(res.Code())) : builder.error;
    }
    return false;
  }
  out = std::move(builder.root);
  return true;
}

// =====================================================================================================================
// Writing
// =====================================================================================================================

struct JsonWriter::Impl {
  rj::StringBuffer buffer;
  bool pretty = false;
  rj::Writer<rj::StringBuffer> compact;
  rj::PrettyWriter<rj::StringBuffer> prettyWriter;
  // Own validation state (RapidJSON's asserts abort): open containers ('{' / '['), whether the open object expects a
  // key next, and whether the root value is complete.
  std::vector<char> open;
  bool expectKey = false;
  bool rootDone = false;
  explicit Impl(bool p) : pretty(p), compact(buffer), prettyWriter(buffer) { prettyWriter.SetIndent(' ', 2); }

  template <class F>
  void Do(F&& f) {
    if (pretty) {
      f(prettyWriter);
    } else {
      f(compact);
    }
  }

  // A value (scalar or container start) may be written here: not after the root, not where an object expects a key.
  bool CanWriteValue(const char* what) const {
    if (rootDone) {
      ReportAssertFailure(__FILE__, __LINE__, what, "JsonWriter: value after the root value is complete (ignored)");
      return false;
    }
    if (!open.empty() && open.back() == '{' && expectKey) {
      ReportAssertFailure(__FILE__, __LINE__, what, "JsonWriter: value in an object without a Key (ignored)");
      return false;
    }
    return true;
  }
  // Bookkeeping after a complete value (scalar, or a container that just closed).
  void ValueDone() {
    if (open.empty()) {
      rootDone = true;
    } else if (open.back() == '{') {
      expectKey = true;
    }
  }
  void Opened(char c) {
    open.push_back(c);
    expectKey = c == '{';
  }
  bool CanClose(char c, const char* what) const {
    if (open.empty() || open.back() != c) {
      ReportAssertFailure(__FILE__, __LINE__, what, "JsonWriter: close does not match the open container (ignored)");
      return false;
    }
    if (c == '{' && !expectKey) {
      ReportAssertFailure(__FILE__, __LINE__, what, "JsonWriter: EndObject after a Key without its value (ignored)");
      return false;
    }
    return true;
  }
};

JsonWriter::JsonWriter(bool pretty) : impl_(std::make_unique<Impl>(pretty)) {}
JsonWriter::~JsonWriter() = default;

void JsonWriter::StartObject() {
  if (!impl_->CanWriteValue("StartObject")) return;
  impl_->Do([](auto& w) { w.StartObject(); });
  impl_->Opened('{');
}
void JsonWriter::EndObject() {
  if (!impl_->CanClose('{', "EndObject")) return;
  impl_->Do([](auto& w) { w.EndObject(); });
  impl_->open.pop_back();
  impl_->expectKey = false;
  impl_->ValueDone();
}
void JsonWriter::StartArray() {
  if (!impl_->CanWriteValue("StartArray")) return;
  impl_->Do([](auto& w) { w.StartArray(); });
  impl_->Opened('[');
}
void JsonWriter::EndArray() {
  if (!impl_->CanClose('[', "EndArray")) return;
  impl_->Do([](auto& w) { w.EndArray(); });
  impl_->open.pop_back();
  impl_->expectKey = false;
  impl_->ValueDone();
}
void JsonWriter::Key(std::string_view k) {
  if (impl_->open.empty() || impl_->open.back() != '{' || !impl_->expectKey) {
    ReportAssertFailure(__FILE__, __LINE__, "Key", "JsonWriter: Key outside an object or twice in a row (ignored)");
    return;
  }
  impl_->Do([&](auto& w) { w.Key(k.data(), static_cast<rj::SizeType>(k.size()), true); });
  impl_->expectKey = false;
}
void JsonWriter::Null() {
  if (!impl_->CanWriteValue("Null")) return;
  impl_->Do([](auto& w) { w.Null(); });
  impl_->ValueDone();
}
void JsonWriter::Bool(bool b) {
  if (!impl_->CanWriteValue("Bool")) return;
  impl_->Do([&](auto& w) { w.Bool(b); });
  impl_->ValueDone();
}
void JsonWriter::Int(int64_t i) {
  if (!impl_->CanWriteValue("Int")) return;
  impl_->Do([&](auto& w) { w.Int64(i); });
  impl_->ValueDone();
}
void JsonWriter::Double(double d) {
  const std::string s = FormatJsonNumber(d);
  if (s == "null") {
    Null();
    return;
  }
  if (!impl_->CanWriteValue("Double")) return;
  impl_->Do([&](auto& w) { w.RawValue(s.data(), s.size(), rj::kNumberType); });
  impl_->ValueDone();
}
void JsonWriter::String(std::string_view s) {
  if (!impl_->CanWriteValue("String")) return;
  impl_->Do([&](auto& w) { w.String(s.data(), static_cast<rj::SizeType>(s.size()), true); });
  impl_->ValueDone();
}

void JsonWriter::Value(const JsonValue& v) {
  switch (v.Type()) {
    case JsonType::Null: Null(); break;
    case JsonType::Bool: Bool(v.AsBool()); break;
    case JsonType::Number:
      if (v.IsInteger()) {
        Int(v.AsInt64());
      } else {
        Double(v.AsDouble());
      }
      break;
    case JsonType::String: String(v.AsString()); break;
    case JsonType::Array:
      StartArray();
      for (const JsonValue& e : v.Items()) Value(e);
      EndArray();
      break;
    case JsonType::Object:
      StartObject();
      for (const JsonMember& m : v.Members()) {
        Key(m.key);
        Value(m.value);
      }
      EndObject();
      break;
  }
}

bool JsonWriter::Complete() const { return impl_->rootDone && impl_->open.empty(); }

std::string JsonWriter::Take() {
  if (!Complete()) return std::string();
  return std::string(impl_->buffer.GetString(), impl_->buffer.GetSize());
}

std::string WriteJson(const JsonValue& v, bool pretty) {
  JsonWriter w(pretty);
  w.Value(v);
  return w.Take();
}

}  // namespace abyss
