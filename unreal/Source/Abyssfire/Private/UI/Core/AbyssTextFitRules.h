// Text fitting for painted / fixed-width UI text (dark remake 1C U5 text-fit pass; implementation-plan.md 4 U5,
// DECISIONS DR14): Slate wraps STextBlocks itself, but text painted through FAbyssPainter (centre banners) and labels
// in fixed boxes (panel titles, buttons) do not wrap or shrink, and the remake's longer English strings ran past them.
//
// - WrapLines: greedy line breaking of a painted string at a width. Break opportunities: after a space (the spaces stay
//   at the end of the line and are not measured), between two characters when either is CJK (ideographs, kana,
//   CJK / full-width punctuation) except before closing punctuation (full-width comma, full stop, enumeration comma,
//   ! ? : ; ) and closing corner brackets, closing curly quotes, the ellipsis, the middle dot, ASCII ,.;:!?)]}%) or
//   after opening punctuation (full-width (, opening corner brackets, opening curly quotes, ASCII ([{), and a forced
//   break at '\n' (a blank paragraph is a blank line). A run with no opportunity that is wider than the line (a long
//   word) breaks between characters, closing punctuation hanging past the edge.
// - FitFontPx: the largest font size, down to a floor, at which a one-line label measured at `Px` fits.
//
// Std only (no CoreMinimal, no UE macros): the glue passes code units through a callable and measures with
// FAbyssPainter::Measure; tested in PresentationTests/tests/textfit_test.cpp.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace AbyssTextFitRules
{
	/** One wrapped line: the code units [Start, End) of the source, trailing spaces excluded. */
	struct FLineRange
	{
		size_t Start = 0;
		size_t End = 0;
	};

	inline bool IsSpace(uint32_t C)
	{
		return C == 0x20 || C == 0x09 || C == 0x3000;
	}

	/**
	 * CJK ideographs, kana, CJK symbols / punctuation, full-width forms: a line may break on either side. Curly quotes,
	 * the ellipsis and the middle dot are shared with Latin text ("Marshal's" with U+2019), so they only break beside a CJK
	 * character.
	 */
	inline bool IsCjk(uint32_t C)
	{
		return (C >= 0x2E80 && C <= 0x9FFF) || (C >= 0xF900 && C <= 0xFAFF) || (C >= 0xFE30 && C <= 0xFE4F) || (C >= 0xFF00 && C <= 0xFFEF);
	}

	/** Characters that never start a line (closing punctuation). */
	inline bool NoBreakBefore(uint32_t C)
	{
		switch (C)
		{
		case 0xFF0C: case 0x3002: case 0x3001: case 0xFF01: case 0xFF1F: case 0xFF1A: case 0xFF1B: case 0xFF09: case 0x300D:
		case 0x300F: case 0x3011: case 0x300B: case 0x201D: case 0x2019: case 0x2026: case 0x00B7: case 0xFF0E:
		case ',': case '.': case ';': case ':': case '!': case '?': case ')': case ']': case '}': case '%':
			return true;
		default:
			return false;
		}
	}

	/** Characters that never end a line (opening punctuation). */
	inline bool NoBreakAfter(uint32_t C)
	{
		switch (C)
		{
		case 0xFF08: case 0x300C: case 0x300E: case 0x3010: case 0x300A: case 0x201C: case 0x2018:
		case '(': case '[': case '{':
			return true;
		default:
			return false;
		}
	}

	/** Whether a line may break between code units Index - 1 and Index (0 < Index < Count). */
	template <class FCharAt>
	bool CanBreakBefore(size_t Index, FCharAt&& CharAt)
	{
		const uint32_t Prev = static_cast<uint32_t>(CharAt(Index - 1));
		const uint32_t Cur = static_cast<uint32_t>(CharAt(Index));
		if (IsSpace(Cur) || NoBreakBefore(Cur) || NoBreakAfter(Prev))
		{
			return false;  // spaces hang at the end of the line they follow
		}
		if (IsSpace(Prev))
		{
			return true;
		}
		return IsCjk(Prev) || IsCjk(Cur);
	}

	/**
	 * Greedy wrap of Count code units at MaxWidth. CharAt(i) returns code unit i; Measure(Start, End) the width of
	 * [Start, End). Returns no line for an empty text; a line never starts with a space.
	 */
	template <class FCharAt, class FMeasure>
	std::vector<FLineRange> WrapLines(size_t Count, double MaxWidth, FCharAt&& CharAt, FMeasure&& Measure)
	{
		std::vector<FLineRange> Out;
		const auto TrimEnd = [&CharAt](size_t Start, size_t End)
		{
			while (End > Start && IsSpace(static_cast<uint32_t>(CharAt(End - 1))))
			{
				--End;
			}
			return End;
		};
		size_t Start = 0;
		while (Start < Count)
		{
			while (Start < Count && IsSpace(static_cast<uint32_t>(CharAt(Start))))
			{
				++Start;
			}
			if (Start >= Count)
			{
				break;
			}
			// the forced end of this paragraph
			size_t Para = Start;
			while (Para < Count && CharAt(Para) != '\n')
			{
				++Para;
			}
			size_t Fit = Start;   // last break position that fits (Start = none yet)
			size_t Next = Para;   // where the next line starts
			bool bWhole = true;
			for (size_t Pos = Start + 1; Pos <= Para; ++Pos)
			{
				if (Pos < Para && !CanBreakBefore(Pos, CharAt))
				{
					continue;
				}
				const size_t End = TrimEnd(Start, Pos);
				if (End == Start || Measure(Start, End) <= MaxWidth)
				{
					Fit = Pos;
					continue;
				}
				bWhole = false;
				if (Fit > Start)
				{
					Next = Fit;
				}
				else
				{
					// no opportunity fits: break the run between characters (at least one per line); closing punctuation
					// hangs past the edge rather than start the next line
					size_t Cut = Start + 1;
					while (Cut < Pos && Measure(Start, Cut + 1) <= MaxWidth)
					{
						++Cut;
					}
					while (Cut < Pos && NoBreakBefore(static_cast<uint32_t>(CharAt(Cut))))
					{
						++Cut;
					}
					Next = Cut;
				}
				break;
			}
			if (bWhole)
			{
				Next = Para;
			}
			Out.push_back(FLineRange{ Start, TrimEnd(Start, Next) });
			Start = Next;
			if (bWhole && Start < Count && CharAt(Start) == '\n')
			{
				++Start;
			}
		}
		return Out;
	}

	/**
	 * Font size for a one-line label that measures WidthAtPx at Px: Px when it fits MaxWidth, else scaled down to fit
	 * (rounded down to a half pixel), never below MinPx.
	 */
	inline float FitFontPx(double WidthAtPx, float Px, double MaxWidth, float MinPx)
	{
		if (WidthAtPx <= MaxWidth || WidthAtPx <= 0.0)
		{
			return Px;
		}
		const double Scaled = static_cast<double>(Px) * MaxWidth / WidthAtPx;
		const double Half = static_cast<double>(static_cast<int64_t>(Scaled * 2.0)) * 0.5;
		return Half < static_cast<double>(MinPx) ? MinPx : static_cast<float>(Half);
	}
}  // namespace AbyssTextFitRules
