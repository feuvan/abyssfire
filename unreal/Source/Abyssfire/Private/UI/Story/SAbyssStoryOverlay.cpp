#include "UI/Story/SAbyssStoryOverlay.h"

#include "InputCoreTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include <variant>

#include "abyss/data/DataStore.h"
#include "abyss/sim/Commands.h"

#include "Framework/AbyssText.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Core/AbyssUiDraw.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	// StoryScene layout constants (quests-story-ch1.md 8.5; web px in the 1280 x 720 frame).
	constexpr double GAbyssStoryBarPx = 78.0;
	constexpr double GAbyssStorySayW = 900.0;
	constexpr double GAbyssStorySayH = 150.0;
	constexpr int32 GAbyssStoryEmberCount = 56;

	/** Deterministic 0..1 hash (particles and jitter are pure functions of time). */
	float AbyssStory_Hash(int32 Index, int32 Channel)
	{
		uint32 X = static_cast<uint32>(Index) * 747796405u + static_cast<uint32>(Channel) * 2891336453u + 0x6A09E667u;
		X = ((X >> ((X >> 28u) + 4u)) ^ X) * 277803737u;
		X = (X >> 22u) ^ X;
		return static_cast<float>(X & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
	}

	/** 0..1 progress of [Start, Start + Duration] at T. */
	float AbyssStory_Ramp(double T, double Start, double Duration)
	{
		if (Duration <= 0.0)
		{
			return T >= Start ? 1.f : 0.f;
		}
		return static_cast<float>(FMath::Clamp((T - Start) / Duration, 0.0, 1.0));
	}

	/** tweenTo's default ease (Sine.easeInOut). */
	float AbyssStory_Ease(float T)
	{
		return AbyssEase::SineInOut(T);
	}

	/** Phaser yoyo tween with the default linear ease: 1 -> Low -> 1 over 2 x HalfPeriod. Returns the 0..1 triangle. */
	float AbyssStory_Triangle(double Seconds, double HalfPeriod)
	{
		const double Phase = FMath::Fmod(FMath::Max(Seconds, 0.0), HalfPeriod * 2.0) / HalfPeriod;
		return static_cast<float>(Phase < 1.0 ? Phase : 2.0 - Phase);
	}

	FLinearColor AbyssStory_Css(const std::string& Css, const FLinearColor& Fallback)
	{
		const FString Text = AbyssText::ToFString(Css).TrimStartAndEnd().ToLower();
		if (Text.StartsWith(TEXT("#")))
		{
			return FAbyssUiStyle::Hex(Text, Fallback);
		}
		int32 Open = INDEX_NONE;
		int32 Close = INDEX_NONE;
		if (Text.FindChar(TEXT('('), Open) && Text.FindChar(TEXT(')'), Close) && Close > Open)
		{
			TArray<FString> Parts;
			Text.Mid(Open + 1, Close - Open - 1).ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() >= 3)
			{
				const uint32 R = static_cast<uint32>(FMath::Clamp(FCString::Atoi(*Parts[0].TrimStartAndEnd()), 0, 255));
				const uint32 G = static_cast<uint32>(FMath::Clamp(FCString::Atoi(*Parts[1].TrimStartAndEnd()), 0, 255));
				const uint32 B = static_cast<uint32>(FMath::Clamp(FCString::Atoi(*Parts[2].TrimStartAndEnd()), 0, 255));
				const float A = Parts.Num() >= 4 ? FMath::Clamp(FCString::Atof(*Parts[3].TrimStartAndEnd()), 0.f, 1.f) : 1.f;
				return FAbyssUiStyle::Rgb((R << 16) | (G << 8) | B, A);
			}
		}
		return Fallback;
	}

	struct FAbyssStoryMoodLook
	{
		FLinearColor Top;
		FLinearColor Bottom;
		FLinearColor Glow;
		FLinearColor Embers;
	};

	/** MOODS (StoryScene.ts:35-43; story.json moods when present). Index order = abyss::StoryMood. */
	FAbyssStoryMoodLook AbyssStory_MoodLook(const abyss::StoryScript* Script, abyss::StoryMood Mood)
	{
		static const uint32 Tops[7] = { 0x0c0605, 0x15142a, 0x03050c, 0x0e0906, 0x1c1208, 0x07030a, 0x1d2636 };
		static const uint32 Bottoms[7] = { 0x2a0f06, 0x5a3a30, 0x101a33, 0x40200c, 0x5a3c1a, 0x260a2c, 0x8a7550 };
		static const uint32 Glows[7] = { 0xff7828, 0xffbe78, 0x7896ff, 0xff8c28, 0xffc878, 0xc82878, 0xffecb4 };
		static const float GlowAlpha[7] = { 0.35f, 0.35f, 0.22f, 0.4f, 0.3f, 0.32f, 0.45f };
		static const uint32 Embers[7] = { 0xff9a3c, 0xffd08a, 0x9fb8ff, 0xffb040, 0xffd79a, 0xd24cff, 0xfff1c8 };
		const int32 Index = FMath::Clamp(static_cast<int32>(Mood), 0, 6);
		FAbyssStoryMoodLook Look;
		Look.Top = FAbyssUiStyle::Rgb(Tops[Index]);
		Look.Bottom = FAbyssUiStyle::Rgb(Bottoms[Index]);
		Look.Glow = FAbyssUiStyle::Rgb(Glows[Index], GlowAlpha[Index]);
		Look.Embers = FAbyssUiStyle::Rgb(Embers[Index]);
		if (Script != nullptr)
		{
			for (const abyss::StoryMoodColors& Colors : Script->moods)
			{
				if (Colors.mood == Mood)
				{
					Look.Top = AbyssStory_Css(Colors.top, Look.Top);
					Look.Bottom = AbyssStory_Css(Colors.bottom, Look.Bottom);
					Look.Glow = AbyssStory_Css(Colors.glow, Look.Glow);
					if (Colors.embers != 0u)
					{
						Look.Embers = FAbyssUiStyle::Rgb(Colors.embers);
					}
					break;
				}
			}
		}
		return Look;
	}

	/** Web letterSpacing (px) as FSlateFontInfo::LetterSpacing (1/1000 em). */
	FSlateFontInfo AbyssStory_Spaced(FSlateFontInfo Font, float SpacingPx, float Px)
	{
		Font.LetterSpacing = FMath::RoundToInt(SpacingPx / FMath::Max(Px, 1.f) * 1000.f);
		return Font;
	}

	/** The blinking "more" triangle (StoryScene "▼"). */
	void AbyssStory_PaintMore(FAbyssPainter& P, const FVector2D& Center, float Half, const FLinearColor& Color)
	{
		TArray<FVector2D> Points;
		Points.Add(Center + FVector2D(-Half, -Half * 0.6));
		Points.Add(Center + FVector2D(Half, -Half * 0.6));
		Points.Add(Center + FVector2D(0.0, Half * 0.7));
		P.ConvexPolygon(Points, Color);
	}

	void AbyssStory_Ellipse(FAbyssPainter& P, const FVector2D& Center, const FVector2D& Radii, const FLinearColor& Color)
	{
		P.RadialGradient(Center, FVector2D::ZeroVector, Radii, Color, Color, 32);
	}
}

// =====================================================================================================================
// Construction
// =====================================================================================================================

