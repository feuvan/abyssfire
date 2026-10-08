// JSON reader/writer for the core (DECISIONS U3, ue58-platform.md 3.3).
//
// * Parsing uses vendored RapidJSON (namespace abyss_rapidjson, exceptions off) through its SAX reader in iterative
//   mode with numbers delivered as raw text; numbers are converted by the core's own locale-independent decimal parser
//   (ParseJsonNumber: Clinger fast path + exact big-integer fallback; never strtod/atof/float from_chars).
// * The DOM (JsonValue) keeps object members in DOCUMENT ORDER (JS insertion order matters for data tables and saves).
// * No RapidJSON type ever appears in a public header.
// * Writing uses RapidJSON's writer; numbers are written by FormatJsonNumber (integral values without a fraction,
//   other values as the shortest round-trip form).
// * Nesting is limited to kMaxJsonDepth containers: the parser itself is iterative, but the JsonValue tree is
//   destroyed, copied, compared and written recursively, so a corrupt or hostile file (a save edited by hand, a 40 KB
//   run of '[') must be rejected as a parse error instead of overflowing a small (mobile / worker thread) stack.
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"

namespace abyss {

enum class JsonType : uint8_t { Null, Bool, Number, String, Array, Object };

// Maximum container nesting accepted by ParseJson (saves and data tables use < 10 levels).
inline constexpr size_t kMaxJsonDepth = 128;

struct JsonMember;

class ABYSS_API JsonValue {
 public:
  JsonValue();
  ~JsonValue();
  JsonValue(const JsonValue&);
  JsonValue(JsonValue&&) noexcept;
  JsonValue& operator=(const JsonValue&);
  JsonValue& operator=(JsonValue&&) noexcept;

  // ---- construction ----
  static JsonValue Null();
  static JsonValue Bool(bool b);
  static JsonValue Number(double d);
  static JsonValue Integer(int64_t i);
  static JsonValue String(std::string_view s);
  static JsonValue Array();
  static JsonValue Object();

  // ---- type queries ----
  JsonType Type() const { return type_; }
  bool IsNull() const { return type_ == JsonType::Null; }
  bool IsBool() const { return type_ == JsonType::Bool; }
  bool IsNumber() const { return type_ == JsonType::Number; }
  bool IsString() const { return type_ == JsonType::String; }
  bool IsArray() const { return type_ == JsonType::Array; }
  bool IsObject() const { return type_ == JsonType::Object; }
  // A number whose value is integral and fits in int64 (e.g. 3, -7, 1e3; not 2.5).
  bool IsInteger() const;

  // ---- scalar access (return `def` on a type mismatch) ----
  bool AsBool(bool def = false) const;
  double AsDouble(double def = 0.0) const;
  int64_t AsInt64(int64_t def = 0) const;   // integral numbers only
  int32_t AsInt(int32_t def = 0) const;     // integral numbers in int32 range only
  std::string_view AsString(std::string_view def = {}) const;
  // The number's source text when it was parsed (empty for constructed numbers).
  std::string_view RawNumberText() const;

  // ---- arrays ----
  size_t Size() const;  // array length or object member count; 0 otherwise
  // Element i, or a shared Null value when out of range / not an array.
  const JsonValue& At(size_t i) const;
  std::span<const JsonValue> Items() const;
  std::span<JsonValue> MutableItems();
  void Append(JsonValue v);  // converts a Null value into an Array first

  // ---- objects (linear member lookup; tables are small) ----
  const JsonValue* Find(std::string_view key) const;
  JsonValue* FindMutable(std::string_view key);
  // Member value, or a shared Null value when missing / not an object.
  const JsonValue& Get(std::string_view key) const;
  bool Has(std::string_view key) const { return Find(key) != nullptr; }
  std::span<const JsonMember> Members() const;
  std::span<JsonMember> MutableMembers();
  // Sets (replaces in place, keeping the position) or appends a member. Converts a Null value into an Object first.
  void Set(std::string_view key, JsonValue v);
  bool Remove(std::string_view key);
  // Appends a member without the duplicate-key check of Set (parser / writers that know keys are unique).
  void AppendMember(std::string_view key, JsonValue v);
  // Records the source text of a parsed number (RawNumberText).
  void SetRawNumberText(std::string_view text);

  bool operator==(const JsonValue& o) const;

 private:
  JsonType type_ = JsonType::Null;
  bool bool_ = false;
  bool isInt_ = false;
  double num_ = 0.0;
  int64_t int_ = 0;
  std::string str_;  // string value, or the raw number text
  std::vector<JsonValue> arr_;
  std::vector<JsonMember> obj_;
};

struct ABYSS_API JsonMember {
  std::string key;
  JsonValue value;
  bool operator==(const JsonMember& o) const;
};

struct JsonParseError {
  size_t offset = 0;      // byte offset of the error
  std::string message;    // English, for logs
};

// Parses UTF-8 JSON text (no comments, no NaN/Infinity, validated UTF-8, at most kMaxJsonDepth nested containers).
// Returns false and fills `err` on failure.
ABYSS_API bool ParseJson(std::string_view text, JsonValue& out, JsonParseError* err = nullptr);

// Serialises a value. pretty = 2-space indentation like the exporter's files.
ABYSS_API std::string WriteJson(const JsonValue& v, bool pretty = false);

// In-house decimal parser for JSON number text (`-?digits(.digits)?([eE][+-]?digits)?`). Correctly rounded
// (round-half-even), locale independent. Returns false for malformed text or values out of double range.
ABYSS_API bool ParseJsonNumber(std::string_view text, double& out);
// Integer parsing of JSON integer text into int64 (std::from_chars). False if not an integer or out of range.
ABYSS_API bool ParseJsonInteger(std::string_view text, int64_t& out);
// Writes a double: integral |v| < 2^53 as an integer ("3", "-12"); otherwise the shortest text that parses back to the
// same double. NaN/Inf are written as null (JSON has no representation; JSON.stringify does the same).
ABYSS_API std::string FormatJsonNumber(double v);

// Streaming writer (saves): builds compact or pretty JSON text in document order.
// Self-validating: a call that would produce invalid JSON (EndObject / EndArray that does not match the open container,
// a Key outside an object or twice in a row, a value in an object without its Key, anything after the root value is
// complete) reports ABYSS_ASSERT and is IGNORED, so misuse in one save section can never corrupt memory or the text of
// the others (RapidJSON's own asserts abort). Take() returns "" while containers are still open.
class ABYSS_API JsonWriter {
 public:
  explicit JsonWriter(bool pretty = false);
  ~JsonWriter();
  JsonWriter(const JsonWriter&) = delete;
  JsonWriter& operator=(const JsonWriter&) = delete;

  void StartObject();
  void EndObject();
  void StartArray();
  void EndArray();
  void Key(std::string_view k);
  void Null();
  void Bool(bool b);
  void Int(int64_t i);
  void Double(double d);  // FormatJsonNumber rules
  void String(std::string_view s);
  void Value(const JsonValue& v);
  // Returns the text written so far once the root value is complete; "" while a container is still open.
  std::string Take();
  // True once a complete root value was written and no container is open.
  bool Complete() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace abyss
