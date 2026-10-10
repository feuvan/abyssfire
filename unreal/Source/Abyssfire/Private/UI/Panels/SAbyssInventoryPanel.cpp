#include "UI/Panels/SAbyssInventoryPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Text/STextBlock.h"

#include <string>
#include <utility>

#include "abyss/data/DataStore.h"
#include "abyss/items/Inventory.h"

#include "Framework/AbyssText.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	struct FAbyssDollSlot
	{
		abyss::EquipSlot Slot;
		int32 Column;  // -1 / 0 / +1
		int32 Row;
	};

	// save-ui-input 7.1 paper doll grid
	const FAbyssDollSlot GAbyssDollSlots[] = {
		{ abyss::EquipSlot::Helmet, 0, 0 },
		{ abyss::EquipSlot::Weapon, -1, 1 }, { abyss::EquipSlot::Armor, 0, 1 }, { abyss::EquipSlot::Offhand, 1, 1 },
		{ abyss::EquipSlot::Gloves, -1, 2 }, { abyss::EquipSlot::Belt, 0, 2 }, { abyss::EquipSlot::Boots, 1, 2 },
		{ abyss::EquipSlot::Ring1, -1, 3 }, { abyss::EquipSlot::Necklace, 0, 3 }, { abyss::EquipSlot::Ring2, 1, 3 },
	};

	bool AbyssInventory_IsConsumable(const abyss::DataStore& Data, const abyss::ItemInstance& Item, abyss::ConsumableEffect& OutEffect)
	{
		const abyss::ItemBaseDef* Base = Data.FindItemBase(Item.baseId);
		if (Base == nullptr || (Base->type != abyss::ItemType::Consumable && Base->type != abyss::ItemType::Scroll))
		{
			return false;
		}
		OutEffect = Base->consumableEffect;
		return !Data.Items().IsRemovedItem(Item.baseId);
	}

	/** Context popup: optional item card (touch) + a column of action buttons. */
	TSharedRef<SWidget> AbyssInventory_Popup(const TSharedRef<FAbyssUiContext>& Ctx, const abyss::ItemInstance* Item,
		TArray<TTuple<FText, EAbyssButtonKind, TFunction<void()>>>& Actions, bool bEnabled)
	{
		const bool bTouch = Ctx->IsTouch();
		const float Width = bTouch ? 190.f : 108.f;
		const float Height = bTouch ? 56.f : 28.f;
		TSharedRef<SVerticalBox> Buttons = SNew(SVerticalBox);
		for (TTuple<FText, EAbyssButtonKind, TFunction<void()>>& Action : Actions)
		{
			TFunction<void()> Handler = MoveTemp(Action.Get<2>());
			Buttons->AddSlot()
				.AutoHeight()
				.Padding(FMargin(0.f, 2.f))
				[
					SNew(SAbyssButton, Ctx)
					.Text(Action.Get<0>())
					.Kind(Action.Get<1>())
					.FontPx(bTouch ? 20.f : 12.f)
					.Width(Width)
					.Height(Height)
					.IsEnabled(bEnabled)
					.OnClicked_Lambda([Ctx, Handler]()
					{
						Ctx->ClosePopup();
						if (Handler)
						{
							Handler();
						}
					})
				];
		}
		TSharedRef<SWidget> Panel = SNew(SAbyssCardFrame, Ctx).Padding(FMargin(6.f))[Buttons];
		if (bTouch && Item != nullptr)
		{
			// touch: the card beside the actions (7.2), scaled x1.45 by the popup host
			AbyssItemTooltip::FOptions Options;
			Options.bCompare = true;
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[AbyssItemTooltip::MakeCard(Ctx, *Item, Options)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(8.f, 0.f, 0.f, 0.f))[Panel];
		}
		return Panel;
	}
}

// =====================================================================================================================
// Inventory
// =====================================================================================================================

void SAbyssInventoryPanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	const TSharedRef<FAbyssUiContext> Context = InContext;
	SetPanelContent(
		SNew(SAbyssPanelFrame, Context)
		.Size(GetDesignSize())
		.Title_Lambda([Context]()
		{
			const abyss::Snapshot* Snap = Context->GetSnapshot();
			const int64 Count = Snap && Snap->inventory ? static_cast<int64>(Snap->inventory->Bag().size()) : 0;
			const int64 Max = Snap && Snap->inventory ? Snap->inventory->Capacity() : abyss::kMaxInventoryEntries;
			return Context->LocArgsOr("ui.inventory.title", TEXT("Inventory ({count}/{max})"),
				{ FAbyssUiContext::Arg("count", Count), FAbyssUiContext::Arg("max", Max) });
		})
		.ContentPadding(FMargin(0.f))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SAssignNew(Body, SCanvas)
		]);
}

void SAbyssInventoryPanel::Refresh(const abyss::Snapshot& Snap)
{
	Body->ClearChildren();
	if (Snap.inventory == nullptr || Ctx->GetData() == nullptr)
	{
		return;
	}
	bDying = Ctx->IsHeroDying();
	const TSharedRef<SCanvas> Canvas = Body.ToSharedRef();
	BuildPaperDoll(Canvas, Snap);
	BuildBag(Canvas, Snap);
	BuildBonus(Canvas, Snap);
}

void SAbyssInventoryPanel::BuildPaperDoll(const TSharedRef<SCanvas>& Canvas, const abyss::Snapshot& Snap)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const abyss::Inventory& Inventory = *Snap.inventory;
	const FAbyssUiPalette& C = Ctx->Style().Colors();

	// faint plinth behind the doll
	Canvas->AddSlot().Position(FVector2D(18.0, 46.0)).Size(FVector2D(236.0, 300.0))
	[
		SNew(SAbyssCanvas, Context, [](FAbyssPainter& P, const FVector2D& Size)
		{
			P.Glow(FVector2D(Size.X * 0.5, Size.Y * 0.55), static_cast<float>(Size.X * 0.45), FLinearColor(0.85f, 0.6f, 0.3f, 0.08f), 6);
		})
	];
	Canvas->AddSlot().Position(FVector2D(22.0, 46.0)).Size(FVector2D(228.0, 18.0))
	[
		AbyssUi::SectionHeader(Context, Ctx->LocOr("ui.inventory.equipment", TEXT("Equipment")), 228.f)
	];
	const abyss::DataStore& Data = *Ctx->GetData();
	for (const FAbyssDollSlot& Doll : GAbyssDollSlots)
	{
		const double CenterX = 136.0 + Doll.Column * 70.0;
		const double Top = 72.0 + Doll.Row * 64.0;
		const abyss::ItemInstance* Equipped = Inventory.Equipped(Doll.Slot);
		FAbyssSlotItem Entry;
		Entry.bCompare = false;
		Entry.GhostSlot = Doll.Slot;
		int32 Capacity = 0;
		if (Equipped != nullptr)
		{
			Entry.Item = *Equipped;
			Capacity = abyss::ItemSocketCapacity(*Equipped, Data);
			Entry.SocketsFilled = static_cast<int32>(Equipped->sockets.size());
			Entry.SocketsMax = Capacity;
		}
		const abyss::EquipSlot SlotId = Doll.Slot;
		const TOptional<abyss::ItemInstance> ItemCopy = Entry.Item;
		Canvas->AddSlot().Position(FVector2D(CenterX - 22.0, Top)).Size(FVector2D(44.0, 44.0))
		[
			SNew(SAbyssItemSlot, Context)
			.SlotSize(44.f)
			.Entry(Entry)
			.OnClicked_Lambda([this, SlotId, ItemCopy, Capacity](const FVector2D& Position)
			{
				if (ItemCopy.IsSet())
				{
					OnEquippedClicked(SlotId, ItemCopy.GetValue(), Capacity, Position);
				}
			})
		];
		Canvas->AddSlot().Position(FVector2D(CenterX - 34.0, Top + 45.0)).Size(FVector2D(68.0, 14.0))
		[
			SNew(STextBlock)
			.Text(FText::AsCultureInvariant(Ctx->NameOr("ui.inventory.slot." + std::string(abyss::EnumName(Doll.Slot)),
				std::string(abyss::EnumName(Doll.Slot)))))
			.Font(Ctx->Style().Body(10.f, false, 1))
			.ColorAndOpacity(FSlateColor(C.Muted))
			.Justification(ETextJustify::Center)
		];
	}
	// under the doll: divider, coin + gold, hint
	Canvas->AddSlot().Position(FVector2D(22.0, 336.0)).Size(FVector2D(228.0, 12.0))[AbyssUi::Divider(Context, 228.f, true)];
	const FString Gold = FAbyssUiContext::Int(Snap.hero.gold);
	Canvas->AddSlot().Position(FVector2D(22.0, 352.0)).Size(FVector2D(228.0, 22.0))
	[
		SNew(SAbyssCanvas, Context, [Gold, Context](FAbyssPainter& P, const FVector2D& Size)
		{
			const FSlateFontInfo Font = Context->Style().Body(14.f, true, 2);
			const FVector2D TextSize = FAbyssPainter::Measure(Gold, Font);
			const double X = Size.X * 0.5 - (TextSize.X + 20.0) * 0.5;
			P.Circle(FVector2D(X + 7.0, Size.Y * 0.5), 7.f, FAbyssUiStyle::Rgb(0xe3b44c), FAbyssUiStyle::Rgb(0x8a5a10), 1.f);
			P.Text(FVector2D(X + 20.0, Size.Y * 0.5 - TextSize.Y * 0.5), Gold, Font, FAbyssUiStyle::Rgb(0xffd35a));
		})
	];
	Canvas->AddSlot().Position(FVector2D(22.0, 378.0)).Size(FVector2D(228.0, 40.0))
	[
		AbyssUi::Label(*Ctx, Ctx->LocOr("ui.inventory.equipHint", TEXT("Click a worn item to unequip it or socket gems")), 10.f, C.Muted, false, 1, 226.f)
	];
}