void SAbyssStoryOverlay::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Ctx = InContext;
	BuildFonts();
	SetVisibility(TAttribute<EVisibility>::CreateLambda([this]() { return bPlaying ? EVisibility::Visible : EVisibility::Collapsed; }));
	const TSharedRef<FAbyssUiContext> Context = InContext;

	TSharedRef<SConstraintCanvas> Texts = SNew(SConstraintCanvas).Visibility(EVisibility::HitTestInvisible);
	const auto Place = [&Texts](const FAnchors& Anchors, const FVector2D& Alignment, const TAttribute<FMargin>& Offset, const TSharedRef<SWidget>& Widget)
	{
		Texts->AddSlot()
			.Anchors(Anchors)
			.Alignment(Alignment)
			.AutoSize(true)
			.Offset(Offset)
		[
			Widget
		];
	};
	const FAnchors Center(0.5f, 0.5f);
	const FAnchors Bottom(0.5f, 1.f);
	const FVector2D Mid(0.5, 0.5);

	// sequence slide block (rebuilt per slide) and the credits column (rebuilt per roll)
	Place(Center, Mid, FMargin(0.f), SAssignNew(SlideHost, SBox));
	Place(Bottom, FVector2D(0.5, 0.0), TAttribute<FMargin>::CreateLambda([this]() { return FMargin(0.f, static_cast<float>(40.0 - CreditsScroll), 0.f, 0.f); }),
		SAssignNew(CreditsHost, SBox)
		.Visibility_Lambda([this]() { return View.bCredits ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }));

	// chapter card
	Place(Center, Mid, FMargin(0.f, -92.f, 0.f, 0.f), MakeText([this]() { return Content.ChNum; }, [this]() { return FontChNum; },
		FAbyssUiStyle::Rgb(0xc9a45a), [this]() { return View.ChNum; }));
	Place(Center, Mid, FMargin(0.f, -36.f, 0.f, 0.f), MakeText([this]() { return Content.ChTitle; }, [this]() { return FontChTitle; },
		FAbyssUiStyle::Rgb(0xf6e0a8), [this]() { return View.ChTitle; }, 0.f, true, nullptr, [this]() { return View.ChTitleScale; }));
	Place(Center, Mid, FMargin(0.f, 22.f, 0.f, 0.f), MakeText([this]() { return Content.ChSub; }, [this]() { return FontChSub; },
		FAbyssUiStyle::Rgb(0xe2cfa4), [this]() { return View.ChSub; }));
	Place(Center, FVector2D(0.5, 0.0), FMargin(0.f, 74.f, 0.f, 0.f), MakeText([this]() { return Content.ChBody; }, [this]() { return FontChBody; },
		FAbyssUiStyle::Rgb(0xd8c8a8), [this]() { return View.ChBody; }, 820.f));

	// narrate
	Place(Center, Mid, FMargin(0.f), MakeText([this]() { return Content.Narrate; }, [this]() { return FontNarrate; },
		FAbyssUiStyle::Rgb(0xefe2c4), [this]() { return View.NarrText; }, 860.f));

	// whisper: two chromatic ghosts under the jittering main line
	const auto Jitter = [this](int32 Channel, double Base, double Spread, bool bCentred)
	{
		const int32 Bucket = FMath::FloorToInt((Ctx->Now() - BeatStartTime) * 1000.0 / 60.0);
		const double R = AbyssStory_Hash(Bucket, Channel);
		return bCentred ? (R - 0.5) * Spread : Base + R * Spread;
	};
	Place(Center, Mid, FMargin(0.f), MakeText([this]() { return Content.Whisper; }, [this]() { return FontWhisperGhost; },
		FAbyssUiStyle::Rgb(0xff3a8c), [this]() { return View.WhGhost; }, 820.f, true,
		[Jitter]() { return FVector2D(-Jitter(11, 3.0, 3.0, false), 0.0); }));
	Place(Center, Mid, FMargin(0.f), MakeText([this]() { return Content.Whisper; }, [this]() { return FontWhisperGhost; },
		FAbyssUiStyle::Rgb(0x4a6bff), [this]() { return View.WhGhost; }, 820.f, true,
		[Jitter]() { return FVector2D(Jitter(12, 3.0, 3.0, false), 0.0); }));
	Place(Center, Mid, FMargin(0.f), MakeText([this]() { return Content.Whisper; }, [this]() { return FontWhisper; },
		FAbyssUiStyle::Rgb(0xf0dcff), [this]() { return View.WhMain; }, 820.f, true,
		[Jitter]() { return FVector2D(Jitter(13, 0.0, 2.4, true), Jitter(14, 0.0, 2.4, true)); }));

	// boss / place title card
	Place(Center, Mid, FMargin(0.f, -16.f, 0.f, 0.f), MakeText([this]() { return Content.TitleName; }, [this]() { return FontTitleName; },
		FAbyssUiStyle::Rgb(0xffe2a8), [this]() { return View.TiName; }, 0.f, true, nullptr, [this]() { return View.TiNameScale; }));
	Place(Center, Mid, FMargin(0.f, 40.f, 0.f, 0.f), MakeText([this]() { return Content.TitleSub; }, [this]() { return FontTitleSub; },
		FAbyssUiStyle::Rgb(0xd9b98a), [this]() { return View.TiEpi; }));

	// speaker box texts: name at (bx + 156, by + 20), typewriter body at (bx + 156, by + 54), wrap boxW - 190
	const float SayLeft = static_cast<float>(-GAbyssStorySayW * 0.5 + 156.0);
	const float SayTop = static_cast<float>(-(GAbyssStoryBarPx + GAbyssStorySayH + 10.0));
	Place(Bottom, FVector2D(0.0, 0.0), FMargin(SayLeft, SayTop + 20.f, 0.f, 0.f), MakeText([this]() { return Content.SayName; },
		[this]() { return FontSayName; },
		TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(FAbyssUiStyle::Rgb(Content.bVillain ? 0xd9a6ff : 0xf0cf86)); }),
		[this]() { return View.SayAlpha; }, 0.f, false));
	Place(Bottom, FVector2D(0.0, 0.0), FMargin(SayLeft, SayTop + 54.f, 0.f, 0.f),
		MakeText([this]() { return FText::FromString(Content.SayFull.Left(View.SayChars)); }, [this]() { return FontSayBody; },
			TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(FAbyssUiStyle::Rgb(Content.bVillain ? 0xeadcff : 0xf1e6cf)); }),
			[this]() { return View.SayAlpha; }, static_cast<float>(GAbyssStorySayW - 190.0), false));

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SAbyssCanvas, Context, [this](FAbyssPainter& P, const FVector2D& Size) { PaintBack(P, Size); })
			.Visibility(EVisibility::HitTestInvisible)
		]
		+ SOverlay::Slot()
		[
			Texts
		]
		+ SOverlay::Slot()
		[
			SNew(SAbyssCanvas, Context, [this](FAbyssPainter& P, const FVector2D& Size) { PaintFront(P, Size); })
			.Visibility(EVisibility::HitTestInvisible)
		]
		// skip button (top right, thumb-sized on touch); not on chapter cards
		+ SOverlay::Slot()
		[
			SNew(SSafeZone)
			.IsTitleSafe(false)
			.Visibility(EVisibility::SelfHitTestInvisible)
			[
				SNew(SBox)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Top)
				.Padding(FMargin(0.f, 22.f, 22.f, 0.f))
				.Visibility(EVisibility::SelfHitTestInvisible)
				[
					SNew(SAbyssHitArea, Context)
					.ClickSound(false)
					.Visibility_Lambda([this]() { return View.bSkip ? EVisibility::Visible : EVisibility::Collapsed; })
					.OnPressed_Lambda([this]() { HandleSkip(); })
					[
						SNew(SBorder)
						.BorderImage(Context->Style().Solid(FLinearColor(0.f, 0.f, 0.f, 0.533f)))
						.Padding_Lambda([this]() { return Ctx->IsTouch() ? FMargin(28.f, 26.f) : FMargin(10.f, 5.f); })
						[
							SNew(STextBlock)
							.Text_Lambda([this]()
							{
								return Ctx->IsTouch() ? Ctx->LocOr("story.ui.skipTouch", TEXT("Skip \x25B8\x25B8"))
									: Ctx->LocOr("story.ui.skip", TEXT("Esc Skip"));
							})
							.Font_Lambda([this]() { return Ctx->Style().Body(Ctx->IsTouch() ? 28.f : 14.f); })
							.ColorAndOpacity(FSlateColor(FAbyssUiStyle::Rgb(0xb8a888)))
						]
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SAbyssStoryOverlay::MakeText(TFunction<FText()> GetText, TFunction<FSlateFontInfo()> GetFont,
	const TAttribute<FSlateColor>& Color, TFunction<float()> GetAlpha, float WrapAt, bool bCentered, TFunction<FVector2D()> GetTranslate,
	TFunction<float()> GetScale)
{
	return SNew(SBorder)
		.BorderImage(Ctx->Style().None())
		.Padding(FMargin(0.f))
		.Visibility_Lambda([GetAlpha]() { return GetAlpha() > 0.002f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		.ColorAndOpacity_Lambda([GetAlpha]() { return FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(GetAlpha(), 0.f, 1.f)); })
		.RenderTransformPivot(FVector2D(0.5, 0.5))
		.RenderTransform_Lambda([GetTranslate, GetScale]() -> TOptional<FSlateRenderTransform>
		{
			const FVector2D Translate = GetTranslate ? GetTranslate() : FVector2D::ZeroVector;
			const float Scale = GetScale ? GetScale() : 1.f;
			if (Translate.IsNearlyZero() && FMath::IsNearlyEqual(Scale, 1.f))
			{
				return TOptional<FSlateRenderTransform>();
			}
			return TOptional<FSlateRenderTransform>(FSlateRenderTransform(Scale, FVector2f(Translate)));
		})
		[
			SNew(STextBlock)
			.Text_Lambda(GetText)
			.Font_Lambda(GetFont)
			.ColorAndOpacity(Color)
			.WrapTextAt(WrapAt)
			.Justification(bCentered ? ETextJustify::Center : ETextJustify::Left)
			.LineHeightPercentage(1.12f)
		];
}

void SAbyssStoryOverlay::BuildFonts()
{
	const FAbyssUiStyle& S = Ctx->Style();
	const FLinearColor Black(0.f, 0.f, 0.f, 1.f);
	FontNarrate = S.Font(EAbyssFontFace::Serif, 24.f, false, 4, Black);
	FontSayName = S.Font(EAbyssFontFace::Serif, 20.f, true, 3, Black);
	FontSayBody = S.Font(EAbyssFontFace::Serif, 20.f, false);
	FontWhisper = S.Font(EAbyssFontFace::Serif, 28.f, false, 5, FAbyssUiStyle::Rgb(0x1a0026));
	FontWhisperGhost = S.Font(EAbyssFontFace::Serif, 28.f, false);
	FontTitleName = AbyssStory_Spaced(S.Font(EAbyssFontFace::Serif, 54.f, true, 8, FAbyssUiStyle::Rgb(0x2a0a02)), 6.f, 54.f);
	FontTitleSub = AbyssStory_Spaced(S.Font(EAbyssFontFace::Serif, 22.f, false, 4, Black), 4.f, 22.f);
	FontChNum = AbyssStory_Spaced(S.Font(EAbyssFontFace::Serif, 20.f, false), 10.f, 20.f);
	FontChTitle = AbyssStory_Spaced(S.Font(EAbyssFontFace::Serif, 56.f, true, 7, FAbyssUiStyle::Rgb(0x140a03)), 8.f, 56.f);
	FontChSub = AbyssStory_Spaced(S.Font(EAbyssFontFace::Serif, 22.f, false), 3.f, 22.f);
	FontChBody = S.Font(EAbyssFontFace::Serif, 19.f, false, 3, Black);
	FontSlideHeading = AbyssStory_Spaced(S.Font(EAbyssFontFace::Serif, 18.f, false), 6.f, 18.f);
	FontSlideTitle = AbyssStory_Spaced(S.Font(EAbyssFontFace::Serif, 44.f, true, 6, FAbyssUiStyle::Rgb(0x1a0c04)), 4.f, 44.f);
	FontSlideText = S.Font(EAbyssFontFace::Serif, 22.f, false, 3, Black);
	FontCreditHeading = AbyssStory_Spaced(S.Font(EAbyssFontFace::Serif, 16.f, false), 4.f, 16.f);
	FontCreditTitle = S.Font(EAbyssFontFace::Serif, 34.f, true, 5, FAbyssUiStyle::Rgb(0x1a0c04));
	FontCreditText = S.Font(EAbyssFontFace::Serif, 20.f, false);
}

// =====================================================================================================================
// Core state
// =====================================================================================================================

void SAbyssStoryOverlay::Reset()
{
	bPlaying = false;
	ClearBeat();
	Speakers.clear();
	SpeakersBeat.clear();
}

void SAbyssStoryOverlay::ClearBeat()
{
	BeatId.clear();
	ContentId.clear();
	Segments.clear();
	Segment = -1;
	ElapsedMs = 0.0;
	bSkipping = false;
	RevealedStep = -1;
	RevealedSlideSegment = -1;
	AdvanceSentSegment = -1;
	ContentSegment = -2;
	SlideBuiltKey = -1;
	SlideParts = 0;
	CreditsSegment = -1;
	CreditsScroll = 0.0;
	bCreditsFast = false;
	bCreditsDone = false;
	CreditsColumn.Reset();
	if (SlideHost.IsValid())
	{
		SlideHost->SetContent(SNullWidget::NullWidget);
	}
	if (CreditsHost.IsValid())
	{
		CreditsHost->SetContent(SNullWidget::NullWidget);
	}
	View = FView();
	Content = FContent();
}

void SAbyssStoryOverlay::StartBeatLocal(const abyss::StoryPlayback& Play)
{
	ClearBeat();
	BeatId = Play.beat.id;
	BeatKind = Play.beat.kind;
	ContentId = Play.beat.contentId;
	Segments = Play.segments;
	BeatStartTime = Ctx->Now();
}

void SAbyssStoryOverlay::Sync(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame)
{
	HeroClass = Snap.hero.cls;
	const abyss::StoryPlayback* Play = Snap.story;
	if (Play == nullptr || !Play->playing)
	{
		if (bPlaying)
		{
			bPlaying = false;
			ClearBeat();
		}
		return;
	}
	if (!bPlaying || Play->beat.id != BeatId || Play->segments.size() != Segments.size())
	{
		StartBeatLocal(*Play);
	}
	bPlaying = true;
	double DeltaMs = 0.0;
	if (Play->segment != Segment)
	{
		Segment = Play->segment;
		ElapsedMs = Play->segmentElapsedMs;
		DeltaMs = ElapsedMs;
		OnSegmentEntered();
	}
	else
	{
		DeltaMs = FMath::Max(0.0, Play->segmentElapsedMs - ElapsedMs);
		ElapsedMs = Play->segmentElapsedMs;
	}
	bSkipping = Play->skipping;
	if (ContentSegment != Segment)
	{
		ResolveContent();
	}
	UpdateView(DeltaMs);
}

void SAbyssStoryOverlay::OnSegmentEntered()
{
	AdvanceSentSegment = -1;
	ContentSegment = -2;
}

void SAbyssStoryOverlay::HandleEvent(const abyss::Event& Event)
{
	if (const abyss::EvStoryStep* Step = std::get_if<abyss::EvStoryStep>(&Event))
	{
		if (Step->isSlide)
		{
			return;
		}
		if (Step->beatId != SpeakersBeat)
		{
			Speakers.clear();
			SpeakersBeat = Step->beatId;
		}
		FStepSpeaker& Info = Speakers[Step->index];
		Info.NameKey = Step->speakerNameKey;
		Info.ArtId = Step->speakerArtId;
		ContentSegment = -2;  // re-resolve the speaker on the next sync
		return;
	}
	if (const abyss::EvStoryBeat* Beat = std::get_if<abyss::EvStoryBeat>(&Event))
	{
		if (Beat->phase == abyss::EvStoryBeat::Phase::Began && Beat->beatId != BeatId)
		{
			bPlaying = false;  // the next sync starts the new beat from its playback
			ClearBeat();
		}
	}
}

void SAbyssStoryOverlay::OnLocaleChanged()
{
	BuildFonts();
	SlideBuiltKey = -1;
	ContentSegment = -2;
	if (bPlaying)
	{
		ResolveContent();
		if (CreditsColumn.IsValid())
		{
			RebuildCredits();
		}
	}
}

// =====================================================================================================================
// Script access
// =====================================================================================================================

const abyss::StoryScript* SAbyssStoryOverlay::Script() const
{
	const abyss::DataStore* Data = Ctx->GetData();
	return Data != nullptr ? &Data->Story() : nullptr;
}

const abyss::StorySegment* SAbyssStoryOverlay::CurrentSegment() const
{
	return Segment >= 0 && static_cast<size_t>(Segment) < Segments.size() ? &Segments[static_cast<size_t>(Segment)] : nullptr;
}

const abyss::CutsceneStep* SAbyssStoryOverlay::StepAt(int32 Index) const
{
	const abyss::StoryScript* S = Script();
	const abyss::Cutscene* Scene = S != nullptr ? S->FindCutscene(ContentId) : nullptr;
	if (Scene == nullptr || Index < 0 || static_cast<size_t>(Index) >= Scene->steps.size())
	{
		return nullptr;
	}
	return &Scene->steps[static_cast<size_t>(Index)];
}

const abyss::StorySequence* SAbyssStoryOverlay::SequenceForPart(int32 Part) const
{
	const abyss::StoryScript* S = Script();
	if (S == nullptr || BeatKind != abyss::StoryBeatKind::Sequence)
	{
		return nullptr;
	}
	if (ContentId == "epilogue")
	{
		return Part == 0 ? &S->epilogue : &S->credits;
	}
	return &S->prologue;
}

abyss::StoryMood SAbyssStoryOverlay::MoodBefore(int32 SegmentIndex) const
{
	if (SegmentIndex < 0 || static_cast<size_t>(SegmentIndex) >= Segments.size())
	{
		return abyss::StoryMood::Embers;
	}
	const int32 Part = Segments[static_cast<size_t>(SegmentIndex)].part;
	const abyss::StorySequence* Seq = SequenceForPart(Part);
	if (Seq == nullptr)
	{
		return abyss::StoryMood::Embers;
	}
	abyss::StoryMood Mood = !Seq->slides.empty() && Seq->slides[0].hasMood ? Seq->slides[0].mood : abyss::StoryMood::Embers;
	for (int32 Index = 0; Index < SegmentIndex; ++Index)
	{
		const abyss::StorySegment& Prev = Segments[static_cast<size_t>(Index)];
		if (Prev.part == Part && Prev.kind == abyss::StorySegmentKind::MoodSwap && Prev.index >= 0
			&& static_cast<size_t>(Prev.index) < Seq->slides.size())
		{
			Mood = Seq->slides[static_cast<size_t>(Prev.index)].mood;
		}
	}
	return Mood;
}

bool SAbyssStoryOverlay::IsSayTyping() const
{
	const abyss::StorySegment* Seg = CurrentSegment();
	if (Seg == nullptr || (Seg->kind != abyss::StorySegmentKind::StepIn && Seg->kind != abyss::StorySegmentKind::StepWait))
	{
		return false;
	}
	const abyss::CutsceneStep* Step = StepAt(Seg->index);
	return Step != nullptr && Step->kind == abyss::StoryStepKind::Say && RevealedStep != Seg->index
		&& View.SayChars < Content.SayFull.Len();
}

void SAbyssStoryOverlay::ResolveContent()
{
	ContentSegment = Segment;
	const abyss::StorySegment* Seg = CurrentSegment();
	const abyss::StoryScript* S = Script();
	if (Seg == nullptr || S == nullptr)
	{
		return;
	}
	switch (BeatKind)
	{
	case abyss::StoryBeatKind::Sequence:
	{
		if (Seg->kind == abyss::StorySegmentKind::MoodSwap || Seg->kind == abyss::StorySegmentKind::Slide
			|| Seg->kind == abyss::StorySegmentKind::SlideOut)
		{
			const int32 Key = Seg->part * 1000 + Seg->index;
			if (Key != SlideBuiltKey)
			{
				const abyss::StorySequence* Seq = SequenceForPart(Seg->part);
				const abyss::StorySlide* Slide = Seq != nullptr && Seg->index >= 0 && static_cast<size_t>(Seg->index) < Seq->slides.size()
					? &Seq->slides[static_cast<size_t>(Seg->index)]
					: nullptr;
				RebuildSlide(Slide);
				SlideBuiltKey = Key;
			}
		}
		else if (Seg->kind != abyss::StorySegmentKind::Credits && SlideBuiltKey != -1)
		{
			RebuildSlide(nullptr);
			SlideBuiltKey = -1;
		}
		break;
	}
	case abyss::StoryBeatKind::Chapter:
	{
		if (const abyss::ChapterCard* Card = S->ChapterFor(ContentId))
		{
			Content.ChNum = Ctx->Loc(Card->number);
			Content.ChTitle = Ctx->Loc(Card->title);
			Content.ChSub = Ctx->Loc(Card->subtitle);
			Content.ChBody = Ctx->Loc(Card->text);
			Content.ChTitleWidth = static_cast<float>(FAbyssPainter::Measure(Content.ChTitle.ToString(), FontChTitle).X);
		}
		break;
	}
	case abyss::StoryBeatKind::Cutscene:
	case abyss::StoryBeatKind::BossIntro:
	{
		const abyss::CutsceneStep* Step = StepAt(Seg->index);
		if (Step == nullptr)
		{
			break;
		}
		switch (Step->kind)
		{
		case abyss::StoryStepKind::Narrate:
			Content.Narrate = Ctx->Loc(Step->text);
			break;
		case abyss::StoryStepKind::Whisper:
			Content.Whisper = Ctx->Loc(Step->text);
			break;
		case abyss::StoryStepKind::Title:
			Content.TitleName = Ctx->Loc(Step->title);
			Content.TitleSub = Step->subtitle.empty() ? FText::GetEmpty() : Ctx->Loc(Step->subtitle);
			Content.TitleNameWidth = static_cast<float>(FAbyssPainter::Measure(Content.TitleName.ToString(), FontTitleName).X);
			break;
		case abyss::StoryStepKind::Say:
		{
			Content.SayFull = Ctx->LocStr(Step->text);
			const abyss::StoryActor& Speaker = Step->speaker;
			Content.bVillain = Speaker.kind == abyss::StoryActorKind::Villain;
			std::string NameKey;
			std::string Art;
			if (SpeakersBeat == BeatId)
			{
				const auto Found = Speakers.find(Seg->index);
				if (Found != Speakers.end())
				{
					NameKey = Found->second.NameKey;
					Art = Found->second.ArtId;
				}
			}
			// The core resolves both (EvStoryStep); the fallbacks only cover a missing event (StoryDirector.ts:260-282).
			FText Name;
			if (!NameKey.empty() && Ctx->HasKey(NameKey))
			{
				Name = Ctx->Loc(NameKey);
			}
			else
			{
				switch (Speaker.kind)
				{
				case abyss::StoryActorKind::Player:
				case abyss::StoryActorKind::Hero:
					Name = Ctx->LocOr("story.speaker.hero", TEXT("You"));
					break;
				case abyss::StoryActorKind::Villain:
					Name = Ctx->LocOr("story.speaker.villain", TEXT("Ignaroth"));
					break;
				case abyss::StoryActorKind::Npc:
					Name = FText::FromString(Ctx->NpcName(Speaker.id));
					break;
				case abyss::StoryActorKind::Monster:
				{
					const std::string BossKey = "story.boss." + Speaker.id + ".name";
					Name = Ctx->HasKey(BossKey) ? Ctx->Loc(BossKey) : FText::FromString(Ctx->MonsterName(Speaker.id));
					break;
				}
				case abyss::StoryActorKind::None:
				case abyss::StoryActorKind::Tile:
					break;
				}
			}
			if (Art.empty())
			{
				switch (Speaker.kind)
				{
				case abyss::StoryActorKind::Player:
				case abyss::StoryActorKind::Hero:
					Art = std::string(abyss::EnumName(HeroClass));
					break;
				case abyss::StoryActorKind::Villain:
					Art = "emblem_villain";
					break;
				case abyss::StoryActorKind::Npc:
				case abyss::StoryActorKind::Monster:
					Art = Speaker.id;
					break;
				case abyss::StoryActorKind::None:
				case abyss::StoryActorKind::Tile:
					Art = "emblem_generic";
					break;
				}
			}
			Content.SayName = Name;
			Content.SayArt = Art;
			break;
		}
		case abyss::StoryStepKind::Focus:
		case abyss::StoryStepKind::Shake:
		case abyss::StoryStepKind::Flash:
		case abyss::StoryStepKind::Wait:
			break;
		}
		break;
	}
	}
}

void SAbyssStoryOverlay::RebuildSlide(const abyss::StorySlide* Slide)
{
	SlideParts = 0;
	if (Slide == nullptr)
	{
		SlideHost->SetContent(SNullWidget::NullWidget);
		View.PartAlpha.Reset();
		return;
	}
	// StoryScene.showSlide: heading 18 px gold (spacing 6) -> title 44 px bold (+40 centre to centre) -> text lines 22 px
	// wrap 900 (+70 from the title centre, +12 between lines); the block is centred vertically, each part fades on its own.
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	const auto AddPart = [this, &Box](const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color, float Wrap, float BottomPad)
	{
		const int32 Index = SlideParts++;
		Box->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 0.f, 0.f, BottomPad))
		[
			MakeText([Text]() { return Text; }, [Font]() { return Font; }, Color,
				[this, Index]() { return View.PartAlpha.IsValidIndex(Index) ? View.PartAlpha[Index] : 0.f; }, Wrap)
		];
	};
	if (!Slide->heading.empty())
	{
		AddPart(Ctx->Loc(Slide->heading), FontSlideHeading, FAbyssUiStyle::Rgb(0xc9a45a), 0.f, 2.f);
	}
	if (!Slide->title.empty())
	{
		AddPart(Ctx->Loc(Slide->title), FontSlideTitle, FAbyssUiStyle::Rgb(0xf3d9a0), 0.f, 30.f);
	}
	if (!Slide->text.empty())
	{
		TArray<FString> Lines;
		Ctx->LocStr(Slide->text).ParseIntoArray(Lines, TEXT("\n"), false);
		for (const FString& Line : Lines)
		{
			AddPart(FText::FromString(Line), FontSlideText, FAbyssUiStyle::Rgb(0xeadcc0), 900.f, 12.f);
		}
	}
	SlideHost->SetContent(Box);
	View.PartAlpha.SetNumZeroed(SlideParts);
}

void SAbyssStoryOverlay::RebuildCredits()
{
	const abyss::StoryScript* S = Script();
	if (S == nullptr)
	{
		return;
	}
	// rollCredits: heading 16 px (+30), title 34 px bold (+52), text 20 px wrap 900, +90 between slides.
	const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
	const auto AddLine = [&Column](const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color, float Wrap, float MinHeight,
		float BottomPad)
	{
		Column->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 0.f, 0.f, BottomPad))
		[
			SNew(SBox)
			.MinDesiredHeight(MinHeight)
			[
				SNew(STextBlock)
				.Text(Text)
				.Font(Font)
				.ColorAndOpacity(FSlateColor(Color))
				.WrapTextAt(Wrap)
				.Justification(ETextJustify::Center)
				.LineHeightPercentage(1.12f)
			]
		];
	};
	for (const abyss::StorySlide& Slide : S->credits.slides)
	{
		if (!Slide.heading.empty())
		{
			AddLine(Ctx->Loc(Slide.heading), FontCreditHeading, FAbyssUiStyle::Rgb(0xc9a45a), 0.f, 30.f, 0.f);
		}
		if (!Slide.title.empty())
		{
			AddLine(Ctx->Loc(Slide.title), FontCreditTitle, FAbyssUiStyle::Rgb(0xf3d9a0), 0.f, 52.f, 0.f);
		}
		if (!Slide.text.empty())
		{
			AddLine(Ctx->Loc(Slide.text), FontCreditText, FAbyssUiStyle::Rgb(0xeadcc0), 900.f, 0.f, 0.f);
		}
		Column->AddSlot().AutoHeight()
		[
			SNew(SBox).HeightOverride(90.f)
		];
	}
	CreditsColumn = Column;
	CreditsHost->SetContent(Column);
}

