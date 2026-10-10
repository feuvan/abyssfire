// Skill tree (K; save-ui-input 7.4, classes-stats-skills 7) and the hotbar editor (DECISIONS C3):
// * one tab per skill tree of the class (tab order, learned-count badge, tree colour), cards sorted by tier and joined by
//   arrows, card states maxed / learned / investable / locked with the lock reason, level pips, the numbers row, the
//   synergy badge and the "+" (CmdLearnSkill); tooltip with the current and next level (desktop hover, touch tap popup);
// * the 6-slot hotbar strip: pick a learned active skill (card click), then a slot (CmdSetHotbar); click a bound slot to
//   pick its skill up and move it; right click (touch: popup) clears a slot.
#pragma once

#include "CoreMinimal.h"

#include <string>

#include "UI/Panels/SAbyssPanelBase.h"

class SCanvas;
class SScrollBox;
class SVerticalBox;

namespace abyss
{
	struct SkillDef;
}

class SAbyssSkillTreePanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssSkillTreePanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::Skills; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(660.0, 600.0); }
	virtual FVector2D GetDesignPosition() const override { return FVector2D(310.0, 5.0); }
	virtual bool HandleBack() override;

	/** The tooltip body of one skill (also the touch popup's card). */
	static TSharedRef<SWidget> MakeSkillTooltip(const TSharedRef<FAbyssUiContext>& Ctx, const abyss::Snapshot& Snap, int32 SkillIndex);

	// card / slot callbacks
	void OnCardClicked(int32 SkillIndex, const FVector2D& AbsolutePosition, bool bTouch);
	void OnLearnClicked(int32 SkillIndex);
	void OnHotbarSlotClicked(int32 Slot, const FVector2D& AbsolutePosition, bool bTouch);
	void OnHotbarSlotCleared(int32 Slot);
	int32 GetSelectedSkill() const { return SelectedSkill; }

private:
	void BuildHeader(const abyss::Snapshot& Snap);
	void BuildCards(const abyss::Snapshot& Snap);
	void BuildHotbar(const abyss::Snapshot& Snap);

	TSharedPtr<SCanvas> Body;
	TSharedPtr<SScrollBox> Scroll;
	TSharedPtr<SVerticalBox> CardList;
	TArray<std::string> TreeIds;
	int32 ActiveTab = 0;
	int32 SelectedSkill = -1;
	float SavedScroll = 0.f;
	int32 ScrollTab = -1;
};