void SAbyssInventoryPanel::BuildBag(const TSharedRef<SCanvas>& Canvas, const abyss::Snapshot& Snap)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const abyss::Inventory& Inventory = *Snap.inventory;
	const std::span<const abyss::ItemInstance> Bag = Inventory.Bag();
	constexpr int32 PerPage = 50;
	const int32 Pages = FMath::Max(1, (static_cast<int32>(Bag.size()) + PerPage - 1) / PerPage);
	Page = FMath::Clamp(Page, 0, Pages - 1);

	Canvas->AddSlot().Position(FVector2D(274.0, 46.0)).Size(FVector2D(448.0, 18.0))
	[
		AbyssUi::SectionHeader(Context, Ctx->LocOr("ui.inventory.bag", TEXT("Bag")), 448.f)
	];
	for (int32 Cell = 0; Cell < PerPage; ++Cell)
	{
		const int32 Column = Cell % 10;
		const int32 Row = Cell / 10;
		const FVector2D Pos(274.0 + Column * 45.0, 66.0 + Row * 45.0);
		const int32 Index = Page * PerPage + Cell;
		FAbyssSlotItem Entry;
		if (Index < static_cast<int32>(Bag.size()))
		{
			Entry.Item = Bag[static_cast<size_t>(Index)];
		}
		else
		{
			Entry.bDimmed = true;
		}
		const TOptional<abyss::ItemInstance> ItemCopy = Entry.Item;
		Canvas->AddSlot().Position(Pos).Size(FVector2D(41.0, 41.0))
		[
			SNew(SAbyssItemSlot, Context)
			.SlotSize(41.f)
			.Entry(Entry)
			.OnClicked_Lambda([this, ItemCopy](const FVector2D& Position)
			{
				if (ItemCopy.IsSet())
				{
					OnBagItemClicked(ItemCopy.GetValue(), Position);
				}
			})
			.OnRightClicked_Lambda([this, ItemCopy](const FVector2D& Position)
			{
				// desktop shortcut: equip gear / use consumables directly
				const abyss::DataStore* Data = Ctx->GetData();
				if (!ItemCopy.IsSet() || Data == nullptr || bDying)
				{
					return;
				}
				const abyss::ItemInstance& Item = ItemCopy.GetValue();
				abyss::ConsumableEffect Effect = abyss::ConsumableEffect::None;
				if (abyss::IsEquipmentItem(Item, *Data))
				{
					Ctx->Submit(abyss::CmdEquip{ Item.uid });
				}
				else if (AbyssInventory_IsConsumable(*Data, Item, Effect) && Effect != abyss::ConsumableEffect::None)
				{
					Ctx->Submit(abyss::CmdUseItem{ Item.uid });
				}
			})
		];
	}

	// toolbar (y 307): pages when > 50 items, sort, destroy normals (no confirm, Q32)
	if (Pages > 1)
	{
		Canvas->AddSlot().Position(FVector2D(274.0, 300.0)).Size(FVector2D(220.0, 26.0))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("ui.inventory.prevPage", TEXT("< Prev")))
				.Kind(EAbyssButtonKind::Ghost).FontPx(11.f).Width(64.f).Height(24.f)
				.IsEnabled(Page > 0)
				.OnClicked_Lambda([this]()
				{
					--Page;
					MarkDirty();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8.f, 0.f))
			[
				SNew(STextBlock)
				.Text(Ctx->LocArgsOr("ui.inventory.pageLabel", TEXT("Page {current}/{total}"),
					{ FAbyssUiContext::Arg("current", Page + 1), FAbyssUiContext::Arg("total", Pages) }))
				.Font(Ctx->Style().Body(11.f, false, 1))
				.ColorAndOpacity(FSlateColor(Ctx->Style().Colors().TextSoft))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("ui.inventory.nextPage", TEXT("Next >")))
				.Kind(EAbyssButtonKind::Ghost).FontPx(11.f).Width(64.f).Height(24.f)
				.IsEnabled(Page + 1 < Pages)
				.OnClicked_Lambda([this]()
				{
					++Page;
					MarkDirty();
				})
			]
		];
	}
	Canvas->AddSlot().Position(FVector2D(548.0, 300.0)).Size(FVector2D(76.0, 26.0))
	[
		SNew(SAbyssButton, Context)
		.Text(Ctx->LocOr("ui.inventory.sort", TEXT("Sort")))
		.Kind(EAbyssButtonKind::Secondary).FontPx(12.f).Width(76.f).Height(26.f)
		.IsEnabled(!bDying)
		.OnClicked_Lambda([Context]() { Context->Submit(abyss::CmdSortBag{}); })
	];
	Canvas->AddSlot().Position(FVector2D(630.0, 300.0)).Size(FVector2D(92.0, 26.0))
	[
		SNew(SAbyssButton, Context)
		.Text(Ctx->LocOr("ui.inventory.destroy", TEXT("Destroy")))
		.Kind(EAbyssButtonKind::Danger).FontPx(12.f).Width(92.f).Height(26.f)
		.IsEnabled(!bDying)
		.OnClicked_Lambda([this, Context]()
		{
			Page = 0;
			Context->Submit(abyss::CmdDestroyNormals{});
		})
	];
}

