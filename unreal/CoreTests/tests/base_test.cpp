// Foundation tests: base/ (types, enums, stats, math, units, RNG, clock, timers, JSON, i18n, logging, asserts).
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "TestUtil.h"
#include "abyss/base/Assert.h"
#include "abyss/base/Enums.h"
#include "abyss/base/I18n.h"
#include "abyss/base/Json.h"
#include "abyss/base/Log.h"
#include "abyss/base/Math.h"
#include "abyss/base/Rng.h"
#include "abyss/base/SimClock.h"
#include "abyss/base/Stats.h"
#include "abyss/base/StrUtil.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/base/Units.h"
#include "doctest/doctest.h"

using namespace abyss;

TEST_SUITE("base") {

// ---------------------------------------------------------------------------------------------------------------------
TEST_CASE("Vec2 / TilePos / RoundToTile") {
  Vec2 a{3, 4};
  CHECK(a.Length() == 5.0);
  CHECK(a.LengthSq() == 25.0);
  CHECK((a + Vec2{1, 1}) == Vec2{4, 5});
  CHECK((a * 2.0) == Vec2{6, 8});
  CHECK(a.Normalized().x == doctest::Approx(0.6));
  CHECK(Vec2{}.Normalized() == Vec2{});
  CHECK(Vec2{1, 0}.Cross(Vec2{0, 1}) == 1.0);
  // JS Math.round on negative halves goes toward +inf.
  CHECK(RoundToTile({-0.5, 2.5}) == TilePos{0, 3});
  CHECK(RoundToTile({-1.5, -2.4}) == TilePos{-1, -2});
  CHECK(TilePos{2, 3}.Center() == Vec2{2, 3});
}

TEST_CASE("IdIndex keeps data order and finds by binary search") {
  IdIndex idx;
  CHECK(idx.Add("goblin"));
  CHECK(idx.Add("slime_green"));
  CHECK(idx.Add("abyss"));
  CHECK_FALSE(idx.Add("goblin"));
  CHECK(idx.Size() == 3);
  CHECK(idx.Find("goblin") == 0);
  CHECK(idx.Find("slime_green") == 1);
  CHECK(idx.Find("abyss") == 2);
  CHECK(idx.Find("nope") == -1);
  CHECK(idx.IdAt(2) == "abyss");
}

TEST_CASE("Enums round-trip their data spellings") {
  ClassId c{};
  CHECK(ParseEnum("rogue", c));
  CHECK(c == ClassId::Rogue);
  CHECK(EnumName(ClassId::Mage) == "mage");
  CHECK_FALSE(ParseEnum("paladin", c));
  CHECK(EnumCount<EquipSlot>() == 10);
  CHECK(EnumName(EquipSlot::Ring2) == "ring2");
  CHECK(EnumName(LocaleId::ZhTW) == "zh-TW");
  ItemQuality q{};
  CHECK(ParseEnum("legendary", q));
  CHECK(QualityMeetsFloor(ItemQuality::Set, ItemQuality::Rare));
  CHECK_FALSE(QualityMeetsFloor(ItemQuality::Magic, ItemQuality::Rare));
  for (size_t i = 0; i < EnumCount<StatusType>(); ++i) {
    StatusType s{};
    CHECK(ParseEnum(EnumName(static_cast<StatusType>(i)), s));
    CHECK(EnumIndex(s) == i);
  }
}

TEST_CASE("Stat vocabulary: EquipStats has exactly the 45 web fields") {
  CHECK(kEquipStatCount == 45);
  CHECK(EnumName(Stat::DodgeCounter) == "dodgeCounter");
  CHECK(EnumName(Stat::Int) == "int");
  CHECK(IsEquipStat(Stat::ThornsHeal));
  CHECK_FALSE(IsEquipStat(Stat::AllStats));
  Stat s{};
  CHECK(ParseEnum("elementalDamagePercent", s));
  CHECK(s == Stat::ElementalDamagePercent);
  CHECK(ParseEnum("potionDiscount", s));

  EquipStats eq;
  eq.Add(Stat::Str, 3);
  eq.Add(Stat::AllStats, 99);  // ignored (not an EquipStats key)
  CHECK(eq.Get(Stat::Str) == 3);
  CHECK(eq.Get(Stat::AllStats) == 0);

  StatBag bag;
  bag.Add(Stat::MaxHp, 10);
  bag.Add(Stat::Damage, 2);
  bag.Add(Stat::MaxHp, 5);
  REQUIRE(bag.Size() == 2);
  CHECK(bag.Items()[0].stat == Stat::MaxHp);
  CHECK(bag.Items()[0].value == 15);
  bag.Remove(Stat::MaxHp);
  CHECK(bag.Items()[0].stat == Stat::Damage);
  EquipStats e2;
  bag.AddTo(e2);
  CHECK(e2.Get(Stat::Damage) == 2);

  PrimaryStats ps{12, 8, 10, 5, 5, 5};
  CHECK(ps.Get(PrimaryStat::Int) == 5);
  ps.Ref(PrimaryStat::Vit) += 1;
  CHECK(ps.vit == 11);
}

// ---------------------------------------------------------------------------------------------------------------------
TEST_CASE("Math: JS rounding and conversions") {
  CHECK(JsRound(2.5) == 3.0);
  CHECK(JsRound(-2.5) == -2.0);  // std::round would give -3
  CHECK(JsRound(-0.4) == 0.0);
  CHECK(JsRound(82.5) == 83.0);  // goblin nightmare hp (monsters spec)
  CHECK(JsRound1(0.25) == 0.3);
  CHECK(Clamp(5, 0, 3) == 3);
  CHECK(Clamp(-1.0, 0.0, 3.0) == 0.0);
  CHECK(Lerp(0, 10, 0.25) == 2.5);
  CHECK(DistSq({0, 0}, {3, 4}) == 25.0);
  CHECK(ChebyshevDist({0, 0}, {3, -4}) == 4);
  CHECK(ManhattanDist({0, 0}, {3, -4}) == 7);
  CHECK(ToInt32(4294967295.0) == -1);
  CHECK(ToInt32(2147483648.0) == -2147483647 - 1);
  CHECK(ToUint32(-1.0) == 4294967295u);
  CHECK(ToInt32(12.9) == 12);
  CHECK(Imul(0x7fffffff, 3) == 2147483645);
  CHECK(UShr(0x80000000u, 31) == 1u);
}

TEST_CASE("Math: double -> int32 conversions are total (no UB on NaN / inf / out of range)") {
  CHECK(SaturatingInt32(std::nan("")) == 0);
  CHECK(SaturatingInt32(INFINITY) == 2147483647);
  CHECK(SaturatingInt32(-INFINITY) == -2147483647 - 1);
  CHECK(SaturatingInt32(1e300) == 2147483647);
  CHECK(SaturatingInt32(-1e300) == -2147483647 - 1);
  CHECK(SaturatingInt32(-2147483648.0) == -2147483647 - 1);
  CHECK(SaturatingInt32(2147483646.9) == 2147483646);
  CHECK(SaturatingInt32(-3.7) == -3);
  CHECK(JsRoundInt(2.5) == 3);
  CHECK(JsRoundInt(-2.5) == -2);
  CHECK(JsRoundInt(std::nan("")) == 0);
  CHECK(FloorInt(-0.5) == -1);
  CHECK(CeilInt(-0.5) == 0);
  CHECK(FloorInt(1e20) == 2147483647);
  CHECK(RoundToTile({std::nan(""), -1e40}) == TilePos{0, -2147483647 - 1});
  // FormatFixed: magnitudes past 2^63 after scaling fall back to the shortest round-trip text (was UB).
  CHECK(FormatFixed(1e19, 2) == FormatJsonNumber(1e19));
  CHECK(FormatFixed(-5e20, 0) == FormatJsonNumber(-5e20));
  CHECK(FormatFixed(9e18, 0) == "9000000000000000000");
}

TEST_CASE("Math: JsHypot over the integer grid hashes like V8 (FP tripwire: fast-math / FMA drift)") {
  const std::string text = test::ReadFile(test::GoldenDir() + "/maps/_vectors.json");
  REQUIRE_FALSE(text.empty());
  JsonValue root;
  REQUIRE(ParseJson(text, root));
  const JsonValue& grid = root.Get("mathHypot").Get("grid");
  REQUIRE(grid.IsObject());
  const int32_t lo = grid.Get("min").AsInt();
  const int32_t hi = grid.Get("max").AsInt();
  uint64_t h = 0xcbf29ce484222325ull;  // FNV-1a 64 over the little-endian IEEE-754 bytes, a outer, b inner
  int64_t count = 0;
  for (int32_t a = lo; a <= hi; ++a) {
    for (int32_t b = lo; b <= hi; ++b) {
      const uint64_t bits = std::bit_cast<uint64_t>(JsHypot(a, b));
      for (int i = 0; i < 8; ++i) {
        h ^= (bits >> (8 * i)) & 0xffu;
        h *= 0x100000001b3ull;
      }
      ++count;
    }
  }
  CHECK(count == grid.Get("count").AsInt64());
  char hex[17];
  for (int i = 0; i < 16; ++i) hex[i] = "0123456789abcdef"[(h >> (60 - 4 * i)) & 0xfu];
  hex[16] = 0;
  CHECK(std::string(hex) == grid.Get("fnv1a64").AsString());
}

TEST_CASE("Math: JsHypot matches the V8 golden vectors") {
  const std::string text = test::ReadFile(test::GoldenDir() + "/maps/_vectors.json");
  REQUIRE_FALSE(text.empty());
  JsonValue root;
  REQUIRE(ParseJson(text, root));
  const JsonValue& rows = root.Get("mathHypot").Get("rows");
  REQUIRE(rows.Size() >= 10);
  for (const JsonValue& r : rows.Items()) {
    CHECK(JsHypot(r.At(0).AsDouble(), r.At(1).AsDouble()) == r.At(2).AsDouble());
  }
  CHECK(JsHypot(0, 0) == 0.0);
}

TEST_CASE("Units: S4 constants and iso metric") {
  CHECK(kTileSizeUU == 100.0);
  CHECK(TileToWorld({1.5, 2}).x == 150.0);
  CHECK(WorldToTile({250, 50}).y == 0.5);
  CHECK(IsoPx(1, 0) == doctest::Approx(35.777087639996634));
  CHECK(IsoPx(1, 1) == 32.0);
  CHECK(IsoPx(1, -1) == 64.0);
  CHECK(TilesToProjectilePx(2) == 72.0);
  CHECK(VfxPxToTiles(45) == 1.0);
  CHECK(HeroTilesPerSecond(120) == doctest::Approx(3.3333333333));
}

// ---------------------------------------------------------------------------------------------------------------------
TEST_CASE("Rng: SplitMix64 and xoshiro128** reference values") {
  SplitMix64 sm(0);
  CHECK(sm.Next() == 0xE220A8397B1DCDAFull);

  Rng r(12345);
  const RngState st = r.GetState();
  CHECK(st.s[0] == 2849051040u);
  CHECK(st.s[1] == 571572824u);
  CHECK(st.s[2] == 4145281261u);
  CHECK(st.s[3] == 879680741u);
  const uint32_t expected[5] = {2314518269u, 2498321016u, 2055377852u, 4042509560u, 1267802836u};
  for (uint32_t e : expected) CHECK(r.NextU32() == e);

  Rng f(12345);
  CHECK(f.Float01() == 0.5388907759017175);
  CHECK(f.Float01() == 0.4785549487264461);
  CHECK(f.Float01() == 0.2951833465497242);
}

TEST_CASE("Rng: helpers, scripting and state") {
  Rng r(7);
  for (int i = 0; i < 10000; ++i) {
    const double v = r.Float01();
    CHECK(v >= 0.0);
    CHECK(v < 1.0);
    const int32_t k = r.RandomInt(-3, 3);
    CHECK(k >= -3);
    CHECK(k <= 3);
  }
  // Distribution sanity for RandomInt(0, 9).
  int counts[10] = {};
  for (int i = 0; i < 100000; ++i) ++counts[r.RandomInt(0, 9)];
  for (int c : counts) CHECK(std::abs(c - 10000) < 600);

  // Scripting forces the next Float01 values (and everything built on it), then resumes the generator.
  Rng a(99);
  Rng b(99);
  a.Script({0.0, 0.99, 0.5});
  CHECK(a.Float01() == 0.0);
  CHECK_FALSE(a.Chance(98.0));  // 0.99*100 >= 98
  CHECK(a.RandomInt(0, 3) == 2);
  CHECK(a.ScriptedRemaining() == 0);
  CHECK(a.Float01() == b.Float01());  // generator untouched by scripted draws

  // Save / restore.
  Rng c(5);
  c.NextU32();
  const RngState saved = c.GetState();
  const uint32_t n1 = c.NextU32();
  c.SetState(saved);
  CHECK(c.NextU32() == n1);

  CHECK(r.Index(0) == 0);
  CHECK(Rng(1).Roll(1.0));
  CHECK_FALSE(Rng(1).Roll(0.0));
}

TEST_CASE("RngSet: named streams are independent and deterministic") {
  RngSet s1, s2;
  s1.SeedAll(42);
  s2.SeedAll(42);
  CHECK(EnumName(RngStream::Loot) == "loot");
  CHECK(kRngStreamCount == 7);
  // Drawing from one stream does not disturb another.
  s1.Get(RngStream::Combat).NextU32();
  s1.Get(RngStream::Combat).NextU32();
  CHECK(s1.Get(RngStream::Loot).NextU32() == s2.Get(RngStream::Loot).NextU32());
  CHECK(s1.Get(RngStream::Combat).GetState() != s1.Get(RngStream::Ai).GetState());
  auto states = s1.GetStates();
  RngSet s3;
  s3.SetStates(states);
  CHECK(s3.Get(RngStream::Events).NextU32() == s1.Get(RngStream::Events).NextU32());
}

// ---------------------------------------------------------------------------------------------------------------------
TEST_CASE("SimClock: fixed 60 Hz steps, exact at every third step") {
  SimClock c;
  CHECK(c.NowMs() == 0.0);
  for (int i = 0; i < 180; ++i) c.AdvanceStep();
  CHECK(c.NowMs() == 3000.0);
  c.AdvanceStep();
  CHECK(c.NowMs() == doctest::Approx(3016.6666667));
  CHECK(kSimStepMs == doctest::Approx(16.6666667));
}

TEST_CASE("SimClock: real time accumulator, step budget and freeze") {
  SimClock c;
  c.AccumulateRealTime(1000.0 / 60.0 * 2.5);
  int steps = 0;
  while (c.ConsumeStep()) ++steps;
  c.EndFrame();
  CHECK(steps == 2);
  CHECK(c.Accumulator() == doctest::Approx(kSimStepMs * 0.5));

  // Spiral-of-death clamp: at most 4 steps per frame, whole leftover steps are dropped.
  SimClock d;
  d.AccumulateRealTime(200.0);  // 12 steps worth
  steps = 0;
  while (d.ConsumeStep()) ++steps;
  d.EndFrame();
  CHECK(steps == 4);
  CHECK(d.Accumulator() < kSimStepMs);

  // A frame longer than 250 ms is clamped.
  SimClock e;
  e.SetMaxStepsPerFrame(100);
  e.AccumulateRealTime(10000.0);
  steps = 0;
  while (e.ConsumeStep()) ++steps;
  CHECK(steps == 15);  // floor(250 / 16.667)

  // Freeze: nothing accumulates, no catch-up after unfreeze.
  SimClock f;
  f.SetFrozen(FreezeReason::Cinematic, true);
  CHECK(f.IsFrozen());
  CHECK(f.IsFrozenBy(FreezeReason::Cinematic));
  f.AccumulateRealTime(100.0);
  CHECK_FALSE(f.ConsumeStep());
  f.EndFrame();
  f.SetFrozen(FreezeReason::Modal, true);
  f.SetFrozen(FreezeReason::Cinematic, false);
  CHECK(f.IsFrozen());
  f.SetFrozen(FreezeReason::Modal, false);
  CHECK_FALSE(f.IsFrozen());
  CHECK(f.Accumulator() == 0.0);
}

TEST_CASE("SimClock: dilation scales accumulated time for its real duration (S6)") {
  SimClock c;
  c.SetMaxStepsPerFrame(1000);
  c.StartDilation(0.4, 200.0);
  CHECK(c.Dilation() == 0.4);
  c.AccumulateRealTime(100.0);  // 40 ms of sim time
  CHECK(c.Accumulator() == doctest::Approx(40.0));
  c.AccumulateRealTime(150.0);  // 100 ms dilated (40) + 50 ms normal
  CHECK(c.Accumulator() == doctest::Approx(130.0));
  CHECK(c.Dilation() == 1.0);
}

// ---------------------------------------------------------------------------------------------------------------------
TEST_CASE("TimerQueue: due order, ties in scheduling order, cancellation") {
  TimerQueue q;
  const TimerId a = q.Schedule(100.0, TimerOwner::Combat, 1, 10);
  const TimerId b = q.Schedule(50.0, TimerOwner::Monsters, 2, 11);
  const TimerId c = q.Schedule(100.0, TimerOwner::Combat, 3, 12);
  const TimerId d = q.Schedule(75.0, TimerOwner::World, 4, 13);
  CHECK(q.Size() == 4);
  CHECK(q.Peek()->id == b);
  Timer t;
  CHECK_FALSE(q.PopDue(49.0, t));
  REQUIRE(q.PopDue(100.0, t));
  CHECK(t.id == b);
  CHECK(q.Cancel(d));
  CHECK_FALSE(q.Cancel(d));
  REQUIRE(q.PopDue(100.0, t));
  CHECK(t.id == a);  // equal due: scheduling order
  REQUIRE(q.PopDue(100.0, t));
  CHECK(t.id == c);
  CHECK(q.Empty());

  q.Schedule(10, TimerOwner::Monsters, 1, 1);
  q.Schedule(20, TimerOwner::Combat, 1, 2);
  q.Schedule(30, TimerOwner::Monsters, 2, 3);
  std::vector<Timer> removed;
  const size_t n = q.CancelIf([](const Timer& x) { return x.owner == TimerOwner::Monsters; }, &removed);
  CHECK(n == 2);
  REQUIRE(removed.size() == 2);
  CHECK(removed[0].entity == 1);
  CHECK(q.Size() == 1);
  CHECK(q.Peek()->entity == 2);
}

// ---------------------------------------------------------------------------------------------------------------------
TEST_CASE("Json: parse keeps document order and types") {
  JsonValue v;
  JsonParseError err;
  REQUIRE(ParseJson(R"({"b": 1, "a": [true, null, "x", 2.5, -3, 1e3], "c": {"z": 0, "y": "\u4e2d"}})", v, &err));
  REQUIRE(v.IsObject());
  REQUIRE(v.Members().size() == 3);
  CHECK(v.Members()[0].key == "b");
  CHECK(v.Members()[1].key == "a");
  CHECK(v.Get("b").IsInteger());
  CHECK(v.Get("b").AsInt() == 1);
  const JsonValue& a = v.Get("a");
  CHECK(a.Size() == 6);
  CHECK(a.At(0).AsBool());
  CHECK(a.At(1).IsNull());
  CHECK(a.At(2).AsString() == "x");
  CHECK(a.At(3).AsDouble() == 2.5);
  CHECK_FALSE(a.At(3).IsInteger());
  CHECK(a.At(4).AsInt64() == -3);
  CHECK(a.At(5).IsInteger());
  CHECK(a.At(5).AsInt() == 1000);
  CHECK(a.At(5).RawNumberText() == "1e3");
  CHECK(a.At(99).IsNull());
  CHECK(v.Get("c").Get("y").AsString() == "\xE4\xB8\xAD");
  CHECK(v.Get("missing").IsNull());
  CHECK(v.Get("b").AsString("dflt") == "dflt");
}

TEST_CASE("Json: errors are reported without exceptions") {
  JsonValue v;
  JsonParseError err;
  CHECK_FALSE(ParseJson("{\"a\": }", v, &err));
  CHECK_FALSE(err.message.empty());
  CHECK_FALSE(ParseJson("[1, 2", v, &err));
  CHECK_FALSE(ParseJson("01", v, &err));
  CHECK_FALSE(ParseJson("1e400", v, &err));
  CHECK_FALSE(ParseJson("\"\xFF\"", v, &err));  // invalid UTF-8
  CHECK_FALSE(ParseJson("", v, &err));
  // Nesting is limited (kMaxJsonDepth): the tree is destroyed / copied / written recursively, so deep input is a parse
  // error, not a stack overflow (a 40 KB file of '[' crashed a 512 KB thread stack before the limit).
  std::string deep(20000, '[');
  deep += std::string(20000, ']');
  CHECK_FALSE(ParseJson(deep, v, &err));
  CHECK(Contains(err.message, "nesting too deep"));
  const auto nested = [](size_t depth) { return std::string(depth, '[') + std::string(depth, ']'); };
  CHECK(ParseJson(nested(100), v, &err));
  CHECK(ParseJson(nested(kMaxJsonDepth), v, &err));
  CHECK_FALSE(ParseJson(nested(kMaxJsonDepth + 1), v, &err));
  CHECK(ParseJson("{\"a\":[{\"b\":[1,{\"c\":{}}]}]}", v, &err));
}

TEST_CASE("JsonWriter validates its own calls (misuse is reported and ignored, never UB)") {
  test::ScopedAssertCounter asserts;  // non-aborting handler, like UE's ensure-style one
  JsonWriter w;
  w.StartObject();
  w.EndArray();   // does not match the open object: ignored
  w.EndObject();  // closes the root
  w.EndObject();  // nothing open: ignored
  w.Key("k");     // no object open: ignored
  w.String("v");  // after the root value: ignored
  CHECK(asserts.Count() == 4);
  CHECK(w.Complete());
  CHECK(w.Take() == "{}");

  JsonWriter open;
  open.StartObject();
  open.Key("a");
  open.Int(1);
  CHECK_FALSE(open.Complete());
  CHECK(open.Take().empty());  // a container is still open
  open.Key("b");
  open.Key("c");   // twice in a row: ignored
  open.Bool(true);
  open.Int(2);     // value without a key: ignored
  open.EndObject();
  CHECK(open.Take() == R"({"a":1,"b":true})");
  CHECK(asserts.Count() == 6);

  JsonWriter dangling;
  dangling.StartObject();
  dangling.Key("x");
  dangling.EndObject();  // a Key without its value: ignored
  CHECK(asserts.Count() == 7);
  dangling.Null();
  dangling.EndObject();
  CHECK(dangling.Take() == R"({"x":null})");
}

TEST_CASE("Json: write and round-trip") {
  JsonValue o = JsonValue::Object();
  o.Set("name", JsonValue::String("q\"uote\n"));
  o.Set("n", JsonValue::Integer(42));
  o.Set("f", JsonValue::Number(0.1));
  o.Set("arr", JsonValue::Array());
  o.FindMutable("arr")->Append(JsonValue::Number(3.0));
  o.FindMutable("arr")->Append(JsonValue::Bool(false));
  o.Set("n", JsonValue::Integer(43));  // replace keeps position
  const std::string compact = WriteJson(o);
  CHECK(compact == R"({"name":"q\"uote\n","n":43,"f":0.1,"arr":[3,false]})");
  JsonValue back;
  REQUIRE(ParseJson(compact, back));
  CHECK(back == o);
  const std::string pretty = WriteJson(o, true);
  JsonValue back2;
  REQUIRE(ParseJson(pretty, back2));
  CHECK(back2 == o);
  CHECK(Contains(pretty, "\n  \"n\": 43"));
  CHECK(o.Remove("f"));
  CHECK_FALSE(o.Has("f"));

  JsonWriter w;
  w.StartObject();
  w.Key("x");
  w.Double(1.5);
  w.Key("y");
  w.Double(2.0);
  w.Key("z");
  w.Double(NAN);
  w.EndObject();
  CHECK(w.Take() == R"({"x":1.5,"y":2,"z":null})");
}