void SAbyssStoryOverlay::UpdateView(double DeltaMs)
{
	const int32 Parts = SlideParts;
	View = FView();
	View.PartAlpha.SetNumZeroed(Parts);
	const abyss::StorySegment* Seg = CurrentSegment();
	const abyss::StoryScript* S = Script();
	if (!bPlaying || Seg == nullptr || S == nullptr)
	{
		return;
	}
	const abyss::StoryTiming& Tm = S->timing;
	const double T = ElapsedMs;
	View.bSkip = BeatKind != abyss::StoryBeatKind::Chapter && Seg->skipTo >= 0;

	switch (BeatKind)
	{
	case abyss::StoryBeatKind::Sequence:
	{
		View.bBlack = true;
		View.Mood = MoodBefore(Segment);
		View.BgAlpha = 1.f;
		View.EmberAlpha = 1.f;
		const bool bPartStart = Segment == 0 || Segments[static_cast<size_t>(Segment - 1)].part != Seg->part;
		switch (Seg->kind)
		{
		case abyss::StorySegmentKind::Backdrop:
			if (bPartStart)
			{
				View.BgAlpha = AbyssStory_Ease(AbyssStory_Ramp(T, 0.0, Tm.sequenceBackdropInMs));
			}
			else
			{
				View.BgAlpha = 1.f - AbyssStory_Ease(AbyssStory_Ramp(T, 0.0, Tm.sequenceBackdropOutMs));
				View.EmberAlpha = View.BgAlpha;
			}
			break;
		case abyss::StorySegmentKind::MoodSwap:
		{
			const double OutMs = Tm.sequenceMoodOutMs;
			if (T < OutMs)
			{
				View.BgAlpha = 1.f - AbyssStory_Ease(AbyssStory_Ramp(T, 0.0, OutMs));
			}
			else
			{
				const abyss::StorySequence* Seq = SequenceForPart(Seg->part);
				if (Seq != nullptr && Seg->index >= 0 && static_cast<size_t>(Seg->index) < Seq->slides.size())
				{
					View.Mood = Seq->slides[static_cast<size_t>(Seg->index)].mood;
				}
				View.BgAlpha = AbyssStory_Ease(AbyssStory_Ramp(T, OutMs, Tm.sequenceMoodInMs));
			}
			break;
		}
		case abyss::StorySegmentKind::Slide:
		{
			const double PartMs = Tm.slidePartInMs;
			const bool bRushed = RevealedSlideSegment == Segment || bSkipping;
			for (int32 Index = 0; Index < Parts; ++Index)
			{
				View.PartAlpha[Index] = bRushed ? 1.f : AbyssStory_Ease(AbyssStory_Ramp(T, Index * PartMs, PartMs));
			}
			View.bSlideHint = !bSkipping && (bRushed || T >= Parts * PartMs);
			break;
		}
		case abyss::StorySegmentKind::SlideOut:
		{
			const float Out = 1.f - AbyssStory_Ease(AbyssStory_Ramp(T, 0.0, Tm.slideOutMs));
			for (int32 Index = 0; Index < Parts; ++Index)
			{
				View.PartAlpha[Index] = Out;
			}
			break;
		}
		case abyss::StorySegmentKind::Credits:
		{
			View.bCredits = true;
			if (CreditsSegment != Segment)
			{
				CreditsSegment = Segment;
				CreditsScroll = 0.0;
				bCreditsFast = false;
				bCreditsDone = false;
				RebuildCredits();
			}
			else
			{
				CreditsScroll += DeltaMs / 1000.0 * Tm.creditsPxPerSec * (bCreditsFast ? 4.0 : 1.0);
			}
			// The roll ends when the column has left the top (StoryScene: total = H + 40 + column height); the core waits
			// for this StoryAdvance.
			const double ColumnHeight = CreditsColumn.IsValid() ? CreditsColumn->GetDesiredSize().Y : 0.0;
			if (!bCreditsDone && ColumnHeight > 0.0 && CreditsScroll >= LayerSize.Y + 40.0 + ColumnHeight)
			{
				bCreditsDone = true;
				SendAdvance();
			}
			break;
		}
		default:
			break;
		}
		break;
	}
	case abyss::StoryBeatKind::Chapter:
	{
		// playChapter: shade 0-600, glow 600-1500, number 600-1100, title 1100-2000 (+ halo to 2300), rules 2000-2700,
		// subtitle 2000-2600, body 2600-3500; hold; everything fades 900, then the shade 700.
		View.bChapter = true;
		const double Ti = Seg->kind == abyss::StorySegmentKind::ChapterIntro ? FMath::Min(T, Tm.chapterIntroMs) : Tm.chapterIntroMs;
		const bool bOutro = Seg->kind == abyss::StorySegmentKind::ChapterOutro;
		const float Fade = bOutro ? 1.f - AbyssStory_Ease(AbyssStory_Ramp(T, 0.0, 900.0)) : 1.f;
		const float ShadeOut = bOutro ? 1.f - AbyssStory_Ease(AbyssStory_Ramp(T, 900.0, 700.0)) : 1.f;
		View.ChShade = 0.72f * AbyssStory_Ease(AbyssStory_Ramp(Ti, 0.0, 600.0)) * ShadeOut;
		View.ChGlow = 0.55f * AbyssStory_Ease(AbyssStory_Ramp(Ti, 600.0, 900.0)) * Fade;
		View.ChNum = AbyssStory_Ease(AbyssStory_Ramp(Ti, 600.0, 500.0)) * Fade;
		const float TitleT = AbyssEase::CubicOut(AbyssStory_Ramp(Ti, 1100.0, 900.0));
		View.ChTitle = TitleT * Fade;
		View.ChTitleScale = 1.08f - 0.08f * TitleT;
		View.ChHalo = 0.55f * AbyssStory_Ease(AbyssStory_Ramp(Ti, 1100.0, 1200.0)) * Fade;
		View.ChLines = AbyssEase::CubicOut(AbyssStory_Ramp(Ti, 2000.0, 700.0));
		View.ChLinesAlpha = Fade;
		View.ChSub = AbyssStory_Ease(AbyssStory_Ramp(Ti, 2000.0, 600.0)) * Fade;
		View.ChBody = AbyssStory_Ease(AbyssStory_Ramp(Ti, 2600.0, 900.0)) * Fade;
		View.EmberAlpha = View.ChShade / 0.72f;
		break;
	}
	case abyss::StoryBeatKind::Cutscene:
	case abyss::StoryBeatKind::BossIntro:
	{
		switch (Seg->kind)
		{
		case abyss::StorySegmentKind::Letterbox:
			View.Bars = Segment == 0 ? AbyssEase::CubicOut(AbyssStory_Ramp(T, 0.0, Tm.letterboxMs))
				: 1.f - AbyssEase::CubicOut(AbyssStory_Ramp(T, 0.0, Tm.letterboxMs));
			break;
		case abyss::StorySegmentKind::CameraReturn:
			View.Bars = 0.f;
			break;
		default:
			View.Bars = 1.f;
			break;
		}
		const bool bStepPhase = Seg->kind == abyss::StorySegmentKind::StepIn || Seg->kind == abyss::StorySegmentKind::StepWait
			|| Seg->kind == abyss::StorySegmentKind::StepHold || Seg->kind == abyss::StorySegmentKind::StepOut;
		const abyss::CutsceneStep* Step = bStepPhase ? StepAt(Seg->index) : nullptr;
		if (Step == nullptr)
		{
			break;
		}
		const bool bRevealed = RevealedStep == Seg->index || bSkipping;
		const bool bIn = Seg->kind == abyss::StorySegmentKind::StepIn;
		const auto PhaseIn = [&](double InMs) { return bIn ? (bRevealed ? InMs : FMath::Min(T, InMs)) : InMs; };
		const double TOut = Seg->kind == abyss::StorySegmentKind::StepOut ? T : -1.0;
		switch (Step->kind)
		{
		case abyss::StoryStepKind::Narrate:
		{
			// shade -> 0.55 (400), text (700); out: text 350, shade 300
			const double Ti = PhaseIn(Tm.narrate.inMs);
			View.NarrShade = 0.55f * AbyssStory_Ease(AbyssStory_Ramp(Ti, 0.0, 400.0));
			View.NarrText = AbyssStory_Ease(AbyssStory_Ramp(Ti, 400.0, 700.0));
			if (TOut >= 0.0)
			{
				View.NarrText *= 1.f - AbyssStory_Ease(AbyssStory_Ramp(TOut, 0.0, 350.0));
				View.NarrShade *= 1.f - AbyssStory_Ease(AbyssStory_Ramp(TOut, 350.0, 300.0));
			}
			break;
		}
		case abyss::StoryStepKind::Say:
		{
			// box fades in (say.inMs), then the typewriter (typewriterCps); out say.outMs
			const double InMs = Tm.say.inMs;
			const double Ti = PhaseIn(InMs);
			View.SayAlpha = AbyssStory_Ease(AbyssStory_Ramp(Ti, 0.0, InMs));
			if (TOut >= 0.0)
			{
				View.SayAlpha *= 1.f - AbyssStory_Ease(AbyssStory_Ramp(TOut, 0.0, Tm.say.outMs));
			}
			const int32 Len = Content.SayFull.Len();
			if (bRevealed || TOut >= 0.0)
			{
				View.SayChars = Len;
			}
			else
			{
				const double SinceStart = bIn ? T : InMs + T;
				const double Cps = Tm.typewriterCps > 0.0 ? Tm.typewriterCps : 38.0;
				View.SayChars = FMath::Clamp(FMath::FloorToInt((SinceStart - InMs) * Cps / 1000.0), 0, Len);
			}
			View.bSayHint = Seg->kind == abyss::StorySegmentKind::StepWait && View.SayChars >= Len;
			break;
		}
		case abyss::StoryStepKind::Whisper:
		{
			// veil (600), text (700), ghosts -> 0.45 (400, while waiting); out whisper.outMs
			const double Ti = PhaseIn(Tm.whisper.inMs);
			View.WhVeil = AbyssStory_Ease(AbyssStory_Ramp(Ti, 0.0, 600.0));
			View.WhMain = AbyssStory_Ease(AbyssStory_Ramp(Ti, 600.0, 700.0));
			const double Tg = bRevealed ? 400.0 : (Seg->kind == abyss::StorySegmentKind::StepWait ? T : (TOut >= 0.0 ? 400.0 : 0.0));
			View.WhGhost = 0.45f * AbyssStory_Ease(AbyssStory_Ramp(Tg, 0.0, 400.0));
			if (TOut >= 0.0)
			{
				const float Out = 1.f - AbyssStory_Ease(AbyssStory_Ramp(TOut, 0.0, Tm.whisper.outMs));
				View.WhVeil *= Out;
				View.WhMain *= Out;
				View.WhGhost *= Out;
			}
			break;
		}
		case abyss::StoryStepKind::Title:
		{
			// band -> 0.6 (200); halo (500); name scale 1.25 -> 1 (380, Back.easeOut); slash grows (380) with the epithet
			// (400); hold; out title.outMs
			const double Ti = PhaseIn(Tm.title.inMs);
			View.TiBand = 0.6f * AbyssStory_Ease(AbyssStory_Ramp(Ti, 0.0, 200.0));
			View.TiHalo = 0.6f * AbyssStory_Ease(AbyssStory_Ramp(Ti, 200.0, 500.0));
			const float NameT = AbyssEase::BackOut(AbyssStory_Ramp(Ti, 200.0, 380.0));
			View.TiName = FMath::Clamp(NameT, 0.f, 1.f);
			View.TiNameScale = 1.25f - 0.25f * NameT;
			View.TiSlash = AbyssEase::CubicOut(AbyssStory_Ramp(Ti, 580.0, 380.0));
			View.TiSlashAlpha = 1.f;
			View.TiEpi = AbyssStory_Ease(AbyssStory_Ramp(Ti, 580.0, 400.0));
			if (TOut >= 0.0)
			{
				const float Out = 1.f - AbyssStory_Ease(AbyssStory_Ramp(TOut, 0.0, Tm.title.outMs));
				View.TiBand *= Out;
				View.TiHalo *= Out;
				View.TiName *= Out;
				View.TiSlashAlpha = Out;
				View.TiEpi *= Out;
			}
			break;
		}
		case abyss::StoryStepKind::Focus:
		case abyss::StoryStepKind::Shake:
		case abyss::StoryStepKind::Flash:
		case abyss::StoryStepKind::Wait:
			break;
		}
		break;
	}
	}
}

