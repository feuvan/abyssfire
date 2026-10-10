// Achievements (V; save-ui-input 7.14 with FIX Q17: names, descriptions, titles and stat labels through the accessors):
// the unlocked count, the current title (the last unlocked achievement in definition order that grants one), and every
// achievement as a row (medal, name, description, progress bar, reward).
#pragma once

#include "CoreMinimal.h"

#include "UI/Panels/SAbyssPanelBase.h"

class SScrollBox;
class SVerticalBox;

class SAbyssAchievementsPanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssAchievementsPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::Achievements; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(560.0, 560.0); }
	virtual FVector2D GetDesignPosition() const override { return FVector2D(360.0, 10.0); }
	virtual double GetAutoRefreshSeconds() const override { return 1.0; }

private:
	TSharedPtr<SVerticalBox> Header;
	TSharedPtr<SScrollBox> Scroll;
};