TEST_CASE("Json: in-house number parser is correctly rounded") {
  struct Case {
    const char* text;
    double expected;
  };
  const Case cases[] = {
      {"0", 0.0},
      {"-0", -0.0},
      {"0.1", 0.1},
      {"0.015", 0.015},
      {"1.5", 1.5},
      {"555.5555555555555", 555.5555555555555},
      {"2.449489742783178", 2.449489742783178},
      {"1.0471975511965976", 1.0471975511965976},
      {"9007199254740993", 9007199254740992.0},  // tie -> even
      {"9007199254740995", 9007199254740996.0},  // tie -> even
      {"1e23", 1e23},
      {"8.98846567431158e307", 8.98846567431158e307},
      {"1.7976931348623157e308", 1.7976931348623157e308},
      {"4.9406564584124654e-324", 4.9406564584124654e-324},
      {"2.2250738585072011e-308", 2.2250738585072011e-308},
      {"2.2250738585072014e-308", 2.2250738585072014e-308},
      {"0.30000000000000004", 0.30000000000000004},
      {"123456789012345678901234567890", 1.2345678901234568e29},
      {"1e-400", 0.0},
  };
  for (const Case& c : cases) {
    double d = -1;
    INFO(c.text);
    REQUIRE(ParseJsonNumber(c.text, d));
    CHECK(d == c.expected);
    CHECK(std::signbit(d) == std::signbit(c.expected));
  }
  double d;
  CHECK_FALSE(ParseJsonNumber("1.", d));
  CHECK_FALSE(ParseJsonNumber(".5", d));
  CHECK_FALSE(ParseJsonNumber("+1", d));
  CHECK_FALSE(ParseJsonNumber("1e", d));
  CHECK_FALSE(ParseJsonNumber("00", d));
  CHECK_FALSE(ParseJsonNumber("1e400", d));
  int64_t i = 0;
  CHECK(ParseJsonInteger("-9223372036854775808", i));
  CHECK(i == INT64_MIN);
  CHECK_FALSE(ParseJsonInteger("1.0", i));
}