// =====================================================================================================================
// Input
// =====================================================================================================================

void SAbyssStoryOverlay::SendAdvance()
{
	if (AdvanceSentSegment == Segment)
	{
		return;  // one advance per segment (a double tap must not skip the next step)
	}
	AdvanceSentSegment = Segment;
	Ctx->Submit(abyss::CmdStoryAdvance{});
}

bool SAbyssStoryOverlay::HandleAdvance()
{
	if (!bPlaying)
	{
		return false;  // the input layer falls back to CmdStoryAdvance
	}
	const abyss::StorySegment* Seg = CurrentSegment();
	if (Seg == nullptr)
	{
		return true;
	}
	switch (Seg->kind)
	{
	case abyss::StorySegmentKind::StepIn:
		if (RevealedStep != Seg->index)
		{
			RevealedStep = Seg->index;  // first input: complete the reveal here
		}
		else
		{
			SendAdvance();
		}
		return true;
	case abyss::StorySegmentKind::StepWait:
		if (IsSayTyping())
		{
			RevealedStep = Seg->index;  // complete the line; the next input continues
		}
		else
		{
			SendAdvance();
		}
		return true;
	case abyss::StorySegmentKind::StepHold:
	case abyss::StorySegmentKind::ChapterHold:
		SendAdvance();
		return true;
	case abyss::StorySegmentKind::Slide:
	{
		const abyss::StoryScript* S = Script();
		const double PartMs = S != nullptr ? S->timing.slidePartInMs : 900.0;
		if (RevealedSlideSegment != Segment && ElapsedMs < SlideParts * PartMs)
		{
			RevealedSlideSegment = Segment;  // show every part at once
		}
		else
		{
			SendAdvance();
		}
		return true;
	}
	case abyss::StorySegmentKind::Credits:
		bCreditsFast = true;  // a tap speeds the roll x4; Esc ends it
		return true;
	default:
		// letterbox, timed steps, fades, backdrops, mood swaps, chapter intro / outro, the camera return: input waits
		return true;
	}
}

