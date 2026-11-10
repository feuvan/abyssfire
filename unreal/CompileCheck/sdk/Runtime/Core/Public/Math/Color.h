// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Math/Color.h (from UE 5.x source knowledge).
// FLinearColor (float RGBA, linear) and FColor (8-bit BGRA, sRGB). FColor -> FLinearColor converts implicitly (sRGB
// decode); the reverse needs ToFColor(bSRGB) / ToFColorSRGB() / QuantizeRound().
#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Templates/TypeHash.h"

struct FColor;
enum class EGammaSpace : uint8 { Linear, Pow22, sRGB, Invalid };

struct FLinearColor
{
	float R, G, B, A;
	FLinearColor() = default;
	explicit constexpr FLinearColor(EForceInit) : R(0), G(0), B(0), A(0) {}
	constexpr FLinearColor(float InR, float InG, float InB, float InA = 1.0f) : R(InR), G(InG), B(InB), A(InA) {}
	FLinearColor(const FColor& Color);
	FLinearColor(const FVector3f& Vector);
	explicit FLinearColor(const FVector3d& Vector);
	explicit FLinearColor(const FVector4f& Vector);
	explicit FLinearColor(const FVector4d& Vector);
	static FLinearColor FromSRGBColor(const FColor& Color);
	static FLinearColor FromPow22Color(const FColor& Color);
	FColor ToRGBE() const;
	FLinearColor LinearRGBToHSV() const;
	FLinearColor HSVToLinearRGB() const;
	float& Component(int32 Index);
	const float& Component(int32 Index) const;
	constexpr FLinearColor operator+(const FLinearColor& ColorB) const { return FLinearColor(R + ColorB.R, G + ColorB.G, B + ColorB.B, A + ColorB.A); }
	FLinearColor& operator+=(const FLinearColor& ColorB);
	constexpr FLinearColor operator-(const FLinearColor& ColorB) const { return FLinearColor(R - ColorB.R, G - ColorB.G, B - ColorB.B, A - ColorB.A); }
	FLinearColor& operator-=(const FLinearColor& ColorB);
	constexpr FLinearColor operator*(const FLinearColor& ColorB) const { return FLinearColor(R * ColorB.R, G * ColorB.G, B * ColorB.B, A * ColorB.A); }
	FLinearColor& operator*=(const FLinearColor& ColorB);
	constexpr FLinearColor operator*(float Scalar) const { return FLinearColor(R * Scalar, G * Scalar, B * Scalar, A * Scalar); }
	FLinearColor& operator*=(float Scalar);
	constexpr FLinearColor operator/(const FLinearColor& ColorB) const { return FLinearColor(R / ColorB.R, G / ColorB.G, B / ColorB.B, A / ColorB.A); }
	FLinearColor& operator/=(const FLinearColor& ColorB);
	constexpr FLinearColor operator/(float Scalar) const { return FLinearColor(R / Scalar, G / Scalar, B / Scalar, A / Scalar); }
	FLinearColor& operator/=(float Scalar);
	FLinearColor GetClamped(float InMin = 0.0f, float InMax = 1.0f) const;
	constexpr bool operator==(const FLinearColor& ColorB) const { return R == ColorB.R && G == ColorB.G && B == ColorB.B && A == ColorB.A; }
	constexpr bool operator!=(const FLinearColor& Other) const { return !(*this == Other); }
	bool Equals(const FLinearColor& ColorB, float Tolerance = UE_KINDA_SMALL_NUMBER) const;
	FLinearColor CopyWithNewOpacity(float NewOpacicty) const;
	static FLinearColor MakeFromHSV8(uint8 H, uint8 S, uint8 V);
	static FLinearColor MakeRandomColor();
	static FLinearColor MakeFromColorTemperature(float Temp);
	static FLinearColor MakeRandomSeededColor(int32 Seed);
	static float Dist(const FLinearColor& V1, const FLinearColor& V2);
	static FLinearColor LerpUsingHSV(const FLinearColor& From, const FLinearColor& To, const float Progress);
	FColor QuantizeFloor() const;
	FColor Quantize() const;
	FColor QuantizeRound() const;
	FColor ToFColorSRGB() const;
	FColor ToFColor(const bool bSRGB) const;
	FLinearColor Desaturate(float Desaturation) const;
	float GetMax() const;
	bool IsAlmostBlack() const;
	float GetMin() const;
	float GetLuminance() const;
	FString ToString() const;
	bool InitFromString(const FString& InSourceString);
	static const FLinearColor White;
	static const FLinearColor Gray;
	static const FLinearColor Black;
	static const FLinearColor Transparent;
	static const FLinearColor Red;
	static const FLinearColor Green;
	static const FLinearColor Blue;
	static const FLinearColor Yellow;
};
constexpr FLinearColor operator*(float Scalar, const FLinearColor& Color) { return Color.operator*(Scalar); }
uint32 GetTypeHash(const FLinearColor& LinearColor);

struct FColor
{
	uint8 B, G, R, A;
	uint32& DWColor();
	const uint32& DWColor() const;
	FColor() = default;
	explicit constexpr FColor(EForceInit) : B(0), G(0), R(0), A(0) {}
	constexpr FColor(uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255) : B(InB), G(InG), R(InR), A(InA) {}
	explicit FColor(uint32 InColor);
	bool operator==(const FColor& C) const;
	bool operator!=(const FColor& C) const;
	void operator+=(const FColor& C);
	FLinearColor FromRGBE() const;
	static FColor FromHex(const FString& HexString);
	static FColor MakeRandomColor();
	static FColor MakeRedToGreenColorFromScalar(float Scalar);
	static FColor MakeFromColorTemperature(float Temp);
	static FColor MakeRandomSeededColor(int32 Seed);
	FColor WithAlpha(uint8 Alpha) const;
	FLinearColor ReinterpretAsLinear() const;
	FString ToHex() const;
	FString ToString() const;
	bool InitFromString(const FString& InSourceString);
	uint32 ToPackedARGB() const;
	uint32 ToPackedABGR() const;
	uint32 ToPackedRGBA() const;
	uint32 ToPackedBGRA() const;
	static const FColor White;
	static const FColor Black;
	static const FColor Transparent;
	static const FColor Red;
	static const FColor Green;
	static const FColor Blue;
	static const FColor Yellow;
	static const FColor Cyan;
	static const FColor Magenta;
	static const FColor Orange;
	static const FColor Purple;
	static const FColor Turquoise;
	static const FColor Silver;
	static const FColor Emerald;
};
uint32 GetTypeHash(const FColor& Color);
template <> struct TIsPODType<FColor> { enum { Value = true }; };
template <> struct TIsPODType<FLinearColor> { enum { Value = true }; };