TEST_CASE("Json: parser agrees with strtod on random decimal strings (reference only in tests)") {
  std::mt19937_64 gen(1234);
  std::uniform_int_distribution<int> lenDist(1, 25);
  std::uniform_int_distribution<int> digitDist(0, 9);
  std::uniform_int_distribution<int> expDist(-330, 310);
  std::uniform_int_distribution<int> dotDist(0, 30);
  int checked = 0;
  for (int n = 0; n < 20000; ++n) {
    std::string s;
    const int len = lenDist(gen);
    s.push_back(static_cast<char>('1' + digitDist(gen) % 9));
    for (int k = 1; k < len; ++k) s.push_back(static_cast<char>('0' + digitDist(gen)));
    const int dot = dotDist(gen);
    if (dot < len) s.insert(static_cast<size_t>(dot) + 1, ".");
    if (s.back() == '.') s.push_back('0');
    s += "e" + std::to_string(expDist(gen));
    const double ref = std::strtod(s.c_str(), nullptr);
    double mine = 0;
    if (std::isinf(ref) || ref > 1.7e308) continue;
    INFO(s);
    REQUIRE(ParseJsonNumber(s, mine));
    CHECK(mine == ref);
    ++checked;
  }
  CHECK(checked > 15000);
}

TEST_CASE("Json: FormatJsonNumber round-trips doubles") {
  CHECK(FormatJsonNumber(3.0) == "3");
  CHECK(FormatJsonNumber(-12.0) == "-12");
  CHECK(FormatJsonNumber(-0.0) == "0");
  CHECK(FormatJsonNumber(0.1) == "0.1");
  CHECK(FormatJsonNumber(1.5) == "1.5");
  CHECK(FormatJsonNumber(INFINITY) == "null");
  std::mt19937_64 gen(77);
  for (int n = 0; n < 20000; ++n) {
    uint64_t bits = gen();
    double v;
    std::memcpy(&v, &bits, sizeof(v));
    if (!std::isfinite(v)) continue;
    const std::string s = FormatJsonNumber(v);
    double back = 0;
    INFO(s);
    REQUIRE(ParseJsonNumber(s, back));
    CHECK(back == v);
  }
}