void SAbyssInventoryPanel::BuildBonus(const TSharedRef<SCanvas>& Canvas, const abyss::Snapshot& Snap)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	Canvas->AddSlot().Position(FVector2D(274.0, 333.0)).Size(FVector2D(448.0, 18.0))
	[
		AbyssUi::SectionHeader(Context, Ctx->LocOr("ui.inventory.bonusHeader", TEXT("Equipment bonus")), 448.f)
	];
	// getEquipmentStats non-zero entries as "{label} +{v}{%}" joined by three spaces (7.1)
	const abyss::StatBag Bag = Snap.inventory->EquipmentStatBag();
	TArray<FString> Parts;
	for (const abyss::StatValue& Value : Bag.Items())
	{
		if (Value.value == 0.0 || Value.stat == abyss::Stat::WeaponDamageMin || Value.stat == abyss::Stat::WeaponDamageMax)
		{
			continue;
		}
		Parts.Add(FString::Printf(TEXT("%s %s"), *Ctx->StatLabel(Value.stat), *Ctx->StatValue(Value.stat, Value.value)));
	}
	const FString Text = Parts.Num() > 0 ? FString::Join(Parts, TEXT("   ")) : Ctx->LocOrStr("ui.inventory.bonusNone", TEXT("No bonuses"));
	Canvas->AddSlot().Position(FVector2D(274.0, 355.0)).Size(FVector2D(448.0, 84.0))
	[
		SNew(SBox)
		.Clipping(EWidgetClipping::ClipToBounds)
		[
			AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Text), 11.f, FAbyssUiStyle::Rgb(0x9fd4ff), false, 1, 446.f)
		]
	];
}

