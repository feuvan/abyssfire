#include "abyss/base/Platform.h"

#include "JsonReader.h"

#include <cmath>
#include <limits>

namespace abyss::dataload {

void LoadCtx::Error(const std::string& path, std::string message) {
  ++errors_;
  report_->errors.push_back(DataIssue{file_, path, std::move(message)});
}

void LoadCtx::Warn(const std::string& path, std::string message) {
  report_->warnings.push_back(DataIssue{file_, path, std::move(message)});
}

namespace {

const JsonValue& NullJson() {
  static const JsonValue v;
  return v;
}

std::string TypeName(const JsonValue* v) {
  if (!v) return "missing";
  switch (v->Type()) {
    case JsonType::Null: return "null";
    case JsonType::Bool: return "bool";
    case JsonType::Number: return "number";
    case JsonType::String: return "string";
    case JsonType::Array: return "array";
    case JsonType::Object: return "object";
  }
  return "?";
}

}  // namespace

const JsonValue& JNode::V() const { return v_ ? *v_ : NullJson(); }

JNode JNode::Child(std::string_view key) const {
  std::string p = path_.empty() ? std::string(key) : path_ + "." + std::string(key);
  const JsonValue* c = (v_ && v_->IsObject()) ? v_->Find(key) : nullptr;
  return JNode(c, ctx_, std::move(p));
}

bool JNode::Has(std::string_view key) const {
  const JsonValue* c = (v_ && v_->IsObject()) ? v_->Find(key) : nullptr;
  return c && !c->IsNull();
}

double JNode::AsNum() const {
  if (!v_ || !v_->IsNumber()) {
    Error("expected a number, got " + TypeName(v_));
    return 0.0;
  }
  return v_->AsDouble();
}

int32_t JNode::AsInt() const {
  if (!v_ || !v_->IsNumber()) {
    Error("expected an integer, got " + TypeName(v_));
    return 0;
  }
  const double d = v_->AsDouble();
  if (std::trunc(d) != d || d < std::numeric_limits<int32_t>::min() || d > std::numeric_limits<int32_t>::max()) {
    Error("expected an int32 integer, got " + FormatJsonNumber(d));
    return 0;
  }
  return static_cast<int32_t>(d);
}

int64_t JNode::AsI64() const {
  if (!v_ || !v_->IsNumber() || !v_->IsInteger()) {
    Error("expected an integer, got " + TypeName(v_));
    return 0;
  }
  return v_->AsInt64();
}

bool JNode::AsBool() const {
  if (!v_ || !v_->IsBool()) {
    Error("expected a bool, got " + TypeName(v_));
    return false;
  }
  return v_->AsBool();
}

std::string JNode::AsStr() const {
  if (!v_ || !v_->IsString()) {
    Error("expected a string, got " + TypeName(v_));
    return {};
  }
  return std::string(v_->AsString());
}

uint32_t JNode::AsColor() const {
  if (!v_ || !v_->IsInteger() || v_->AsInt64() < 0 || v_->AsInt64() > 0xFFFFFF) {
    Error("expected a 0xRRGGBB integer colour, got " + TypeName(v_));
    return 0;
  }
  return static_cast<uint32_t>(v_->AsInt64());
}

TilePos JNode::AsTile() const {
  if (!v_ || !v_->IsObject()) {
    Error("expected {col,row}, got " + TypeName(v_));
    return {};
  }
  return {Int("col"), Int("row")};
}

TileCircle JNode::AsCircle() const {
  if (!v_ || !v_->IsObject()) {
    Error("expected {col,row,radius}, got " + TypeName(v_));
    return {};
  }
  return {Int("col"), Int("row"), Num("radius")};
}

double JNode::Num(std::string_view key, double def) const { return Has(key) ? Child(key).AsNum() : def; }
int32_t JNode::Int(std::string_view key, int32_t def) const { return Has(key) ? Child(key).AsInt() : def; }
int64_t JNode::I64(std::string_view key, int64_t def) const { return Has(key) ? Child(key).AsI64() : def; }
bool JNode::Bool(std::string_view key, bool def) const { return Has(key) ? Child(key).AsBool() : def; }
std::string JNode::Str(std::string_view key, std::string_view def) const {
  return Has(key) ? Child(key).AsStr() : std::string(def);
}

std::vector<JNode> JNode::SelfItems(bool allowMissing) const {
  std::vector<JNode> out;
  if (!v_ || v_->IsNull()) {
    if (!allowMissing) Error("expected an array, got " + TypeName(v_));
    return out;
  }
  if (!v_->IsArray()) {
    Error("expected an array, got " + TypeName(v_));
    return out;
  }
  const auto items = v_->Items();
  out.reserve(items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    out.emplace_back(&items[i], ctx_, path_ + "[" + std::to_string(i) + "]");
  }
  return out;
}

std::vector<std::pair<std::string, JNode>> JNode::SelfMembers(bool allowMissing) const {
  std::vector<std::pair<std::string, JNode>> out;
  if (!v_ || v_->IsNull()) {
    if (!allowMissing) Error("expected an object, got " + TypeName(v_));
    return out;
  }
  if (!v_->IsObject()) {
    Error("expected an object, got " + TypeName(v_));
    return out;
  }
  for (const JsonMember& m : v_->Members()) {
    out.emplace_back(m.key, JNode(&m.value, ctx_, path_ + "." + m.key));
  }
  return out;
}

std::vector<std::string> JNode::StrList(std::string_view key, bool allowMissing) const {
  std::vector<std::string> out;
  for (const JNode& n : Items(key, allowMissing)) out.push_back(n.AsStr());
  return out;
}

std::vector<int32_t> JNode::IntList(std::string_view key, bool allowMissing) const {
  std::vector<int32_t> out;
  for (const JNode& n : Items(key, allowMissing)) out.push_back(n.AsInt());
  return out;
}

std::vector<double> JNode::NumList(std::string_view key, bool allowMissing) const {
  std::vector<double> out;
  for (const JNode& n : Items(key, allowMissing)) out.push_back(n.AsNum());
  return out;
}

StatBag JNode::Stats(std::string_view key, bool allowMissing) const {
  StatBag bag;
  for (const auto& [k, n] : Members(key, allowMissing)) {
    Stat s{};
    if (!ParseEnum(k, s)) {
      n.Error("unknown stat key '" + k + "'");
      continue;
    }
    bag.Add(s, n.AsNum());
  }
  return bag;
}

JNode Root(const JsonValue& doc, LoadCtx& ctx) {
  JNode r(&doc, &ctx, "");
  if (!doc.IsObject()) {
    ctx.Error("", "root must be an object");
    return r;
  }
  const int32_t v = r.Int("schemaVersion", -1);
  if (v != kDataSchemaVersion) {
    ctx.Error("schemaVersion", "unsupported schemaVersion " + std::to_string(v) + " (expected " +
                                   std::to_string(kDataSchemaVersion) + ")");
  }
  return r;
}

}  // namespace abyss::dataload