// ---------------------------------------------------------------------------------------------------------------------
TEST_CASE("I18n: substitution, fallback chain, missing keys") {
  I18n i18n;
  std::string err;
  REQUIRE(i18n.LoadTable(R"({"locale":"en","fallback":[],"strings":{"a":"Hello {name}!","b":"only en","p":"{x} {y} {x}"}})",
                         &err));
  REQUIRE(i18n.LoadTable(R"({"locale":"zh-CN","fallback":["en"],"strings":{"a":"ni hao {name}"}})", &err));
  CHECK(i18n.Current() == LocaleId::ZhCN);
  const I18nArg args[] = {{"name", "$& Bob"}};
  CHECK(i18n.T("a", args) == "ni hao $& Bob");  // literal replacement (Q27)
  CHECK(i18n.T("b") == "only en");             // falls back to en
  CHECK(i18n.T("missing.key") == "missing.key");
  CHECK_FALSE(i18n.Has("missing.key"));
  CHECK(i18n.NameOr("missing.key", "fallback") == "fallback");
  i18n.SetLocale(LocaleId::En);
  CHECK(i18n.T("a", args) == "Hello $& Bob!");
  const I18nArg xy[] = {{"x", "1"}};
  CHECK(i18n.T("p", xy) == "1 {y} 1");  // unknown placeholders stay
  i18n.SetLocale(LocaleId::ZhTW);       // not loaded -> ignored
  CHECK(i18n.Current() == LocaleId::En);
  CHECK(I18n::Substitute("{a}{b}{", std::vector<I18nArg>{{"a", "A"}, {"b", "B"}}) == "AB{");
  CHECK_FALSE(i18n.LoadTable("{\"locale\":\"xx\",\"strings\":{}}", &err));
  CHECK(MakeLoc("k", {{"n", "1"}}).args.size() == 1);
}

