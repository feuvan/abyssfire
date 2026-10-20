// U5 text fitting (Source/Abyssfire/Private/UI/Core/AbyssTextFitRules.h): the greedy wrap of painted strings (centre
// banners) with Latin word breaks and CJK line-breaking rules, and the one-line font fit (panel titles). Synthetic
// widths: an ASCII unit is 1 wide, anything else 2 (a CJK cell).
#include <string>
#include <vector>

#include "doctest/doctest.h"

#include "UI/Core/AbyssTextFitRules.h"

namespace
{
	using namespace AbyssTextFitRules;

	std::string Utf8(const std::u16string& Text)
	{
		std::string Out;
		for (const char16_t Unit : Text)
		{
			const uint32_t C = Unit;
			if (C < 0x80)
			{
				Out += static_cast<char>(C);
			}
			else if (C < 0x800)
			{
				Out += static_cast<char>(0xC0 | (C >> 6));
				Out += static_cast<char>(0x80 | (C & 0x3F));
			}
			else
			{
				Out += static_cast<char>(0xE0 | (C >> 12));
				Out += static_cast<char>(0x80 | ((C >> 6) & 0x3F));
				Out += static_cast<char>(0x80 | (C & 0x3F));
			}
		}
		return Out;
	}

	double UnitWidth(char16_t C) { return C < 0x80 ? 1.0 : 2.0; }

	std::vector<std::string> Wrap(const std::u16string& Text, double MaxWidth)
	{
		const auto CharAt = [&Text](size_t Index) { return Text[Index]; };
		const auto Measure = [&Text](size_t Start, size_t End)
		{
			double Width = 0.0;
			for (size_t Index = Start; Index < End; ++Index)
			{
				Width += UnitWidth(Text[Index]);
			}
			return Width;
		};
		std::vector<std::string> Out;
		for (const FLineRange& Line : WrapLines(Text.size(), MaxWidth, CharAt, Measure))
		{
			Out.push_back(Utf8(Text.substr(Line.Start, Line.End - Line.Start)));
		}
		return Out;
	}

	using Lines = std::vector<std::string>;
}

