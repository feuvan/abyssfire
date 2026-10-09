#include "World/AbyssWorldTypes.h"

#include "Misc/Parse.h"

FName AbyssAnchorSocketName(EAbyssAnchor Anchor)
{
	switch (Anchor)
	{
	case EAbyssAnchor::Feet: return FName(TEXT("fx_feet"));
	case EAbyssAnchor::Chest: return FName(TEXT("fx_chest"));
	case EAbyssAnchor::Head: return FName(TEXT("fx_head"));
	case EAbyssAnchor::Overhead: return FName(TEXT("fx_overhead"));
	case EAbyssAnchor::HandR: return FName(TEXT("fx_hand_r"));
	case EAbyssAnchor::HandL: return FName(TEXT("fx_hand_l"));
	}
	return NAME_None;
}

namespace AbyssWorldUtil
{
	FLinearColor ColorFromRgb(uint32 Rgb, float Alpha)
	{
		const FColor Srgb(static_cast<uint8>((Rgb >> 16) & 0xFF), static_cast<uint8>((Rgb >> 8) & 0xFF),
			static_cast<uint8>(Rgb & 0xFF), 255);
		FLinearColor Linear = FLinearColor::FromSRGBColor(Srgb);
		Linear.A = Alpha;
		return Linear;
	}

	bool ParseHexColor(FStringView Text, FLinearColor& OutColor)
	{
		FStringView Digits = Text.TrimStartAndEnd();
		if (Digits.StartsWith(TEXT('#')))
		{
			Digits.RightChopInline(1);
		}
		if (Digits.Len() != 6 && Digits.Len() != 3)
		{
			return false;
		}
		uint32 Value = 0;
		for (const TCHAR Char : Digits)
		{
			if (!FChar::IsHexDigit(Char))
			{
				return false;
			}
			Value = (Value << 4) | static_cast<uint32>(FParse::HexDigit(Char));
		}
		if (Digits.Len() == 3)
		{
			const uint32 R = (Value >> 8) & 0xF;
			const uint32 G = (Value >> 4) & 0xF;
			const uint32 B = Value & 0xF;
			Value = (R * 17u << 16) | (G * 17u << 8) | (B * 17u);
		}
		OutColor = ColorFromRgb(Value);
		return true;
	}

	uint32 TileHash(int32 Col, int32 Row, int32 Salt)
	{
		// Math.imul is a wrapping 32-bit multiply; the JS sum of three int32 products followed by `>>> 0` equals the
		// wrapping uint32 sum.
		uint32 H = static_cast<uint32>(Col) * 374761393u + static_cast<uint32>(Row) * 668265263u
			+ static_cast<uint32>(Salt) * 2246822519u;
		H = (H ^ (H >> 13)) * 1274126177u;
		return H ^ (H >> 16);
	}

	FDecorJitter DecorJitter(uint32 Seed)
	{
		// h = uint32(seed * 2654435761) / 2^32; exact in JS doubles for seeds below ~3.4 million (decor counts are < 5000).
		const uint32 A = static_cast<uint32>(static_cast<uint64>(Seed) * 2654435761ull);
		const uint32 B = static_cast<uint32>(static_cast<uint64>(Seed) * 1597334677ull + 12345ull);
		const double H = static_cast<double>(A) / 4294967296.0;
		const double H2 = static_cast<double>(B) / 4294967296.0;
		const double Jx = (H - 0.5) * 18.0;  // iso screen px
		const double Jy = (H2 - 0.5) * 8.0;
		FDecorJitter Out;
		// isoToCart(jx, jy) = ((jx/32 + jy/16) / 2, (jy/16 - jx/32) / 2)
		Out.OffsetTiles = FVector2D((Jx / 32.0 + Jy / 16.0) * 0.5, (Jy / 16.0 - Jx / 32.0) * 0.5);
		Out.Scale = static_cast<float>(0.9 + FMath::Fmod(H + H2, 1.0) * 0.2);
		Out.YawDegrees = static_cast<float>(H * 360.0);
		Out.Random = static_cast<float>(H2);
		return Out;
	}
}
