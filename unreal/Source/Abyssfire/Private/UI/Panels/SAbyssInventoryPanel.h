// Inventory (save-ui-input 7.1, loot 7-9; DECISIONS I3 / I4 / I5 / C12) and the socket sub-panel (7.9 / loot 9.4).
#pragma once

#include "CoreMinimal.h"

#include <optional>
#include <string>
#include <vector>

#include "abyss/base/Stats.h"
#include "abyss/items/Item.h"

#include "UI/Panels/SAbyssPanelBase.h"

class SCanvas;

class SAbyssInventoryPanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssInventoryPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::Inventory; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(740.0, 450.0); }
	virtual FVector2D GetDesignPosition() const override { return FVector2D(-1.0, 12.0); }

private:
	void BuildPaperDoll(const TSharedRef<SCanvas>& Canvas, const abyss::Snapshot& Snap);
	void BuildBag(const TSharedRef<SCanvas>& Canvas, const abyss::Snapshot& Snap);
	void BuildBonus(const TSharedRef<SCanvas>& Canvas, const abyss::Snapshot& Snap);
	void OnBagItemClicked(const abyss::ItemInstance& Item, const FVector2D& AbsolutePosition);
	void OnEquippedClicked(abyss::EquipSlot Slot, const abyss::ItemInstance& Item, int32 SocketCapacity, const FVector2D& AbsolutePosition);
	void Discard(const abyss::ItemInstance& Item);

	TSharedPtr<SCanvas> Body;
	int32 Page = 0;
	bool bDying = false;
};

class SAbyssSocketPanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssSocketPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, abyss::EquipSlot InSlot);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::Socket; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(400.0, 420.0); }
	virtual FVector2D GetDesignPosition() const override { return FVector2D(-1.0, 50.0); }
	virtual bool IsModal() const override { return true; }
	virtual float GetBackdropAlpha() const override { return 0.35f; }
	abyss::EquipSlot GetSlot() const { return Slot; }

private:
	TSharedPtr<SCanvas> Body;
	abyss::EquipSlot Slot = abyss::EquipSlot::Weapon;
};