bool SAbyssStoryOverlay::HandleSkip()
{
	if (!bPlaying)
	{
		return false;  // the input layer falls back to CmdStorySkip
	}
	Ctx->Submit(abyss::CmdStorySkip{});
	return true;
}

FReply SAbyssStoryOverlay::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bPlaying)
	{
		return FReply::Unhandled();
	}
	if (MouseEvent.IsTouchEvent() || MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		HandleAdvance();
	}
	return FReply::Handled();
}

FReply SAbyssStoryOverlay::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return bPlaying ? FReply::Handled() : FReply::Unhandled();
}

FReply SAbyssStoryOverlay::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	// The second press of a quick double click arrives here instead of OnMouseButtonDown.
	return OnMouseButtonDown(InMyGeometry, InMouseEvent);
}

// =====================================================================================================================
// Painting
// =====================================================================================================================

void SAbyssStoryOverlay::PaintMood(FAbyssPainter& P, const FVector2D& Size, abyss::StoryMood InMood, float Alpha) const
{
	if (Alpha <= 0.002f)
	{
		return;
	}
	// moodTexture: vertical gradient, radial glow at (50 %, 83 %), vignette - drawn on a 320 x 180 canvas stretched to
	// the screen, so radii scale per axis.
	const FAbyssStoryMoodLook Look = AbyssStory_MoodLook(Script(), InMood);
	P.VerticalGradient(FVector2D::ZeroVector, Size, FAbyssUiStyle::WithAlpha(Look.Top, Alpha), FAbyssUiStyle::WithAlpha(Look.Bottom, Alpha), 24);
	const double Sx = Size.X / 320.0;
	const double Sy = Size.Y / 180.0;
	const FVector2D GlowCenter(Size.X * 0.5, 150.0 * Sy);
	const FLinearColor Glow = FAbyssUiStyle::WithAlpha(Look.Glow, Look.Glow.A * Alpha);
	P.RadialGradient(GlowCenter, FVector2D::ZeroVector, FVector2D(10.0 * Sx, 10.0 * Sy), Glow, Glow);
	P.RadialGradient(GlowCenter, FVector2D(10.0 * Sx, 10.0 * Sy), FVector2D(190.0 * Sx, 190.0 * Sy), Glow, FAbyssUiStyle::WithAlpha(Glow, 0.f));
	P.RadialGradient(Size * 0.5, FVector2D(60.0 * Sx, 60.0 * Sy), FVector2D(210.0 * Sx, 210.0 * Sy), FLinearColor(0.f, 0.f, 0.f, 0.f),
		FLinearColor(0.f, 0.f, 0.f, 0.75f * Alpha));
}