void SAbyssInventoryPanel::OnBagItemClicked(const abyss::ItemInstance& Item, const FVector2D& AbsolutePosition)
{
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr)
	{
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	TArray<TTuple<FText, EAbyssButtonKind, TFunction<void()>>> Actions;
	const std::string Uid = Item.uid;
	abyss::ConsumableEffect Effect = abyss::ConsumableEffect::None;
	if (abyss::IsEquipmentItem(Item, *Data))
	{
		Actions.Add(MakeTuple(Ctx->LocOr("ui.context.equip", TEXT("Equip")), EAbyssButtonKind::Primary,
			TFunction<void()>([Context, Uid]() { Context->Submit(abyss::CmdEquip{ Uid }); })));
	}
	else if (AbyssInventory_IsConsumable(*Data, Item, Effect) && Effect != abyss::ConsumableEffect::None)
	{
		Actions.Add(MakeTuple(Ctx->LocOr("ui.context.use", TEXT("Use")), EAbyssButtonKind::Primary,
			TFunction<void()>([Context, Uid]() { Context->Submit(abyss::CmdUseItem{ Uid }); })));
		if (Effect == abyss::ConsumableEffect::Heal || Effect == abyss::ConsumableEffect::Mana)
		{
			// I4: bind this potion to its HUD quick slot
			const abyss::PotionSlot PotionSlot = Effect == abyss::ConsumableEffect::Heal ? abyss::PotionSlot::Hp : abyss::PotionSlot::Mp;
			const std::string BaseId = Item.baseId;
			Actions.Add(MakeTuple(Ctx->LocOr("ui.context.quickSlot", TEXT("Quick slot")), EAbyssButtonKind::Secondary,
				TFunction<void()>([Context, PotionSlot, BaseId]() { Context->Submit(abyss::CmdSetPotionSlot{ PotionSlot, BaseId }); })));
		}
	}
	const abyss::ItemInstance Copy = Item;
	Actions.Add(MakeTuple(Ctx->LocOr("ui.context.discard", TEXT("Discard")), EAbyssButtonKind::Danger,
		TFunction<void()>([this, Copy]() { Discard(Copy); })));
	Ctx->HideTooltip(nullptr);
	Ctx->ShowPopup(AbyssInventory_Popup(Context, &Item, Actions, !bDying), AbsolutePosition);
}

void SAbyssInventoryPanel::Discard(const abyss::ItemInstance& Item)
{
	// rare / legendary / set ask first (7.2); others go at once
	const std::string Uid = Item.uid;
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	if (abyss::QualityMeetsFloor(Item.quality, abyss::ItemQuality::Rare))
	{
		FAbyssConfirmRequest Request;
		Request.Title = Ctx->LocOr("ui.context.discardConfirmTitle", TEXT("Discard this item?"));
		Request.Body = FText::AsCultureInvariant(Ctx->ItemName(Item));
		Request.BodyColor = Ctx->Style().QualityColor(Item.quality);
		Request.ConfirmLabel = Ctx->LocOr("ui.context.confirmYes", TEXT("Confirm"));
		Request.CancelLabel = Ctx->LocOr("ui.context.confirmNo", TEXT("Cancel"));
		Request.bDanger = true;
		Request.OnConfirm = [Context, Uid]() { Context->Submit(abyss::CmdDiscardItem{ Uid }); };
		Ctx->Confirm(MoveTemp(Request));
		return;
	}
	Ctx->Submit(abyss::CmdDiscardItem{ Uid });
}

