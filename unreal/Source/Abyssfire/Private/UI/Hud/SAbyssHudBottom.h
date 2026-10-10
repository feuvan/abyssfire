// SAbyssHudBottom: the bottom HUD group (save-ui-input 6.2-6.6, 6.8 prompt, DECISIONS I4 / W3): HUD plate, HP / MP orbs,
// spirit and exp bars, the 6-slot skill bar with cooldown sweeps (desktop), the auto-combat / auto-loot / bag buttons
// (desktop), the HP / MP potion quick slots (desktop; touch has its own buttons), the town-portal channel bar and the
// interact prompt. Painted in one pass from FAbyssHudState; the interactive parts are invisible buttons placed exactly
// over their rects so the rest of the region stays click-through to the world.
//
// Region: the web's design coordinates x 0..1280, y 560..720 (local y = design y - 560). Desktop anchors it bottom-centre,
// touch bottom-left (the touch HUD leaves the right side to the skill fan).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "UI/Core/AbyssUiContext.h"
#include "UI/Hud/AbyssHudState.h"

class SCanvas;
struct FAbyssPainter;

class SAbyssHudBottom : public SCompoundWidget
{
public:
	static constexpr float RegionTop = 560.f;
	static constexpr float RegionHeight = 160.f;

	SLATE_BEGIN_ARGS(SAbyssHudBottom) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState);
	/** Re-creates the hit areas for the desktop / touch layout. */
	void RebuildLayout();

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	struct FLayout
	{
		bool bTouch = false;
		float OrbR = 44.f;
		FVector2D HpOrb, MpOrb;
		FVector2D PlatePos, PlateSize;
		FVector2D SpiritPos, SpiritSize;
		FVector2D SpiritLabel;
		FVector2D ExpPos, ExpSize;
		float SkillSize = 44.f;
		FVector2D SkillStart;
		float SkillGap = 6.f;
		FVector2D UtilityStart;
		FVector2D UtilitySize;
		float UtilityGap = 5.f;
		FVector2D PotionCenters[2];
		float PotionR = 15.f;
		FVector2D PromptCenter;
		FVector2D PortalCenter;
		float FontScale = 1.f;
	};

	static FLayout MakeLayout(bool bTouch);
	float Px(float Desktop) const;

	void PaintPlate(FAbyssPainter& P) const;
	void PaintOrb(FAbyssPainter& P, const FVector2D& Center, float Radius, bool bHp, double Now) const;
	void PaintSpirit(FAbyssPainter& P, double Now) const;
	void PaintExp(FAbyssPainter& P) const;
	void PaintSkillSlot(FAbyssPainter& P, int32 Slot, double Now) const;
	void PaintUtility(FAbyssPainter& P) const;
	void PaintPotion(FAbyssPainter& P, int32 Slot) const;
	void PaintPrompt(FAbyssPainter& P) const;
	void PaintPortal(FAbyssPainter& P) const;

	TSharedRef<SWidget> MakeHitArea(TFunction<void()> OnPress, TFunction<void()> OnRightPress, TSharedPtr<SWidget>& OutWidget);
	void ShowPotionPopup(int32 Slot);

	TSharedPtr<FAbyssUiContext> Ctx;
	TSharedPtr<FAbyssHudState> State;
	TSharedPtr<SCanvas> HitCanvas;
	TSharedPtr<SWidget> SkillButtons[6];
	TSharedPtr<SWidget> UtilityButtons[3];
	TSharedPtr<SWidget> PotionButtons[2];
	TSharedPtr<SWidget> BlockAreas[3];
	FLayout Layout;
};
