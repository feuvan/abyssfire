// The system menu (DECISIONS U4 / U9 / C12, save-ui-input Q10 / Q30 fixes): Resume, Settings, Achievements, Controls,
// Save & return to the title, Quit (not on iOS). A UE-owned modal that freezes the world (CmdOpenPanel{SystemMenu});
// the backdrop does not close it. Settings and achievements open over it (Back returns here).
#pragma once

#include "CoreMinimal.h"

#include "UI/Panels/SAbyssPanelBase.h"

class SAbyssSystemMenuPanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssSystemMenuPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::SystemMenu; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override;
	virtual bool IsModal() const override { return true; }
	virtual float GetBackdropAlpha() const override { return 0.65f; }
	virtual bool CloseOnBackdrop() const override { return false; }

private:
	TSharedPtr<class SVerticalBox> Buttons;
	int32 ButtonCount = 6;
};
