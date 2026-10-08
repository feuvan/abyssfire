RapidJSON 1.1.0 (headers only), vendored for AbyssCore (DECISIONS U3).

* Source: Debian/Ubuntu package rapidjson-dev 1.1.0+dfsg2 (upstream https://github.com/Tencent/rapidjson).
* Removed (unused, and they pull in iostreams / FILE I/O, which the core forbids): istreamwrapper.h,
  ostreamwrapper.h, filereadstream.h, filewritestream.h, schema.h.
* Never include these headers directly: only Private/base/Json.cpp does, through Private/base/RapidJsonConfig.h, which
  defines RAPIDJSON_NAMESPACE=abyss_rapidjson, RAPIDJSON_HAS_STDSTRING=1, routes RAPIDJSON_ASSERT to ABYSS_ASSERT and
  keeps exceptions off. Public core headers never expose RapidJSON types.
