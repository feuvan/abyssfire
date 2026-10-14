// SAbyssWorldLayer: the world-anchored UI layer (ue58-platform.md 9.6, World/AbyssWorldUi.h contract): nameplates and HP
// bars (monsters-ai 5 / 15), NPC names and quest markers (quests 5.6), pet labels (18.8), escort / defend bars, loot
// labels, lore / prop labels, the floating combat text (combat-feel 12) and the quest progress popups above the hero
// (quests 5.8). One full-viewport widget; every anchor is projected with the local player's view each paint (after the
// camera update of the frame), so labels never lag the camera.
// Ground-item labels show on demand (DECISIONS R17, AbyssGroundLabelLayout.h): hovered, Alt held, the hero within 2
// tiles, or a rare+ drop for 4 s; they stack in up to 3 rows (7 with Alt) and dim under a live float.
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include <string>

#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "UI/Core/AbyssUiContext.h"
#include "UI/World/AbyssGroundLabelLayout.h"
#include "World/AbyssWorldUi.h"

struct FAbyssPainter;

class SAbyssWorldLayer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssWorldLayer) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	// ---- IAbyssWorldUi (forwarded by the subsystem) ----
	void AddWidget(const FAbyssWorldWidgetDesc& Desc);
	void RemoveWidget(abyss::EntityId Id);
	void ClearWidgets();
	void UpdateFrames(TConstArrayView<FAbyssWorldWidgetFrame> Frames);
	void AddFloatingText(const FAbyssFloatingTextRequest& Request);

	/** Copies what the labels show from the snapshot (names, HP, markers, quality) and the hero's overhead point. */
	void Sync(const abyss::Snapshot& Snap);
	/** Quest progress popups above the hero (EvQuestUpdate{Progress}). */
	void HandleQuestUpdate(const abyss::EvQuestUpdate& Update, const abyss::Snapshot& Snap);
	void SetHovered(abyss::EntityId Id) { HoveredId = Id; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	struct FLabel
	{
		FAbyssWorldWidgetDesc Desc;
		FVector Overhead = FVector::ZeroVector;
		FVector Feet = FVector::ZeroVector;
		bool bVisible = false;
		float Opacity = 1.f;
		bool bHasFrame = false;
		// copied from the snapshot (Name only when its signature / the locale changes)
		bool bPresent = false;
		FString Name;
		uint64 NameSignature = 0;
		bool bNameValid = false;
		FLinearColor NameColor = FLinearColor::White;
		bool bAlwaysShowName = true;
		double Hp = 0.0, MaxHp = 0.0;
		bool bHpBar = false;
		bool bSmallBar = false;
		abyss::NpcMarker Marker = abyss::NpcMarker::None;
		int32 NameFontPx = 12;
		bool bTitleFont = false;
		/** Ground items: quality (rare+ drops show 4 s) and when the widget appeared (the drop, Ctx->Now()). */
		abyss::ItemQuality Quality = abyss::ItemQuality::Normal;
		double CreatedAt = 0.0;
	};

	struct FFloat
	{
		abyss::FloatingTextKind Kind = abyss::FloatingTextKind::Custom;
		FVector World = FVector::ZeroVector;
		FString Text;
		FLinearColor Color = FLinearColor::White;
		float FontPx = 20.f;
		int32 Outline = 3;
		bool bCrit = false;
		double Start = 0.0;
		/** Start offset from the anchor in web px (BaseOffset plus the placement lift of PlaceFloatGroup). */
		FVector2D StartOffset = FVector2D::ZeroVector;
		/** Start offset before the placement lift (the web position of the kind). */
		FVector2D BaseOffset = FVector2D::ZeroVector;
		/** Ink box of the text at scale 1 in Slate px (placement checks). */
		FVector2D Box = FVector2D::ZeroVector;
		double Drift = 0.0;
	};

	struct FPopup
	{
		FString Text;
		bool bDone = false;
		double Start = 0.0;
		int32 Stack = 0;
	};

	bool Project(const FVector& World, FVector2D& OutLocal, const FVector2D& LocalSize) const;
	void PaintLabel(FAbyssPainter& P, const FLabel& Label, const FVector2D& LocalSize, double Now) const;
	/** Ground-item labels on demand (DECISIONS R17): visibility, stacking, plates, dimming under live floats. */
	void PaintGroundLabels(FAbyssPainter& P, const FVector2D& LocalSize, double Now) const;
	void PaintFloat(FAbyssPainter& P, const FFloat& Float, const FVector2D& LocalSize, double Now) const;
	/**
	 * A float T seconds after its spawn: offset from its anchor in web px (StartOffset + rise / drift), opacity and scale.
	 * False once its life is over. Shared by the paint and the placement check.
	 */
	static bool FloatMotion(const FFloat& Float, double T, FVector2D& OutOffset, float& OutAlpha, float& OutScale);
	/**
	 * Places the group of the label float just added (not numbers): the label floats spawned this frame on World, split
	 * in arrival order into groups holding each kind once - one kill's +EXP / +G / +Embers. The group is lifted together
	 * until, over its visible life, it clears every other float on screen, earlier groups on the same spot included
	 * (DECISIONS C13).
	 */
	void PlaceFloatGroup(const FVector& World, double Now);
	FString MonsterLabel(const abyss::MonsterView& Monster) const;

	TSharedPtr<FAbyssUiContext> Ctx;
	TMap<uint32, FLabel> Labels;
	mutable TArray<FFloat> Floats;
	mutable TArray<FPopup> Popups;
	/** Locale the cached label names were built in (-1 = none). */
	int32 NameLocale = -1;
	FVector HeroOverhead = FVector::ZeroVector;
	FVector HeroGround = FVector::ZeroVector;
	bool bHeroKnown = false;
	abyss::EntityId HoveredId = abyss::kNoEntity;
	abyss::EntityId TargetId = abyss::kNoEntity;
	/** Floating-number stacking (combat-feel 12): key = projected position / 28 px, last spawn time and index. */
	mutable TMap<int64, TPair<double, int32>> StackKeys;
	FRandomStream Random;

	// projection cache (one per paint)
	mutable bool bProjectionValid = false;
	mutable FMatrix ViewProjection = FMatrix::Identity;
	mutable FIntRect ViewRect;
	mutable FVector2D ViewportSize = FVector2D::ZeroVector;
	/** Local size of the last paint (AddFloatingText projects with the last paint's camera). */
	mutable FVector2D PaintLocalSize = FVector2D::ZeroVector;
	/** Ground-label layout scratch (one per paint). */
	mutable TArray<AbyssGroundLabels::FPlacement> GroundLabelPlacements;
	/**
	 * Slate px per web world px at the hero (the web draws zone texts as world objects under ZONE_CAMERA_ZOOM 1.8): the
	 * float / popup offsets, rises and stack rows are web world px times this. Kept from the last paint that saw the hero.
	 */
	mutable double WebPx = 1.0;
	// last values written to the log (LogAbyss "WorldLayer:" line, only when they change)
	mutable FVector2D LoggedLocalSize = FVector2D::ZeroVector;
	mutable FVector2D LoggedViewportSize = FVector2D::ZeroVector;
	mutable double LoggedWebPx = 0.0;
};
