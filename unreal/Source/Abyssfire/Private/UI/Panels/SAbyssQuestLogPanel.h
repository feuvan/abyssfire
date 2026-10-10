// Quest log (J; save-ui-input 7.5, quests-story-ch1 5.5 with FIX Q18: every name through the accessors) with the lore
// tab (quests 10.4): tabs Active (active + completed) / Completed (turned in) / Lore, a paged list (13 rows, main quests
// first then by level) and the detail pane (description, special-type summary, objectives with progress bars, rewards,
// prerequisites, track / untrack).
#pragma once

#include "CoreMinimal.h"

#include <string>

#include "UI/Panels/SAbyssPanelBase.h"

class SCanvas;
class SScrollBox;

class SAbyssQuestLogPanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssQuestLogPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::QuestLog; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(720.0, 520.0); }
	virtual FVector2D GetDesignPosition() const override { return FVector2D(280.0, 24.0); }

private:
	void BuildQuestList(const abyss::Snapshot& Snap);
	void BuildLoreList(const abyss::Snapshot& Snap);
	TSharedRef<SWidget> BuildQuestDetail(const abyss::Snapshot& Snap, const std::string& QuestId);
	TSharedRef<SWidget> BuildLoreDetail(const abyss::Snapshot& Snap, const std::string& LoreId);
	void AddPager(int32 Total);

	TSharedPtr<SCanvas> Body;
	int32 Tab = 0;
	int32 Page = 0;
	std::string Selected[3];
};
