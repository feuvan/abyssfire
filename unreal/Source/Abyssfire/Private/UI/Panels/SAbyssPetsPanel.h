// Ley-beasts panel (P; quests-story-ch1 18.8, DECISIONS Q3: owned beasts only, the Ember Tower hints hidden): list of
// owned beasts (active first, then acquisition order), the selected beast's portrait, name with its evolution, role,
// description, level / exp bar and next evolution, bond pips (dimmed above the cap), passive bonus, abilities (locked until
// awakened), and the Summon / Rest (CmdSetActivePet) and Feed (CmdFeedPet, ley fruit count) buttons.
#pragma once

#include "CoreMinimal.h"

#include <string>

#include "UI/Panels/SAbyssPanelBase.h"

class SCanvas;

class SAbyssPetsPanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssPetsPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::Pets; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(660.0, 540.0); }
	virtual FVector2D GetDesignPosition() const override { return FVector2D(310.0, 10.0); }
	virtual double GetAutoRefreshSeconds() const override { return 1.0; }

private:
	TSharedPtr<SCanvas> Body;
	std::string SelectedPet;
};
