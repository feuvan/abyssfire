// Story overlay (quests-story-ch1.md 8.5, StoryScene parity): the full-bleed layer above the HUD and panels that renders
// the beat the core's StoryDirector is playing - prologue / epilogue sequences (mood backdrops, embers, staggered slide
// parts), the credits roll, chapter cards, and cutscenes (letterbox, narration, speaker box with a round portrait and a
// typewriter, villain whispers, boss title cards). Camera work (focus / shake / flash) belongs to the world builder.
//
// Timing model: the core times every segment on real time (StoryPlayback: segments, current segment, elapsed ms) with
// the same StoryTiming numbers the web used; the overlay copies that state every frame and renders each element as a
// pure function of (segment kind, elapsed), so the overlay and the sim unfreeze together. Content comes from the
// script (DataStore::Story) plus EvStoryStep (the core-resolved speaker name / portrait art).
//
// Input (two-tap rule, StoryDirector.h): the first advance during a reveal (fade-ins, the say typewriter, staggered slide
// parts) completes it locally; an advance on a waiting step, a title hold or the chapter hold sends CmdStoryAdvance
// (once per segment); during the credits the first advance speeds the roll x4 and the overlay sends CmdStoryAdvance
// itself when the roll ends. Skip (Esc / Start / Android back / the skip button) sends CmdStorySkip; the chapter card
// has no skip button. While a beat plays the overlay swallows clicks and taps (they advance), so nothing below reacts.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include <map>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/data/StoryData.h"
#include "abyss/sim/Events.h"
#include "abyss/sim/SimTypes.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/story/StoryDirector.h"

#include "Framework/AbyssTypes.h"
#include "UI/Core/AbyssUiContext.h"

class SBox;
class SWidget;
struct FAbyssPainter;

class SAbyssStoryOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssStoryOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	/** Per rendered frame (in session): copies the core playback and recomputes every element. */
	void Sync(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame);
	/** EvStoryBeat / EvStoryStep / EvStoryState. */
	void HandleEvent(const abyss::Event& Event);
	/** Advance input (Space / Enter / pad A / world tap routed by the input layer). True = consumed. */
	bool HandleAdvance();
	/** Skip input (Esc / Start / Android back). True = consumed. */
	bool HandleSkip();
	/** Session start / end: drops the beat state. */
	void Reset();
	void OnLocaleChanged();
	bool IsPlaying() const { return bPlaying; }

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

protected:
	/** Animated every frame while a beat plays. */
	virtual bool ComputeVolatility() const override { return true; }

