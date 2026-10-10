#include "UI/Panels/SAbyssPanelBase.h"

#include "Widgets/SNullWidget.h"

void SAbyssPanelBase::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Ctx = InContext;
	ChildSlot
	[
		SNullWidget::NullWidget
	];
}

bool SAbyssPanelBase::MaybeRefresh(const abyss::Snapshot& Snap, double Now)
{
	const double Interval = GetAutoRefreshSeconds();
	const bool bDue = Interval > 0.0 && (LastRefreshTime < 0.0 || Now - LastRefreshTime >= Interval);
	if (!bDirty && !bDue)
	{
		return false;
	}
	bDirty = false;
	LastRefreshTime = Now;
	Refresh(Snap);
	return true;
}

void SAbyssPanelBase::RequestClose()
{
	CloseHandler.ExecuteIfBound();
}

void SAbyssPanelBase::SetPanelContent(const TSharedRef<SWidget>& Content)
{
	ChildSlot
	[
		Content
	];
}