TEST_CASE("I18n: loads the exported tables (zh-CN, en, zh-TW)") {
  I18n i18n;
  std::string err;
  for (const char* f : {"i18n_zh-CN.json", "i18n_en.json", "i18n_zh-TW.json"}) {
    const std::string bytes = test::ReadFile(test::DataDir() + "/" + f);
    REQUIRE_FALSE(bytes.empty());
    INFO(f);
    REQUIRE(i18n.LoadTable(bytes, &err));
  }
  CHECK(i18n.KeyCount(LocaleId::En) > 2000);
  CHECK(i18n.KeyCount(LocaleId::ZhCN) > 2000);
  i18n.SetLocale(LocaleId::En);
  CHECK(i18n.T("boot.title") == "Abyssfire");
  CHECK(i18n.T("test.enOnly") != "test.enOnly");
  i18n.SetLocale(LocaleId::ZhTW);
  CHECK(i18n.T("test.enOnly") == i18n.T("test.enOnly"));  // resolves through zh-CN -> en
  CHECK(i18n.Has("test.enOnly"));
  i18n.SetLocale(LocaleId::ZhCN);
  const I18nArg args[] = {{"count", "3"}, {"max", "100"}};
  const std::string inv = i18n.T("ui.inventory.title", args);
  CHECK(Contains(inv, "3/100"));
}