private:
	/** Everything the painters and the text attributes read, recomputed by Sync. */
	struct FView
	{
		// sequences (prologue / epilogue / credits)
		bool bBlack = false;
		float BgAlpha = 0.f;
		abyss::StoryMood Mood = abyss::StoryMood::Embers;
		float EmberAlpha = 0.f;
		TArray<float> PartAlpha;
		bool bSlideHint = false;
		bool bCredits = false;
		// chapter card
		bool bChapter = false;
		float ChShade = 0.f, ChGlow = 0.f, ChNum = 0.f, ChTitle = 0.f, ChTitleScale = 1.f, ChHalo = 0.f, ChLines = 0.f,
			ChLinesAlpha = 0.f, ChSub = 0.f, ChBody = 0.f;
		// cutscene
		float Bars = 0.f;
		float NarrShade = 0.f, NarrText = 0.f;
		float SayAlpha = 0.f;
		int32 SayChars = 0;
		bool bSayHint = false;
		float WhVeil = 0.f, WhMain = 0.f, WhGhost = 0.f;
		float TiBand = 0.f, TiHalo = 0.f, TiName = 0.f, TiNameScale = 1.f, TiSlash = 0.f, TiSlashAlpha = 0.f, TiEpi = 0.f;
		bool bSkip = false;
	};

	/** Resolved text of the current step / slide / card (re-resolved on step and locale changes). */
	struct FContent
	{
		FText Narrate;
		FText SayName;
		FString SayFull;
		std::string SayArt;
		bool bVillain = false;
		FText Whisper;
		FText TitleName;
		FText TitleSub;
		float TitleNameWidth = 0.f;
		FText ChNum, ChTitle, ChSub, ChBody;
		float ChTitleWidth = 0.f;
	};

	/** Speaker info of one cutscene step (EvStoryStep). */
	struct FStepSpeaker
	{
		std::string NameKey;
		std::string ArtId;
	};

	void BuildFonts();
	void StartBeatLocal(const abyss::StoryPlayback& Play);
	void ClearBeat();
	void OnSegmentEntered();
	void ResolveContent();
	void UpdateView(double DeltaMs);
	void RebuildSlide(const abyss::StorySlide* Slide);
	void RebuildCredits();
	void SendAdvance();

	// ---- script access ----
	const abyss::StoryScript* Script() const;
	const abyss::StorySegment* CurrentSegment() const;
	const abyss::CutsceneStep* StepAt(int32 Index) const;
	const abyss::StorySequence* SequenceForPart(int32 Part) const;
	/** The mood shown at a segment (before a mood swap completes: the previous one). */
	abyss::StoryMood MoodBefore(int32 SegmentIndex) const;
	bool IsSayTyping() const;

	// ---- painting ----
	void PaintBack(FAbyssPainter& P, const FVector2D& Size);
	void PaintFront(FAbyssPainter& P, const FVector2D& Size) const;
	void PaintMood(FAbyssPainter& P, const FVector2D& Size, abyss::StoryMood InMood, float Alpha) const;
	void PaintEmbers(FAbyssPainter& P, const FVector2D& Size, const FLinearColor& Tint, float Alpha) const;
	void PaintSayBox(FAbyssPainter& P, const FVector2D& Size) const;

	/** One faded text element (SBorder tint carries the alpha so the stroke fades too). */
	TSharedRef<SWidget> MakeText(TFunction<FText()> GetText, TFunction<FSlateFontInfo()> GetFont, const TAttribute<FSlateColor>& Color,
		TFunction<float()> GetAlpha, float WrapAt = 0.f, bool bCentered = true, TFunction<FVector2D()> GetTranslate = nullptr,
		TFunction<float()> GetScale = nullptr);

	TSharedPtr<FAbyssUiContext> Ctx;
	TSharedPtr<SBox> SlideHost;
	TSharedPtr<SBox> CreditsHost;
	TSharedPtr<SWidget> CreditsColumn;

	// ---- copied core playback ----
	bool bPlaying = false;
	std::string BeatId;
	abyss::StoryBeatKind BeatKind = abyss::StoryBeatKind::Cutscene;
	std::string ContentId;
	std::vector<abyss::StorySegment> Segments;
	int32 Segment = -1;
	double ElapsedMs = 0.0;
	bool bSkipping = false;
	abyss::ClassId HeroClass = abyss::ClassId::Warrior;

	// ---- local beat state ----
	double BeatStartTime = 0.0;
	int32 RevealedStep = -1;        // cutscene step whose reveal an input completed
	int32 RevealedSlideSegment = -1;  // slide segment whose parts an input showed at once
	int32 AdvanceSentSegment = -1;  // CmdStoryAdvance already sent for this segment
	int32 ContentSegment = -2;      // segment ResolveContent ran for
	std::map<int32, FStepSpeaker> Speakers;  // by step index, for SpeakersBeat (std::string members: std containers)
	std::string SpeakersBeat;
	int32 SlideBuiltKey = -1;           // part * 1000 + slide index of the built slide block
	int32 SlideParts = 0;
	int32 CreditsSegment = -1;
	double CreditsScroll = 0.0;
	bool bCreditsFast = false;
	bool bCreditsDone = false;
	FVector2D LayerSize = FVector2D(1280.0, 720.0);

	FView View;
	FContent Content;

	// ---- fonts (rebuilt on locale change: zh-TW switches the CJK face) ----
	FSlateFontInfo FontNarrate, FontSayName, FontSayBody, FontWhisper, FontWhisperGhost, FontTitleName, FontTitleSub;
	FSlateFontInfo FontChNum, FontChTitle, FontChSub, FontChBody;
	FSlateFontInfo FontSlideHeading, FontSlideTitle, FontSlideText;
	FSlateFontInfo FontCreditHeading, FontCreditTitle, FontCreditText;
	FSlateFontInfo FontSkip;
};
