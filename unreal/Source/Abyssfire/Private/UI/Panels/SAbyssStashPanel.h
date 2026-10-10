// Stash (save-ui-input 7.8, loot-items-inventory 10; core-owned modal Stash, opened by the stash keeper NPC, I7):
// * left: the stash grid (8 x 5 per page over max(capacity, stored) cells; cells past the capacity read as locked) with
//   its count / capacity header and Sort (CmdSortStash);
// * right: the bag grid (8 x 5 per page) with its count header and Sort (CmdSortBag);
// * desktop: a click moves the item straight across (CmdStashTake / CmdStashPut; the core logs a full bag / stash);
//   touch: a tap shows the item card with an explicit Take / Store button, so browsing never moves things by accident.
#pragma once

#include "CoreMinimal.h"

#include <string>

#include "abyss/items/Item.h"
#include "abyss/sim/SimTypes.h"

#include "UI/Panels/SAbyssPanelBase.h"

class SCanvas;

class SAbyssStashPanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssStashPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const std::string& InNpcId);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::Stash; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(820.0, 480.0); }
	virtual FVector2D GetDesignPosition() const override { return FVector2D(-1.0, 40.0); }
	virtual bool IsModal() const override { return true; }
	virtual float GetBackdropAlpha() const override { return 0.6f; }
	virtual std::string GetNpcId() const override { return NpcId; }

private:
	/** One 8 x 5 grid with its pager. bStash: the stash side (take), else the bag (store). */
	void BuildGrid(const abyss::Snapshot& Snap, bool bStash);
	void OnItem(const abyss::ItemInstance& Item, const FVector2D& AbsolutePosition, bool bStash);
	void Move(const abyss::ItemInstance& Item, bool bStash);

	TSharedPtr<SCanvas> Body;
	std::string NpcId;
	int32 StashPage = 0;
	int32 BagPage = 0;
};