void SAbyssStoryOverlay::PaintEmbers(FAbyssPainter& P, const FVector2D& Size, const FLinearColor& Tint, float Alpha) const
{
	if (Alpha <= 0.002f)
	{
		return;
	}
	// The ember emitter: one particle every ~140 ms from below the screen, rising 18-60 px/s with a little drift,
	// shrinking and fading over 5-9 s. Emission starts with the beat (particles born before it are not drawn).
	const double T = Ctx->Now() - BeatStartTime;
	for (int32 Index = 0; Index < GAbyssStoryEmberCount; ++Index)
	{
		const double Life = 5.0 + 4.0 * AbyssStory_Hash(Index, 1);
		const double FirstBirth = AbyssStory_Hash(Index, 2) * Life;
		if (T < FirstBirth)
		{
			continue;
		}
		const double SinceFirst = T - FirstBirth;
		const double Age = FMath::Fmod(SinceFirst, Life);
		const int32 Cycle = FMath::FloorToInt(SinceFirst / Life);
		const double X0 = AbyssStory_Hash(Index, 3 + Cycle % 7) * Size.X;
		const double Rise = 18.0 + 42.0 * AbyssStory_Hash(Index, 10 + Cycle % 5);
		const double Drift = (AbyssStory_Hash(Index, 16 + Cycle % 3) - 0.5) * 28.0;
		const float Progress = static_cast<float>(Age / Life);
		const FVector2D Pos(X0 + Drift * Age, Size.Y + 10.0 - Rise * Age);
		const float Radius = FMath::Lerp(3.f, 0.5f, Progress);
		const float A = 0.85f * (1.f - Progress) * Alpha;
		P.RadialGradient(Pos, FVector2D::ZeroVector, FVector2D(Radius * 2.6f, Radius * 2.6f), FAbyssUiStyle::WithAlpha(Tint, 0.3f * A),
			FAbyssUiStyle::WithAlpha(Tint, 0.f), 12);
		P.RadialGradient(Pos, FVector2D::ZeroVector, FVector2D(Radius, Radius), FAbyssUiStyle::WithAlpha(Tint, A), FAbyssUiStyle::WithAlpha(Tint, A), 10);
	}
}