void SAbyssInventoryPanel::OnEquippedClicked(abyss::EquipSlot Slot, const abyss::ItemInstance& Item, int32 SocketCapacity,
	const FVector2D& AbsolutePosition)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	if (!Ctx->IsTouch())
	{
		// desktop: sockets -> socket panel, else unequip at once (7.1)
		if (bDying)
		{
			return;
		}
		if (SocketCapacity > 0)
		{
			if (IAbyssUiHost* Host = Ctx->GetHost())
			{
				Host->OpenSocketPanel(Slot);
			}
		}
		else
		{
			Ctx->Submit(abyss::CmdUnequip{ Slot });
		}
		return;
	}
	TArray<TTuple<FText, EAbyssButtonKind, TFunction<void()>>> Actions;
	if (SocketCapacity > 0)
	{
		Actions.Add(MakeTuple(Ctx->LocOr("ui.socket.title", TEXT("Sockets")), EAbyssButtonKind::Secondary, TFunction<void()>([Context, Slot]()
		{
			if (IAbyssUiHost* Host = Context->GetHost())
			{
				Host->OpenSocketPanel(Slot);
			}
		})));
	}
	Actions.Add(MakeTuple(Ctx->LocOr("ui.socket.unequip", TEXT("Unequip")), EAbyssButtonKind::Primary,
		TFunction<void()>([Context, Slot]() { Context->Submit(abyss::CmdUnequip{ Slot }); })));
	Ctx->ShowPopup(AbyssInventory_Popup(Context, &Item, Actions, !bDying), AbsolutePosition);
}

// =====================================================================================================================
// Socket panel (loot 9.4)
// =====================================================================================================================

void SAbyssSocketPanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, abyss::EquipSlot InSlot)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	Slot = InSlot;
	SetPanelContent(
		SNew(SAbyssPanelFrame, InContext)
		.Size(GetDesignSize())
		.Title(InContext->LocOr("ui.socket.title", TEXT("Sockets")))
		.ContentPadding(FMargin(0.f))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SAssignNew(Body, SCanvas)
		]);
}

