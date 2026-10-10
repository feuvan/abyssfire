// HLSL subset emulation for unit-testing the material Custom-node code (abyss_content/hlsl.py) without a shader
// compiler: every node body is compiled as a C++ function with these types and intrinsics, which catches undeclared
// names, wrong argument counts, type mismatches (no implicit float4 -> float3 truncation, no scalar swizzles) and lets
// the tests evaluate the maths. Only what the generated code uses is provided, on purpose: anything else must fail.
#pragma once

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wnested-anon-types"
#endif

typedef unsigned int uint;

struct float2
{
	union { struct { float x, y; }; struct { float r, g; }; };
	float2() = default;
	constexpr float2(float a, float b) : x(a), y(b) {}
};

struct float3
{
	union
	{
		struct { float x, y, z; };
		struct { float r, g, b; };
		struct { float2 xy; float z_; };
	};
	float3() = default;
	constexpr float3(float a, float b, float c) : x(a), y(b), z(c) {}
};

struct float4
{
	union
	{
		struct { float x, y, z, w; };
		struct { float r, g, b, a; };
		struct { float3 xyz; float w_; };
		struct { float3 rgb; float a_; };
		struct { float2 xy; float2 zw; };
	};
	float4() = default;
	constexpr float4(float a0, float a1, float a2, float a3) : x(a0), y(a1), z(a2), w(a3) {}
};

struct int3
{
	int x, y, z;
	int3(int a, int b, int c) : x(a), y(b), z(c) {}
};

// ---- arithmetic (component-wise; scalar promotion both ways) ----
#define AF_V2(OP)                                                                                                          \
	inline float2 operator OP(float2 a, float2 b) { return float2(a.x OP b.x, a.y OP b.y); }                              \
	inline float2 operator OP(float2 a, float s) { return float2(a.x OP s, a.y OP s); }                                   \
	inline float2 operator OP(float s, float2 a) { return float2(s OP a.x, s OP a.y); }
#define AF_V3(OP)                                                                                                          \
	inline float3 operator OP(float3 a, float3 b) { return float3(a.x OP b.x, a.y OP b.y, a.z OP b.z); }                  \
	inline float3 operator OP(float3 a, float s) { return float3(a.x OP s, a.y OP s, a.z OP s); }                         \
	inline float3 operator OP(float s, float3 a) { return float3(s OP a.x, s OP a.y, s OP a.z); }
#define AF_V4(OP)                                                                                                          \
	inline float4 operator OP(float4 a, float4 b) { return float4(a.x OP b.x, a.y OP b.y, a.z OP b.z, a.w OP b.w); }      \
	inline float4 operator OP(float4 a, float s) { return float4(a.x OP s, a.y OP s, a.z OP s, a.w OP s); }               \
	inline float4 operator OP(float s, float4 a) { return float4(s OP a.x, s OP a.y, s OP a.z, s OP a.w); }
AF_V2(+) AF_V2(-) AF_V2(*) AF_V2(/)
AF_V3(+) AF_V3(-) AF_V3(*) AF_V3(/)
AF_V4(+) AF_V4(-) AF_V4(*) AF_V4(/)
#undef AF_V2
#undef AF_V3
#undef AF_V4
inline float2 operator-(float2 a) { return float2(-a.x, -a.y); }
inline float3 operator-(float3 a) { return float3(-a.x, -a.y, -a.z); }

// ---- intrinsics ----
#define AF_MAP1(NAME, EXPR)                                                                                                \
	inline float NAME(float v) { return EXPR; }                                                                           \
	inline float2 NAME(float2 v) { return float2(NAME(v.x), NAME(v.y)); }                                                 \
	inline float3 NAME(float3 v) { return float3(NAME(v.x), NAME(v.y), NAME(v.z)); }
AF_MAP1(saturate, v < 0.f ? 0.f : (v > 1.f ? 1.f : v))
AF_MAP1(frac, v - std::floor(v))
AF_MAP1(floor, std::floor(v))
AF_MAP1(abs, std::fabs(v))
AF_MAP1(sin, std::sin(v))
AF_MAP1(cos, std::cos(v))
AF_MAP1(sqrt, std::sqrt(v))
AF_MAP1(exp, std::exp(v))
#undef AF_MAP1

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float2 lerp(float2 a, float2 b, float t) { return a + (b - a) * t; }
inline float3 lerp(float3 a, float3 b, float t) { return a + (b - a) * t; }
inline float3 lerp(float3 a, float3 b, float3 t) { return a + (b - a) * t; }
inline float4 lerp(float4 a, float4 b, float t) { return a + (b - a) * t; }