void SAbyssStoryOverlay::PaintSayBox(FAbyssPainter& P, const FVector2D& Size) const
{
	const float A = View.SayAlpha;
	const bool bVillain = Content.bVillain;
	const double Bx = (Size.X - GAbyssStorySayW) * 0.5;
	const double By = Size.Y - GAbyssStoryBarPx - GAbyssStorySayH - 10.0;
	const FLinearColor Accent = FAbyssUiStyle::Rgb(bVillain ? 0x9b4dff : 0xc9a45a);
	P.RoundBox(FVector2D(Bx, By), FVector2D(GAbyssStorySayW, GAbyssStorySayH), FAbyssUiStyle::Rgb(bVillain ? 0x12061a : 0x100b07, 0.93f * A), 10.f,
		FAbyssUiStyle::WithAlpha(Accent, 0.9f * A), 2.f);
	P.RoundBox(FVector2D(Bx + 5.0, By + 5.0), FVector2D(GAbyssStorySayW - 10.0, GAbyssStorySayH - 10.0), FLinearColor::Transparent, 7.f,
		FAbyssUiStyle::Rgb(bVillain ? 0x4a1f66 : 0x5a4420, 0.9f * A), 1.f);

	// portrait medallion r 56 (head-and-shoulders art masked to r 53; the emblem when there is none)
	const FVector2D Medal(Bx + 78.0, By + GAbyssStorySayH * 0.5);
	AbyssStory_Ellipse(P, Medal, FVector2D(56.0, 56.0), FLinearColor(0.f, 0.f, 0.f, 0.85f * A));
	const FSlateBrush* Art = bVillain ? Ctx->TextureBrush(FName(TEXT("T_UI_Emblem_Villain"))) : Ctx->Portrait(Content.SayArt);
	if (Art != nullptr)
	{
		P.TexturedCircle(Art, Medal, 53.f, FVector2D(0.5, 0.5), 0.5f, FLinearColor(1.f, 1.f, 1.f, A));
	}
	else if (bVillain)
	{
		// an eye in violet flame
		for (int32 Ring = 5; Ring >= 1; --Ring)
		{
			AbyssStory_Ellipse(P, Medal, FVector2D(10.0 + Ring * 7.0, 6.0 + Ring * 3.5), FAbyssUiStyle::Rgb(0x6a1fb0, 0.12f * Ring * A));
		}
		AbyssStory_Ellipse(P, Medal, FVector2D(15.0, 6.0), FAbyssUiStyle::Rgb(0xffd0ff, A));
		AbyssStory_Ellipse(P, Medal, FVector2D(4.0, 6.0), FAbyssUiStyle::Rgb(0x2a0036, A));
	}
	else
	{
		// a sigil
		P.CircleOutline(Medal, 26.f, FAbyssUiStyle::Rgb(0xc9a45a, A), 3.f, 40);
		TArray<FVector2D> Flame;
		Flame.Add(Medal + FVector2D(0.0, -18.0));
		Flame.Add(Medal + FVector2D(-12.0, 12.0));
		Flame.Add(Medal + FVector2D(12.0, 12.0));
		P.ConvexPolygon(Flame, FAbyssUiStyle::Rgb(0xff9a3c, 0.9f * A));
	}
	P.CircleOutline(Medal, 56.f, FAbyssUiStyle::WithAlpha(Accent, A), 3.f, 56);

	if (View.bSayHint)
	{
		const float Blink = 1.f - 0.75f * AbyssStory_Triangle(Ctx->Now(), 0.6);
		AbyssStory_PaintMore(P, FVector2D(Bx + GAbyssStorySayW - 26.0, By + GAbyssStorySayH - 22.0), 6.f,
			FAbyssUiStyle::Rgb(bVillain ? 0xb889ff : 0xc9a45a, Blink * A));
	}
}