void SAbyssSocketPanel::Refresh(const abyss::Snapshot& Snap)
{
	Body->ClearChildren();
	const abyss::DataStore* Data = Ctx->GetData();
	if (Snap.inventory == nullptr || Data == nullptr)
	{
		return;
	}
	const abyss::ItemInstance* Equipped = Snap.inventory->Equipped(Slot);
	if (Equipped == nullptr)
	{
		// unequipped meanwhile: nothing to socket
		RequestClose();
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const bool bDying = Ctx->IsHeroDying();
	const int32 Capacity = abyss::ItemSocketCapacity(*Equipped, *Data);
	const int32 Filled = static_cast<int32>(Equipped->sockets.size());
	const abyss::EquipSlot SlotId = Slot;

	// the item
	FAbyssSlotItem ItemEntry;
	ItemEntry.Item = *Equipped;
	ItemEntry.bCompare = false;
	Body->AddSlot().Position(FVector2D(20.0, 48.0)).Size(FVector2D(44.0, 44.0))
	[
		SNew(SAbyssItemSlot, Context).SlotSize(44.f).Entry(ItemEntry)
	];
	Body->AddSlot().Position(FVector2D(74.0, 50.0)).Size(FVector2D(306.0, 40.0))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Ctx->ItemName(*Equipped)), 14.f, Ctx->Style().QualityColor(Equipped->quality), true, 1, 300.f)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			AbyssUi::Label(*Ctx, Ctx->LocArgsOr("ui.socket.slotCount", TEXT("Sockets ({filled}/{max})"),
				{ FAbyssUiContext::Arg("filled", Filled), FAbyssUiContext::Arg("max", Capacity) }), 11.f, FAbyssUiStyle::Rgb(0x8be9fd))
		]
	];

	// sockets: filled (gem, tier, effect, remove) or empty
	double Y = 104.0;
	for (int32 Index = 0; Index < Capacity; ++Index)
	{
		const FVector2D RowPos(20.0, Y);
		if (Index < Filled)
		{
			const abyss::GemInstance& Gem = Equipped->sockets[static_cast<size_t>(Index)];
			FAbyssSlotItem GemEntry;
			abyss::ItemInstance GemItem;
			GemItem.baseId = Gem.gemId;
			GemItem.quality = abyss::ItemQuality::Magic;
			GemEntry.Item = GemItem;
			GemEntry.bCompare = false;
			Body->AddSlot().Position(RowPos).Size(FVector2D(34.0, 34.0))[SNew(SAbyssItemSlot, Context).SlotSize(34.f).Entry(GemEntry)];
			Body->AddSlot().Position(RowPos + FVector2D(42.0, 2.0)).Size(FVector2D(240.0, 32.0))
			[
				AbyssUi::Label(*Ctx, FText::AsCultureInvariant(FString::Printf(TEXT("T%d  %s (%s %s)"), Gem.tier, *Ctx->ItemBaseName(Gem.gemId),
					*Ctx->StatValue(Gem.stat, Gem.value), *Ctx->StatLabel(Gem.stat))), 12.f, C.Text, false, 1, 236.f)
			];
			Body->AddSlot().Position(RowPos + FVector2D(290.0, 4.0)).Size(FVector2D(70.0, 26.0))
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("ui.socket.remove", TEXT("Remove")))
				.Kind(EAbyssButtonKind::Secondary).FontPx(11.f).Width(70.f).Height(26.f)
				.IsEnabled(!bDying)
				.OnClicked_Lambda([Context, SlotId, Index]() { Context->Submit(abyss::CmdUnsocketGem{ SlotId, Index }); })
			];
		}
		else
		{
			Body->AddSlot().Position(RowPos).Size(FVector2D(340.0, 34.0))
			[
				SNew(SAbyssCanvas, Context, [Context](FAbyssPainter& P, const FVector2D& Size)
				{
					P.Well(FVector2D::ZeroVector, FVector2D(34.0, 34.0));
					P.Diamond(FVector2D(17.0, 17.0), 12.f, FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0x8be9fd), 0.4f));
					P.Text(FVector2D(44.0, 9.0), Context->LocOrStr("ui.socket.emptySlot", TEXT("Empty socket")), Context->Style().Body(12.f, false, 1),
						Context->Style().Colors().Dim);
				})
			];
		}
		Y += 40.0;
	}

	// gems in the bag (7 per row): click sockets when a socket is free
	Y = FMath::Max(Y + 6.0, 230.0);
	Body->AddSlot().Position(FVector2D(20.0, Y)).Size(FVector2D(360.0, 18.0))
	[
		AbyssUi::SectionHeader(Context, Ctx->LocOr("ui.socket.gemsInBag", TEXT("Gems in bag")), 360.f)
	];
	Y += 24.0;
	int32 GemCount = 0;
	const bool bFree = Filled < Capacity;
	for (const abyss::ItemInstance& Item : Snap.inventory->Bag())
	{
		const abyss::ItemBaseDef* Base = Data->FindItemBase(Item.baseId);
		if (Base == nullptr || !Base->isGem)
		{
			continue;
		}
		const int32 Column = GemCount % 7;
		const int32 Row = GemCount / 7;
		FAbyssSlotItem GemEntry;
		GemEntry.Item = Item;
		GemEntry.bCompare = false;
		GemEntry.bDimmed = !bFree || bDying;
		const std::string Uid = Item.uid;
		Body->AddSlot().Position(FVector2D(20.0 + Column * 50.0, Y + Row * 50.0)).Size(FVector2D(44.0, 44.0))
		[
			SNew(SAbyssItemSlot, Context)
			.SlotSize(44.f)
			.Entry(GemEntry)
			.OnClicked_Lambda([Context, SlotId, Uid, bFree, bDying](const FVector2D&)
			{
				if (bFree && !bDying)
				{
					Context->Submit(abyss::CmdSocketGem{ SlotId, Uid });
				}
			})
		];
		++GemCount;
		if (GemCount >= 14)
		{
			break;
		}
	}
	if (GemCount == 0)
	{
		Body->AddSlot().Position(FVector2D(20.0, Y)).Size(FVector2D(360.0, 20.0))
		[
			AbyssUi::Label(*Ctx, Ctx->LocOr("ui.socket.noGems", TEXT("No gems")), 12.f, C.Dim)
		];
	}
	Body->AddSlot().Position(FVector2D(130.0, 374.0)).Size(FVector2D(140.0, 30.0))
	[
		SNew(SAbyssButton, Context)
		.Text(Ctx->LocOr("ui.socket.unequip", TEXT("Unequip")))
		.Kind(EAbyssButtonKind::Danger).FontPx(12.f).Width(140.f).Height(30.f)
		.IsEnabled(!bDying)
		.OnClicked_Lambda([this, Context, SlotId]()
		{
			Context->Submit(abyss::CmdUnequip{ SlotId });
			RequestClose();
		})
	];
}