inline float smoothstep(float e0, float e1, float x)
{
	float t = saturate((x - e0) / (e1 - e0));
	return t * t * (3.f - 2.f * t);
}
inline float step(float edge, float x) { return x >= edge ? 1.f : 0.f; }
inline float3 step(float edge, float3 x) { return float3(step(edge, x.x), step(edge, x.y), step(edge, x.z)); }

inline float pow(float a, float b) { return std::pow(a, b); }
inline float3 pow(float3 a, float b) { return float3(std::pow(a.x, b), std::pow(a.y, b), std::pow(a.z, b)); }

inline float min(float a, float b) { return a < b ? a : b; }
inline float max(float a, float b) { return a > b ? a : b; }
inline int min(int a, int b) { return a < b ? a : b; }
inline int max(int a, int b) { return a > b ? a : b; }
inline float3 max(float3 a, float3 b) { return float3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)); }
inline float3 min(float3 a, float3 b) { return float3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)); }
inline float clamp(float v, float lo, float hi) { return min(max(v, lo), hi); }
inline int clamp(int v, int lo, int hi) { return min(max(v, lo), hi); }

// Mixed scalar literals (HLSL literals are float; the generated code passes C++ double literals): exact-match templates
// win over the (float, float) / (int, int) overloads and over <cmath>'s ::pow(double, double).
template <class A, class B>
using AfMixed = std::enable_if_t<std::is_arithmetic_v<A> && std::is_arithmetic_v<B> &&
	!(std::is_integral_v<A> && std::is_integral_v<B>) && !(std::is_same_v<A, float> && std::is_same_v<B, float>), int>;
template <class A, class B, AfMixed<A, B> = 0>
inline float max(A a, B b) { return max(static_cast<float>(a), static_cast<float>(b)); }
template <class A, class B, AfMixed<A, B> = 0>
inline float min(A a, B b) { return min(static_cast<float>(a), static_cast<float>(b)); }
template <class A, class B, AfMixed<A, B> = 0>
inline float pow(A a, B b) { return std::pow(static_cast<float>(a), static_cast<float>(b)); }
template <class A, class B, class C, std::enable_if_t<std::is_arithmetic_v<A> && std::is_arithmetic_v<B> &&
	std::is_arithmetic_v<C> && !(std::is_integral_v<A> && std::is_integral_v<B> && std::is_integral_v<C>) &&
	!(std::is_same_v<A, float> && std::is_same_v<B, float> && std::is_same_v<C, float>), int> = 0>
inline float clamp(A v, B lo, C hi) { return clamp(static_cast<float>(v), static_cast<float>(lo), static_cast<float>(hi)); }

inline float fmod(float a, float b) { return std::fmod(a, b); }
inline float2 fmod(float2 a, float2 b) { return float2(std::fmod(a.x, b.x), std::fmod(a.y, b.y)); }

inline float dot(float2 a, float2 b) { return a.x * b.x + a.y * b.y; }
inline float dot(float3 a, float3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(float2 v) { return std::sqrt(dot(v, v)); }
inline float length(float3 v) { return std::sqrt(dot(v, v)); }
inline float3 normalize(float3 v) { return v / length(v); }

// ---- textures / material parameters ----
struct SamplerState {};
struct Texture2D
{
	int Width = 1, Height = 1;
	std::vector<float4> Texels;
	float4 Load(int3 P) const
	{
		if (P.x < 0 || P.y < 0 || P.x >= Width || P.y >= Height || Texels.empty())
		{
			return float4(0.f, 0.f, 0.f, 0.f);
		}
		return Texels[static_cast<size_t>(P.y * Width + P.x)];
	}
};

struct FMaterialPixelParameters
{
	float4 SvPosition = float4(0.5f, 0.5f, 0.f, 1.f);
};
inline FMaterialPixelParameters Parameters;
