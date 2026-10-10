// Shop / blacksmith (save-ui-input 7.7, loot-items-inventory 12-13; core-owned modal Shop / Forge):
// * left: the wares in a scrolling list (FIX loot Q7) with Buy (CmdBuy; disabled when short of gold), the buyback list
//   (CmdBuyback) and the gold line; the blacksmith adds the Forge tab (13.5): the anvil card, owned materials and one
//   row per action (salvage / reforge / upgrade / socket) with its preview, cost chips or block reason (CheckCraft),
//   confirm on salvaging rare+ or socketed gear -> CmdCraft;
// * right: the bag (8 x 5 per page): click / right click sells (legendary / set confirm), in the forge tab a click puts
//   gear on the anvil; touch shows the item card with the explicit action.
#pragma once

#include "CoreMinimal.h"

#include <string>

#include "abyss/data/ItemData.h"
#include "abyss/items/Item.h"
#include "abyss/sim/SimTypes.h"

#include "UI/Panels/SAbyssPanelBase.h"

class SCanvas;
class SScrollBox;

class SAbyssShopPanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssShopPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const std::string& InNpcId, abyss::PanelId InPanel);

	virtual abyss::PanelId GetPanelId() const override { return Panel; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(780.0, 480.0); }
	virtual FVector2D GetDesignPosition() const override { return FVector2D(250.0, 40.0); }
	virtual bool IsModal() const override { return true; }
	virtual float GetBackdropAlpha() const override { return 0.6f; }
	virtual std::string GetNpcId() const override { return NpcId; }
	virtual bool HandleBack() override;

private:
	void BuildWares(const abyss::Snapshot& Snap);
	void BuildForge(const abyss::Snapshot& Snap);
	void BuildBag(const abyss::Snapshot& Snap);
	void OnBagItem(const abyss::ItemInstance& Item, const FVector2D& AbsolutePosition, bool bRightClick);
	void Sell(const abyss::ItemInstance& Item);
	void Craft(abyss::CraftAction Action, const abyss::ItemInstance& Item);

	TSharedPtr<SCanvas> Body;
	TSharedPtr<SScrollBox> WareScroll;
	std::string NpcId;
	abyss::PanelId Panel = abyss::PanelId::Shop;
	bool bBlacksmith = false;
	int32 Tab = 0;  // 0 wares, 1 forge
	int32 BagPage = 0;
	std::string AnvilUid;
	float SavedWareScroll = 0.f;
};