TEST_CASE("I18n: number formatting") {
  CHECK(FormatI18nNumber(3) == "3");
  CHECK(FormatI18nNumber(2.5) == "2.5");
  CHECK(FormatFixed(1.25, 1) == "1.3");
  CHECK(FormatFixed(5.5, 0) == "6");
  CHECK(FormatFixed(0.04, 1) == "0.0");
  CHECK(FormatFixed(-1.25, 1) == "-1.3");
  CHECK(FormatFixed(6.0, 1) == "6.0");
}

// ---------------------------------------------------------------------------------------------------------------------
TEST_CASE("StrUtil") {
  CHECK(StrCat("a", 1, "-", int64_t{-2}, std::string("x")) == "a1--2x");
  // Every integer type resolves (Apple: int64_t = long long, size_t = unsigned long; a fixed int64/uint64 overload set
  // is ambiguous there - and for long long on Linux, so this line guards the Apple build in CI).
  CHECK(StrCat(1L, 2LL, 3UL, 4ULL, size_t{5}, int8_t{6}, uint16_t{7}, ptrdiff_t{8}) == "12345678");
  CHECK(StrCat(int64_t{-9223372036854775807 - 1}, " ", uint64_t{18446744073709551615ull}) ==
        "-9223372036854775808 18446744073709551615");
  CHECK(StrCat(short{-3}, static_cast<unsigned char>(200), 2.5) == "-32002.5");
  CHECK(StrCat(1.5) == "1.5");
  CHECK(StartsWith("mat_slime", "mat_"));
  CHECK(EndsWith("file.json", ".json"));
  auto parts = Split("a,b,,c", ',');
  REQUIRE(parts.size() == 4);
  CHECK(parts[2].empty());
  CHECK(ToHex(255) == "ff");
  int64_t v = 0;
  CHECK(ParseInt("-42", v));
  CHECK(v == -42);
  CHECK_FALSE(ParseInt("4x", v));
}