TEST_SUITE("textfit")
{
	TEST_CASE("empty and blank texts have no line")
	{
		CHECK(Wrap(u"", 10.0).empty());
		CHECK(Wrap(u"   ", 10.0).empty());
		CHECK(Wrap(u"ab", 10.0) == Lines{ "ab" });
	}

	TEST_CASE("Latin text breaks after spaces; the spaces stay behind and never start a line")
	{
		CHECK(Wrap(u"aa bb cc", 5.0) == Lines{ "aa bb", "cc" });
		CHECK(Wrap(u"aa bb cc", 8.0) == Lines{ "aa bb cc" });
		CHECK(Wrap(u"aa   bb", 3.0) == Lines{ "aa", "bb" });
		// a hyphenated or possessive word is one run
		CHECK(Wrap(u"Lord-Marshal Valdras", 14.0) == Lines{ "Lord-Marshal", "Valdras" });
		// a curly apostrophe does not split its word
		CHECK(Wrap(u"The Marshal’s Last Order", 13.0) == Lines{ "The", Utf8(u"Marshal’s"), "Last Order" });
	}

	TEST_CASE("a word wider than the line breaks between characters")
	{
		CHECK(Wrap(u"abcdefgh", 3.0) == Lines{ "abc", "def", "gh" });
		CHECK(Wrap(u"ab abcdefgh", 4.0) == Lines{ "ab", "abcd", "efgh" });
		// even a line narrower than one character takes one per line
		CHECK(Wrap(u"一二", 1.0) == Lines{ Utf8(u"一"), Utf8(u"二") });
	}

	TEST_CASE("CJK breaks between characters")
	{
		CHECK(Wrap(u"一二三四五", 6.0) == Lines{ Utf8(u"一二三"), Utf8(u"四五") });
		// mixed: a Latin run inside CJK stays whole
		CHECK(Wrap(u"返回NPC交付", 6.0) == Lines{ Utf8(u"返回"), Utf8(u"NPC交"), Utf8(u"付") });
	}

	TEST_CASE("closing punctuation never starts a line, opening punctuation never ends one")
	{
		// 一二。三 at 4: the full stop stays with 二
		CHECK(Wrap(u"一二。三", 4.0) == Lines{ Utf8(u"一"), Utf8(u"二。"), Utf8(u"三") });
		// 一，二 at 2 (narrower than two cells): the comma cannot lead, so it hangs past the edge of the first line
		CHECK(Wrap(u"一，二", 2.0) == Lines{ Utf8(u"一，"), Utf8(u"二") });
		// 一“二三” at 4: the opening quote never ends a line (it goes down with 二), the closing one never starts one
		CHECK(Wrap(u"一“二三”", 4.0) == Lines{ Utf8(u"一"), Utf8(u"“二"), Utf8(u"三”") });
		CHECK(Wrap(u"一“二三”", 6.0) == Lines{ Utf8(u"一“二"), Utf8(u"三”") });
		// ASCII: "(a b)" keeps the parenthesis with its word
		CHECK(Wrap(u"x (ab) y", 5.0) == Lines{ "x", "(ab)", "y" });
	}

	TEST_CASE("a newline forces a break")
	{
		CHECK(Wrap(u"ab\ncd", 10.0) == Lines{ "ab", "cd" });
		CHECK(Wrap(u"ab cd\nef", 3.0) == Lines{ "ab", "cd", "ef" });
		CHECK(Wrap(u"ab\n\ncd", 10.0) == Lines{ "ab", "", "cd" });  // a blank paragraph is a blank line
	}

	TEST_CASE("every line fits unless it is a single character; nothing is lost but spaces")
	{
		const std::u16string Text = u"You cut the pilgrim loose. “They said I walked too slowly to keep up with the Flame.” "
			u"你割断了朝圣者身上的绳子。";
		for (const double Width : { 6.0, 11.0, 23.0, 40.0, 97.0 })
		{
			const auto CharAt = [&Text](size_t Index) { return Text[Index]; };
			size_t Kept = 0;
			size_t Previous = 0;
			for (const FLineRange& Line : WrapLines(Text.size(), Width, CharAt, [&Text](size_t Start, size_t End)
				{
					double W = 0.0;
					for (size_t Index = Start; Index < End; ++Index)
					{
						W += UnitWidth(Text[Index]);
					}
					return W;
				}))
			{
				INFO("width " << Width << " line at " << Line.Start);
				CHECK(Line.Start >= Previous);
				CHECK(Line.End > Line.Start);
				CHECK(!IsSpace(Text[Line.Start]));
				double W = 0.0;
				for (size_t Index = Line.Start; Index < Line.End; ++Index)
				{
					W += UnitWidth(Text[Index]);
					Kept += IsSpace(Text[Index]) ? 0 : 1;
				}
				// a closing mark may hang one cell past the edge (it never starts the next line)
				CHECK((W <= Width + 2.0 || Line.End - Line.Start == 1));
				Previous = Line.End;
			}
			size_t NonSpace = 0;
			for (const char16_t C : Text)
			{
				NonSpace += IsSpace(C) ? 0 : 1;
			}
			CHECK(Kept == NonSpace);
		}
	}

	TEST_CASE("one-line fit: the size when it fits, else scaled down to a half pixel, never below the floor")
	{
		CHECK(FitFontPx(200.0, 18.f, 232.0, 13.f) == doctest::Approx(18.f));
		CHECK(FitFontPx(232.0, 18.f, 232.0, 13.f) == doctest::Approx(18.f));
		CHECK(FitFontPx(249.0, 18.f, 232.0, 13.f) == doctest::Approx(16.5f));  // 16.77 -> 16.5
		CHECK(FitFontPx(400.0, 18.f, 232.0, 13.f) == doctest::Approx(13.f));   // 10.4 -> floor
		CHECK(FitFontPx(0.0, 18.f, 232.0, 13.f) == doctest::Approx(18.f));
		for (const double Width : { 233.0, 260.0, 300.0 })
		{
			const float Px = FitFontPx(Width, 18.f, 232.0, 13.f);
			CHECK(Width * Px / 18.0 <= 232.0 + 1e-9);
		}
	}
}
