// Character panel (C; save-ui-input 7.3 with FIX Q19: crit / dodge / hit numbers come from the core's damage formula
// summary, never re-implemented here): six primary stats with the gear bonus and the "+" point allocation
// (CmdAllocStat), then the combat stats well.
#pragma once

#include "CoreMinimal.h"

#include "UI/Panels/SAbyssPanelBase.h"

class SCanvas;

class SAbyssCharacterPanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssCharacterPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::Character; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(380.0, 560.0); }
	virtual FVector2D GetDesignPosition() const override { return FVector2D(450.0, 14.0); }
	/** HP / MP move every frame in combat. */
	virtual double GetAutoRefreshSeconds() const override { return 0.5; }

private:
	TSharedPtr<SCanvas> Body;
};