TEST_CASE("Log sink and assert handler are installable") {
  struct Sink {
    int count = 0;
    std::string last;
  } sink;
  SetLogSink(
      [](LogLevel, std::string_view msg, void* user) {
        auto* s = static_cast<Sink*>(user);
        ++s->count;
        s->last.assign(msg);
      },
      &sink);
  SetMinLogLevel(LogLevel::Info);
  Log(LogLevel::Debug, "dropped");
  LogWarning("kept");
  CHECK(sink.count == 1);
  CHECK(sink.last == "kept");
  {
    test::ScopedAssertCounter counter;
    ABYSS_ASSERT(1 + 1 == 3, "math is broken");
    CHECK(counter.Count() == 1);
    CHECK(Contains(counter.LastMessage(), "math is broken"));
    ABYSS_UNIMPLEMENTED();
    CHECK(Contains(sink.last, "unimplemented"));
  }
  SetLogSink(nullptr, nullptr);
}

TEST_CASE("Scoped log sink / assert handler restore the previous binding (UE PIE / module shutdown)") {
  struct Count {
    int n = 0;
  } outer, inner;
  const auto sink = [](LogLevel, std::string_view, void* user) { ++static_cast<Count*>(user)->n; };
  const LogSinkBinding before = SetLogSink(sink, &outer);
  CHECK(before.sink == nullptr);
  {
    ScopedLogSink scoped(sink, &inner);
    CHECK(CurrentLogSink().user == &inner);
    LogWarning("to inner");
  }
  CHECK(CurrentLogSink().user == &outer);  // restored
  LogWarning("to outer");
  CHECK(inner.n == 1);
  CHECK(outer.n == 1);
  const LogSinkBinding mine = SetLogSink(nullptr, nullptr);
  CHECK(mine.user == &outer);

  int handled = 0;
  const auto handler = [](const char*, int, const char*, const char*, void* user) { ++*static_cast<int*>(user); };
  {
    test::ScopedAssertCounter counter;  // itself scoped: restores whatever was installed before it
    {
      ScopedAssertHandler scoped(handler, &handled);
      ABYSS_ASSERT(false, "to the scoped handler");
    }
    ABYSS_ASSERT(false, "to the counter again");
    CHECK(counter.Count() == 1);
  }
  CHECK(handled == 1);
  CHECK(CurrentAssertHandler().handler == nullptr);  // the default (abort) handler again
}

}  // TEST_SUITE