void SAbyssStoryOverlay::PaintBack(FAbyssPainter& P, const FVector2D& Size)
{
	LayerSize = Size;
	if (!bPlaying)
	{
		return;
	}
	const double W = Size.X;
	const double H = Size.Y;
	const abyss::StoryScript* S = Script();
	switch (BeatKind)
	{
	case abyss::StoryBeatKind::Sequence:
	{
		if (View.bBlack)
		{
			P.Box(FVector2D::ZeroVector, Size, FLinearColor::Black);
		}
		PaintMood(P, Size, View.Mood, View.BgAlpha);
		PaintEmbers(P, Size, AbyssStory_MoodLook(S, View.Mood).Embers, View.EmberAlpha);
		if (View.bSlideHint)
		{
			const float Tri = AbyssStory_Triangle(Ctx->Now(), 0.7);
			AbyssStory_PaintMore(P, FVector2D(W * 0.5, H - 46.0 + 6.0 * Tri), 7.f, FAbyssUiStyle::Rgb(0xc9a45a, 1.f - 0.8f * Tri));
		}
		break;
	}
	case abyss::StoryBeatKind::Chapter:
	{
		const abyss::ChapterCard* Card = S != nullptr ? S->ChapterFor(ContentId) : nullptr;
		const abyss::StoryMood Mood = Card != nullptr ? Card->mood : abyss::StoryMood::Dawn;
		const FAbyssStoryMoodLook Look = AbyssStory_MoodLook(S, Mood);
		P.Box(FVector2D::ZeroVector, Size, FLinearColor(0.f, 0.f, 0.f, View.ChShade));
		PaintMood(P, Size, Mood, View.ChGlow);
		if (View.ChHalo > 0.002f)
		{
			const double HaloW = FMath::Max(420.0, static_cast<double>(Content.ChTitleWidth) * 1.6);
			P.RadialGradient(FVector2D(W * 0.5, H * 0.5 - 36.0), FVector2D::ZeroVector, FVector2D(HaloW * 0.5, HaloW * 0.16),
				FAbyssUiStyle::WithAlpha(Look.Embers, View.ChHalo), FAbyssUiStyle::WithAlpha(Look.Embers, 0.f));
		}
		PaintEmbers(P, Size, Look.Embers, View.EmberAlpha);
		if (View.ChLines > 0.f && View.ChLinesAlpha > 0.002f)
		{
			const double Len = 260.0 * View.ChLines;
			const FLinearColor Gold = FAbyssUiStyle::Rgb(0xc9a45a, View.ChLinesAlpha);
			P.Box(FVector2D(W * 0.5 - 20.0 - Len, H * 0.5 + 51.0), FVector2D(Len, 2.0), Gold);
			P.Box(FVector2D(W * 0.5 + 20.0, H * 0.5 + 51.0), FVector2D(Len, 2.0), Gold);
		}
		break;
	}
	case abyss::StoryBeatKind::Cutscene:
	case abyss::StoryBeatKind::BossIntro:
	{
		if (View.NarrShade > 0.002f)
		{
			P.Box(FVector2D::ZeroVector, Size, FLinearColor(0.f, 0.f, 0.f, View.NarrShade));
		}
		if (View.WhVeil > 0.002f)
		{
			// violet vignette: rgba(20,0,30,.25) at r 40, rgba(60,0,90,.6) at 60 %, rgba(10,0,16,.95) at r 200 (320 x 180)
			const double Sx = W / 320.0;
			const double Sy = H / 180.0;
			const FVector2D Mid(W * 0.5, H * 0.5);
			const FLinearColor C0 = FAbyssUiStyle::Rgb(0x14001e, 0.25f * View.WhVeil);
			const FLinearColor C1 = FAbyssUiStyle::Rgb(0x3c005a, 0.6f * View.WhVeil);
			const FLinearColor C2 = FAbyssUiStyle::Rgb(0x0a0010, 0.95f * View.WhVeil);
			P.RadialGradient(Mid, FVector2D::ZeroVector, FVector2D(40.0 * Sx, 40.0 * Sy), C0, C0);
			P.RadialGradient(Mid, FVector2D(40.0 * Sx, 40.0 * Sy), FVector2D(136.0 * Sx, 136.0 * Sy), C0, C1);
			P.RadialGradient(Mid, FVector2D(136.0 * Sx, 136.0 * Sy), FVector2D(200.0 * Sx, 200.0 * Sy), C1, C2);
		}
		if (View.TiBand > 0.002f)
		{
			P.Box(FVector2D(0.0, H * 0.5 - 75.0), FVector2D(W, 150.0), FLinearColor(0.f, 0.f, 0.f, View.TiBand));
		}
		if (View.TiHalo > 0.002f)
		{
			const double HaloW = FMath::Max(460.0, static_cast<double>(Content.TitleNameWidth) * 1.7);
			const FLinearColor Orange = FAbyssUiStyle::Rgb(0xff5a1a);
			P.RadialGradient(FVector2D(W * 0.5, H * 0.5 - 16.0), FVector2D::ZeroVector, FVector2D(HaloW * 0.5, HaloW * 0.16),
				FAbyssUiStyle::WithAlpha(Orange, View.TiHalo), FAbyssUiStyle::WithAlpha(Orange, 0.f));
		}
		if (View.TiSlash > 0.f && View.TiSlashAlpha > 0.002f)
		{
			const double Len = 520.0 * View.TiSlash;
			P.Box(FVector2D(W * 0.5 - Len * 0.5, H * 0.5 + 12.5), FVector2D(Len, 3.0), FAbyssUiStyle::Rgb(0xff8a3c, View.TiSlashAlpha));
		}
		if (View.SayAlpha > 0.002f)
		{
			PaintSayBox(P, Size);
		}
		break;
	}
	}
}

void SAbyssStoryOverlay::PaintFront(FAbyssPainter& P, const FVector2D& Size) const
{
	if (!bPlaying || View.Bars <= 0.f)
	{
		return;
	}
	// letterbox: two black bars of 78 px slide in / out (450 ms Cubic.easeOut), full bleed
	const double Bar = GAbyssStoryBarPx * View.Bars;
	P.Box(FVector2D::ZeroVector, FVector2D(Size.X, Bar), FLinearColor::Black);
	P.Box(FVector2D(0.0, Size.Y - Bar), FVector2D(Size.X, Bar), FLinearColor::Black);
}
