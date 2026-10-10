#include "UI/Panels/SAbyssShopPanel.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include <span>
#include <string>
#include <vector>

#include "abyss/data/DataStore.h"
#include "abyss/items/Crafting.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/Shop.h"

#include "Framework/AbyssText.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	constexpr int32 GAbyssShopBagPerPage = 40;
	constexpr abyss::CraftAction GAbyssShopActions[4] = {
		abyss::CraftAction::Salvage, abyss::CraftAction::Reforge, abyss::CraftAction::Upgrade, abyss::CraftAction::Socket,
	};

	/** Touch: the item card beside one explicit action button (save-ui-input 7.0.6 / 7.7). */
	TSharedRef<SWidget> AbyssShop_ActionPopup(const TSharedRef<FAbyssUiContext>& Ctx, const abyss::ItemInstance& Item, const FText& Label,
		EAbyssButtonKind Kind, bool bEnabled, TFunction<void()> Action)
	{
		AbyssItemTooltip::FOptions Options;
		Options.bCompare = true;
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
			[
				AbyssItemTooltip::MakeCard(Ctx, Item, Options)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(8.f, 0.f, 0.f, 0.f))
			[
				SNew(SAbyssCardFrame, Ctx)
				.Padding(FMargin(6.f))
				[
					SNew(SAbyssButton, Ctx)
					.Text(Label)
					.Kind(Kind)
					.FontPx(18.f)
					.Width(180.f)
					.Height(54.f)
					.IsEnabled(bEnabled)
					.OnClicked_Lambda([Ctx, Action]()
					{
						Ctx->ClosePopup();
						if (Action)
						{
							Action();
						}
					})
				]
			];
	}

	/** "{name}x{qty}" list of material yields / costs. */
	FString AbyssShop_MaterialList(const FAbyssUiContext& Ctx, const std::vector<abyss::MaterialYield>& Yields)
	{
		TArray<FString> Parts;
		for (const abyss::MaterialYield& Yield : Yields)
		{
			Parts.Add(Ctx.LocArgsOrStr("ui.forge.qty", TEXT("{name}\x00D7{qty}"),
				{ FAbyssUiContext::Arg("name", Ctx.ItemBaseName(Yield.baseId)), FAbyssUiContext::Arg("qty", Yield.quantity) }));
		}
		return FString::Join(Parts, TEXT(", "));
	}
}

void SAbyssShopPanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const std::string& InNpcId,
	abyss::PanelId InPanel)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	NpcId = InNpcId;
	Panel = InPanel;
	bBlacksmith = InPanel == abyss::PanelId::Forge;
	const TSharedRef<FAbyssUiContext> Context = InContext;
	SetPanelContent(
		SNew(SAbyssPanelFrame, Context)
		.Size(GetDesignSize())
		.Title_Lambda([this, Context]()
		{
			return bBlacksmith ? Context->LocOr("ui.shop.blacksmith", TEXT("Blacksmith")) : Context->LocOr("ui.shop.shop", TEXT("Shop"));
		})
		.ContentPadding(FMargin(0.f))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SAssignNew(Body, SCanvas)
		]);
}

bool SAbyssShopPanel::HandleBack()
{
	if (!AnvilUid.empty())
	{
		AnvilUid.clear();  // Esc first takes the item off the anvil
		MarkDirty();
		return true;
	}
	return false;
}

void SAbyssShopPanel::Refresh(const abyss::Snapshot& Snap)
{
	if (WareScroll.IsValid())
	{
		SavedWareScroll = WareScroll->GetScrollOffset();
	}
	Body->ClearChildren();
	WareScroll.Reset();
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr || Snap.shop == nullptr || Snap.inventory == nullptr)
	{
		return;
	}
	bBlacksmith = Snap.shop->blacksmith || Panel == abyss::PanelId::Forge;
	if (!bBlacksmith)
	{
		Tab = 0;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();

	// left header: tabs (blacksmith) or the wares title
	if (bBlacksmith)
	{
		TArray<SAbyssTabBar::FTab> Tabs;
		SAbyssTabBar::FTab Wares;
		Wares.Label = Ctx->LocOr("ui.shop.tabBuy", TEXT("Wares"));
		Wares.Accent = C.Gold;
		SAbyssTabBar::FTab Forge;
		Forge.Label = Ctx->LocOr("ui.shop.tabForge", TEXT("Forge"));
		Forge.Accent = FAbyssUiStyle::Rgb(0xff8a2a);
		Tabs.Add(Wares);
		Tabs.Add(Forge);
		Body->AddSlot().Position(FVector2D(16.0, 44.0)).Size(FVector2D(330.0, 28.0))
		[
			SNew(SAbyssTabBar, Context)
			.Tabs(Tabs)
			.TabHeight(28.f)
			.ActiveIndex_Lambda([this]() { return Tab; })
			.OnTabSelected_Lambda([this](int32 Index)
			{
				if (Index != Tab)
				{
					Tab = Index;
					MarkDirty();
				}
			})
		];
	}
	else
	{
		Body->AddSlot().Position(FVector2D(16.0, 48.0)).Size(FVector2D(330.0, 18.0))
		[
			AbyssUi::SectionHeader(Context, Ctx->LocOr("ui.shop.itemList", TEXT("Items for Sale")), 330.f)
		];
	}
	// divider at x 356
	Body->AddSlot().Position(FVector2D(356.0, 44.0)).Size(FVector2D(2.0, 420.0))
	[
		SNew(SAbyssCanvas, Context, [](FAbyssPainter& P, const FVector2D& Size)
		{
			const FLinearColor Gold = P.Style.Colors().Gold;
			P.ColoredQuad(FVector2D::ZeroVector, FVector2D(1.0, Size.Y * 0.5), FAbyssUiStyle::WithAlpha(Gold, 0.f), FAbyssUiStyle::WithAlpha(Gold, 0.f),
				FAbyssUiStyle::WithAlpha(Gold, 0.55f), FAbyssUiStyle::WithAlpha(Gold, 0.55f));
			P.ColoredQuad(FVector2D(0.0, Size.Y * 0.5), FVector2D(1.0, Size.Y * 0.5), FAbyssUiStyle::WithAlpha(Gold, 0.55f),
				FAbyssUiStyle::WithAlpha(Gold, 0.55f), FAbyssUiStyle::WithAlpha(Gold, 0.f), FAbyssUiStyle::WithAlpha(Gold, 0.f));
		})
	];
	if (Tab == 1)
	{
		BuildForge(Snap);
	}
	else
	{
		BuildWares(Snap);
	}
	BuildBag(Snap);

	// gold line + hint
	Body->AddSlot().Position(FVector2D(16.0, 448.0)).Size(FVector2D(330.0, 22.0))
	[
		AbyssUi::Label(*Ctx, Ctx->LocArgsOr("ui.shop.gold", TEXT("Gold: {gold}G"), { FAbyssUiContext::Arg("gold", Snap.hero.gold) }), 13.f,
			FAbyssUiStyle::Rgb(0xffd35a), true, 1)
	];
	const bool bTouch = Ctx->IsTouch();
	const FText Hint = Tab == 1
		? (bTouch ? Ctx->LocOr("ui.forge.bagHintTouch", TEXT("Tap gear to place it on the anvil")) : Ctx->LocOr("ui.forge.bagHint", TEXT("Click gear to place it")))
		: (bTouch ? Ctx->LocOr("ui.shop.sellHintTouch", TEXT("Tap an item, then tap Sell")) : Ctx->LocOr("ui.shop.sellHint", TEXT("Right-click to quick sell")));
	Body->AddSlot().Position(FVector2D(370.0, 450.0)).Size(FVector2D(394.0, 20.0))
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		[
			AbyssUi::Label(*Ctx, Hint, 11.f, C.Muted, false, 1)
		]
	];
}

// =====================================================================================================================
// Wares + buyback (12.3 / 12.5)
// =====================================================================================================================

void SAbyssShopPanel::BuildWares(const abyss::Snapshot& Snap)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const abyss::ShopState& Shop = *Snap.shop;
	const int64 Gold = Snap.hero.gold;
	const bool bDying = Ctx->IsHeroDying();
	const bool bTouch = Ctx->IsTouch();

	SAssignNew(WareScroll, SScrollBox)
		.ScrollBarStyle(&Ctx->Style().ScrollBar())
		.ScrollBarThickness(FVector2D(6.0, 6.0))
		.ConsumeMouseWheel(EConsumeMouseWheel::Always);
	for (int32 Index = 0; Index < static_cast<int32>(Shop.wares.size()); ++Index)
	{
		const abyss::ShopWare& Ware = Shop.wares[static_cast<size_t>(Index)];
		const bool bAffordable = Gold >= Ware.price;
		abyss::ItemInstance Preview;
		Preview.baseId = Ware.baseId;
		Preview.quality = abyss::ItemQuality::Normal;
		Preview.level = Snap.hero.level;
		FAbyssSlotItem SlotEntry;
		SlotEntry.Item = Preview;
		SlotEntry.bCompare = true;
		SlotEntry.TooltipFooter = Ctx->LocArgsOr("ui.shop.buyPrice", TEXT("Price: {price}G"), { FAbyssUiContext::Arg("price", Ware.price) });
		WareScroll->AddSlot()
			.Padding(FMargin(0.f, 0.f, 8.f, 4.f))
			[
				SNew(SBox)
				.HeightOverride(bTouch ? 52.f : 40.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SAbyssCanvas, Context, [](FAbyssPainter& P, const FVector2D& Size)
						{
							P.Card(FVector2D::ZeroVector, Size, P.Style.Colors().Card, FAbyssUiStyle::Rgb(0x3a343f));
						})
					]
					+ SOverlay::Slot()
					.Padding(FMargin(4.f, 3.f, 6.f, 3.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SAbyssItemSlot, Context).SlotSize(32.f).Entry(SlotEntry)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(8.f, 0.f))
						[
							AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Ctx->ItemBaseName(Ware.baseId)), 12.f, bAffordable ? C.Text : C.Dim, false, 1, 150.f)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
						[
							AbyssUi::Label(*Ctx, FText::AsCultureInvariant(FString::Printf(TEXT("%lldG"), static_cast<long long>(Ware.price))), 12.f,
								bAffordable ? FAbyssUiStyle::Rgb(0xffd35a) : C.Bad, true, 1)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SAbyssButton, Context)
							.Text(Ctx->LocOr("ui.shop.buy", TEXT("Buy")))
							.Kind(EAbyssButtonKind::Success)
							.FontPx(bTouch ? 14.f : 11.f)
							.Width(bTouch ? 70.f : 52.f)
							.Height(bTouch ? 40.f : 24.f)
							.IsEnabled(bAffordable && !bDying)
							.OnClicked_Lambda([Context, Index]() { Context->Submit(abyss::CmdBuy{ Index }); })
						]
					]
				]
			];
	}

	// buyback (session list shared by every shop; FIFO 5)
	const std::span<const abyss::BuybackEntry> Buyback = Snap.inventory->BuybackList();
	if (!Buyback.empty())
	{
		WareScroll->AddSlot()
			.Padding(FMargin(0.f, 8.f, 8.f, 4.f))
			[
				AbyssUi::SectionHeader(Context, Ctx->LocOr("ui.shop.buyback", TEXT("Buyback")), 316.f)
			];
		for (int32 Index = 0; Index < static_cast<int32>(Buyback.size()); ++Index)
		{
			const abyss::BuybackEntry& Entry = Buyback[static_cast<size_t>(Index)];
			const bool bAffordable = Gold >= Entry.price;
			FAbyssSlotItem SlotEntry;
			SlotEntry.Item = Entry.item;
			SlotEntry.bCompare = true;
			WareScroll->AddSlot()
				.Padding(FMargin(0.f, 0.f, 8.f, 4.f))
				[
					SNew(SBox)
					.HeightOverride(bTouch ? 52.f : 40.f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SAbyssCanvas, Context, [](FAbyssPainter& P, const FVector2D& Size)
							{
								P.Card(FVector2D::ZeroVector, Size, P.Style.Colors().Card, FAbyssUiStyle::Rgb(0x3a343f));
							})
						]
						+ SOverlay::Slot()
						.Padding(FMargin(4.f, 3.f, 6.f, 3.f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(SAbyssItemSlot, Context).SlotSize(32.f).Entry(SlotEntry)
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(8.f, 0.f))
							[
								AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Ctx->ItemName(Entry.item)), 12.f, Ctx->Style().QualityColor(Entry.item.quality), false, 1, 140.f)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
							[
								AbyssUi::Label(*Ctx, FText::AsCultureInvariant(FString::Printf(TEXT("%lldG"), static_cast<long long>(Entry.price))), 12.f,
									bAffordable ? FAbyssUiStyle::Rgb(0xffd35a) : C.Bad, true, 1)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(SAbyssButton, Context)
								.Text(Ctx->LocOr("ui.shop.buybackBtn", TEXT("Buyback")))
								.Kind(EAbyssButtonKind::Secondary)
								.FontPx(bTouch ? 13.f : 11.f)
								.Width(bTouch ? 80.f : 62.f)
								.Height(bTouch ? 40.f : 24.f)
								.IsEnabled(bAffordable && !bDying && !Snap.inventory->IsFull())
								.OnClicked_Lambda([Context, Index]() { Context->Submit(abyss::CmdBuyback{ Index }); })
							]
						]
					]
				];
		}
	}
	Body->AddSlot().Position(FVector2D(16.0, bBlacksmith ? 80.0 : 72.0)).Size(FVector2D(334.0, bBlacksmith ? 362.0 : 370.0))
	[
		WareScroll.ToSharedRef()
	];
	WareScroll->SetScrollOffset(SavedWareScroll);
}

// =====================================================================================================================
// Forge (13.5)
// =====================================================================================================================

void SAbyssShopPanel::BuildForge(const abyss::Snapshot& Snap)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const abyss::DataStore& Data = *Ctx->GetData();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const abyss::Inventory& Inventory = *Snap.inventory;
	const bool bDying = Ctx->IsHeroDying();
	const bool bTouch = Ctx->IsTouch();
	const abyss::ItemInstance* Anvil = AnvilUid.empty() ? nullptr : Inventory.FindInBag(AnvilUid);
	if (Anvil == nullptr)
	{
		AnvilUid.clear();
	}

	// anvil card
	if (Anvil == nullptr)
	{
		Body->AddSlot().Position(FVector2D(16.0, 82.0)).Size(FVector2D(330.0, 84.0))
		[
			SNew(SAbyssCanvas, Context, [Context](FAbyssPainter& P, const FVector2D& Size)
			{
				P.Card(FVector2D::ZeroVector, Size, P.Style.Colors().Card, FAbyssUiStyle::Rgb(0x3a343f));
				P.Well(FVector2D(12.0, 18.0), FVector2D(48.0, 48.0), 5.f);
				P.Text(FVector2D(72.0, 22.0), Context->LocOrStr("ui.forge.slotEmpty", TEXT("Place gear")), P.Style.Body(13.f, true, 1), P.Style.Colors().Dim);
			})
			.Padding(FMargin(72.f, 44.f, 8.f, 4.f))
			[
				AbyssUi::Label(*Ctx, Ctx->LocOr("ui.forge.pickHint", TEXT("Click a piece of gear in your bag to put it on the anvil")), 11.f, C.Muted, false, 1, 250.f)
			]
		];
	}
	else
	{
		FAbyssSlotItem SlotEntry;
		SlotEntry.Item = *Anvil;
		SlotEntry.bCompare = false;
		const int32 Capacity = abyss::ItemSocketCapacity(*Anvil, Data);
		TArray<FString> Affixes;
		for (const abyss::ItemAffix& Affix : Anvil->affixes)
		{
			Affixes.Add(FString::Printf(TEXT("%s %s"), *Ctx->StatValue(Affix.stat, Affix.value), *Ctx->StatLabel(Affix.stat)));
		}
		const FString AffixText = Affixes.Num() > 0 ? FString::Join(Affixes, TEXT("  ")) : Ctx->LocOrStr("ui.forge.noAffixes", TEXT("No affixes"));
		const abyss::ItemInstance AnvilCopy = *Anvil;
		Body->AddSlot().Position(FVector2D(16.0, 82.0)).Size(FVector2D(330.0, 84.0))
		[
			SNew(SAbyssCanvas, Context, [](FAbyssPainter& P, const FVector2D& Size)
			{
				P.Card(FVector2D::ZeroVector, Size, P.Style.Colors().Card, FAbyssUiStyle::Rgb(0xff8a2a), FAbyssUiStyle::Rgb(0xff8a2a));
			})
			.Padding(FMargin(10.f, 8.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
				[
					SNew(SAbyssItemSlot, Context)
					.SlotSize(48.f)
					.Entry(SlotEntry)
					.OnClicked_Lambda([this](const FVector2D&)
					{
						AnvilUid.clear();  // clicking it again takes it off the anvil
						MarkDirty();
					})
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Ctx->ItemName(AnvilCopy)), 13.f, Ctx->Style().QualityColor(AnvilCopy.quality), true, 1, 250.f)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						AbyssUi::Label(*Ctx, FText::AsCultureInvariant(FString::Printf(TEXT("%s   %s"),
							*Ctx->LocArgsOrStr("ui.forge.itemInfo", TEXT("{quality} \x00B7 Lv.{level}"),
								{ FAbyssUiContext::Arg("quality", Ctx->QualityName(AnvilCopy.quality)), FAbyssUiContext::Arg("level", AnvilCopy.level) }),
							*Ctx->LocArgsOrStr("ui.forge.sockets", TEXT("Sockets {filled}/{max}"),
								{ FAbyssUiContext::Arg("filled", static_cast<int64>(AnvilCopy.sockets.size())), FAbyssUiContext::Arg("max", Capacity) }))),
							11.f, C.TextSoft, false, 1)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						AbyssUi::Label(*Ctx, FText::AsCultureInvariant(AffixText), 10.f, FAbyssUiStyle::Rgb(0x7fb0ff), false, 1, 250.f)
					]
				]
			]
		];
	}

	// owned materials
	TArray<FString> Owned;
	for (const std::string& Material : Data.Items().crafting.materials)
	{
		Owned.Add(Ctx->LocArgsOrStr("ui.forge.qty", TEXT("{name}\x00D7{qty}"),
			{ FAbyssUiContext::Arg("name", Ctx->ItemBaseName(Material)), FAbyssUiContext::Arg("qty", Inventory.CountOf(Material)) }));
	}
	Body->AddSlot().Position(FVector2D(16.0, 172.0)).Size(FVector2D(330.0, 20.0))
	[
		AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Ctx->LocOrStr("ui.forge.materials", TEXT("Materials")) + TEXT(":  ") + FString::Join(Owned, TEXT("   "))),
			11.f, C.TextSoft, false, 1, 328.f)
	];

	// one row per action: label, preview, cost chips or block reason, button iff CheckCraft ok
	for (int32 Row = 0; Row < 4; ++Row)
	{
		const abyss::CraftAction Action = GAbyssShopActions[Row];
		const std::string ActionId(abyss::EnumName(Action));
		FString Preview;
		FString Block;
		TArray<TPair<FString, bool>> Chips;  // text, short
		bool bOk = false;
		if (Anvil != nullptr)
		{
			const abyss::CraftCheck Check = abyss::CheckCraft(Data, Action, Anvil->uid, Inventory, Snap.hero.gold);
			bOk = Check.ok && !bDying;
			switch (Action)
			{
			case abyss::CraftAction::Salvage:
			{
				Preview = Ctx->LocArgsOrStr("ui.forge.preview.salvage", TEXT("Get {list}"),
					{ FAbyssUiContext::Arg("list", AbyssShop_MaterialList(*Ctx, abyss::SalvageYield(Data, *Anvil))) });
				if (!Anvil->sockets.empty())
				{
					Preview += TEXT(" \x00B7 ") + Ctx->LocOrStr("ui.forge.preview.salvageGems", TEXT("gems return to bag"));
				}
				break;
			}
			case abyss::CraftAction::Reforge:
			{
				const abyss::CraftingDef& Crafting = Data.Items().crafting;
				const bool bRare = Anvil->quality == abyss::ItemQuality::Rare;
				Preview = Ctx->LocArgsOrStr("ui.forge.preview.reforge", TEXT("Reroll {min}-{max} affixes"),
					{ FAbyssUiContext::Arg("min", bRare ? Crafting.rerollRareMin : Crafting.rerollMagicMin),
						FAbyssUiContext::Arg("max", bRare ? Crafting.rerollRareMax : Crafting.rerollMagicMax) });
				break;
			}
			case abyss::CraftAction::Upgrade:
			{
				const std::optional<abyss::ItemQuality> Next = abyss::CraftUpgradeTarget(Anvil->quality);
				if (Next.has_value())
				{
					Preview = Ctx->LocArgsOrStr("ui.forge.preview.upgrade", TEXT("{from} \x2192 {to} \x00B7 keeps affixes"),
						{ FAbyssUiContext::Arg("from", Ctx->QualityName(Anvil->quality)), FAbyssUiContext::Arg("to", Ctx->QualityName(Next.value())) });
				}
				break;
			}
			case abyss::CraftAction::Socket:
			{
				const int32 Capacity = abyss::ItemSocketCapacity(*Anvil, Data);
				Preview = Ctx->LocArgsOrStr("ui.forge.preview.socket", TEXT("Sockets {from} \x2192 {to}"),
					{ FAbyssUiContext::Arg("from", Capacity), FAbyssUiContext::Arg("to", Capacity + 1) });
				break;
			}
			}
			if (Check.cost.applies && Check.reason != abyss::CraftFail::NotEquipment && Check.reason != abyss::CraftFail::Quality
				&& Check.reason != abyss::CraftFail::UnknownBase)
			{
				if (Check.cost.gold > 0)
				{
					Chips.Add(TPair<FString, bool>(FString::Printf(TEXT("%lldG"), static_cast<long long>(Check.cost.gold)), Snap.hero.gold < Check.cost.gold));
				}
				else if (Check.cost.materials.empty())
				{
					Chips.Add(TPair<FString, bool>(Ctx->LocOrStr("ui.forge.free", TEXT("Free")), false));
				}
				for (const abyss::CraftMaterialCost& Material : Check.cost.materials)
				{
					Chips.Add(TPair<FString, bool>(Ctx->LocArgsOrStr("ui.forge.qty", TEXT("{name}\x00D7{qty}"),
						{ FAbyssUiContext::Arg("name", Ctx->ItemBaseName(Material.itemId)), FAbyssUiContext::Arg("qty", Material.count) }),
						Inventory.CountOf(Material.itemId) < Material.count));
				}
			}
			if (!Check.ok)
			{
				std::string Reason(abyss::EnumName(Check.reason));
				if (Action == abyss::CraftAction::Upgrade && Check.reason == abyss::CraftFail::Quality)
				{
					Reason = "upgradeQuality";
				}
				Block = Ctx->NameOr("ui.forge.block." + Reason, Reason);
			}
		}
		FString ChipText;
		bool bAnyShort = false;
		for (const TPair<FString, bool>& Chip : Chips)
		{
			ChipText += (ChipText.IsEmpty() ? TEXT("") : TEXT("  ")) + Chip.Key;
			bAnyShort = bAnyShort || Chip.Value;
		}
		const double Y = 198.0 + Row * 60.0;
		const abyss::ItemInstance AnvilItem = Anvil != nullptr ? *Anvil : abyss::ItemInstance();
		const bool bHasItem = Anvil != nullptr;
		Body->AddSlot().Position(FVector2D(16.0, Y)).Size(FVector2D(330.0, 54.0))
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SAbyssCanvas, Context, [bOk](FAbyssPainter& P, const FVector2D& Size)
				{
					P.Card(FVector2D::ZeroVector, Size, P.Style.Colors().Card, bOk ? FAbyssUiStyle::Rgb(0x6fb35a) : FAbyssUiStyle::Rgb(0x3a343f));
				})
			]
			+ SOverlay::Slot()
			.Padding(FMargin(10.f, 4.f, 8.f, 4.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						AbyssUi::Label(*Ctx, Ctx->LocOr("ui.forge.action." + ActionId, *AbyssText::ToFString(ActionId)), 13.f, C.Parchment, true, 1)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Preview), 10.f, C.TextSoft, false, 1, 220.f)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Block.IsEmpty() ? ChipText : Block), 10.f,
							!Block.IsEmpty() || bAnyShort ? FAbyssUiStyle::Rgb(0xff6b5a) : FAbyssUiStyle::Rgb(0xffd35a), true, 1, 220.f)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SAbyssButton, Context)
					.Text(Ctx->LocOr("ui.forge.action." + ActionId, *AbyssText::ToFString(ActionId)))
					.Kind(Action == abyss::CraftAction::Salvage ? EAbyssButtonKind::Danger : EAbyssButtonKind::Primary)
					.FontPx(bTouch ? 14.f : 12.f)
					.Width(bTouch ? 86.f : 70.f)
					.Height(bTouch ? 44.f : 28.f)
					.IsEnabled(bOk && bHasItem)
					.OnClicked_Lambda([this, Action, AnvilItem]() { Craft(Action, AnvilItem); })
				]
			]
		];
	}
}

void SAbyssShopPanel::Craft(abyss::CraftAction Action, const abyss::ItemInstance& Item)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const std::string Uid = Item.uid;
	// 13.5: salvaging rare / legendary / set gear or anything with filled sockets asks first
	if (Action == abyss::CraftAction::Salvage && (abyss::QualityMeetsFloor(Item.quality, abyss::ItemQuality::Rare) || !Item.sockets.empty()))
	{
		FAbyssConfirmRequest Request;
		Request.Body = Ctx->LocArgsOr("ui.forge.salvageConfirm", TEXT("Salvage {name}?\nThis cannot be undone"),
			{ FAbyssUiContext::Arg("name", Ctx->ItemName(Item)) });
		Request.BodyColor = Ctx->Style().QualityColor(Item.quality);
		Request.ConfirmLabel = Ctx->LocOr("ui.forge.action.salvage", TEXT("Salvage"));
		Request.CancelLabel = Ctx->LocOr("ui.shop.cancel", TEXT("Cancel"));
		Request.bDanger = true;
		Request.OnConfirm = [Context, Uid]() { Context->Submit(abyss::CmdCraft{ abyss::CraftAction::Salvage, Uid }); };
		Ctx->Confirm(MoveTemp(Request));
		return;
	}
	Ctx->Submit(abyss::CmdCraft{ Action, Uid });
}

// =====================================================================================================================
// Bag (sell / anvil)
// =====================================================================================================================

void SAbyssShopPanel::BuildBag(const abyss::Snapshot& Snap)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const abyss::DataStore& Data = *Ctx->GetData();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const std::span<const abyss::ItemInstance> Bag = Snap.inventory->Bag();
	const int32 Total = static_cast<int32>(Bag.size());
	const int32 Pages = FMath::Max(1, (Total + GAbyssShopBagPerPage - 1) / GAbyssShopBagPerPage);
	BagPage = FMath::Clamp(BagPage, 0, Pages - 1);
	const bool bForge = Tab == 1;

	Body->AddSlot().Position(FVector2D(370.0, 48.0)).Size(FVector2D(394.0, 18.0))
	[
		AbyssUi::SectionHeader(Context, FText::AsCultureInvariant(FString::Printf(TEXT("%s (%d/%d)"),
			*Ctx->LocOrStr("ui.shop.yourBag", TEXT("Your Inventory")), Total, Snap.inventory->Capacity())), 394.f)
	];
	constexpr double Cell = 44.0;
	constexpr double Gap = 5.0;
	for (int32 CellIndex = 0; CellIndex < GAbyssShopBagPerPage; ++CellIndex)
	{
		const int32 Column = CellIndex % 8;
		const int32 Row = CellIndex / 8;
		const int32 Index = BagPage * GAbyssShopBagPerPage + CellIndex;
		FAbyssSlotItem Entry;
		if (Index < Total)
		{
			const abyss::ItemInstance& Item = Bag[static_cast<size_t>(Index)];
			Entry.Item = Item;
			Entry.bSelected = bForge && Item.uid == AnvilUid;
			Entry.bDimmed = bForge && !abyss::IsEquipmentItem(Item, Data);
		}
		else
		{
			Entry.bDimmed = true;
		}
		const TOptional<abyss::ItemInstance> ItemCopy = Entry.Item;
		Body->AddSlot().Position(FVector2D(370.0 + Column * (Cell + Gap), 72.0 + Row * (Cell + Gap))).Size(FVector2D(Cell, Cell))
		[
			SNew(SAbyssItemSlot, Context)
			.SlotSize(static_cast<float>(Cell))
			.Entry(Entry)
			.OnClicked_Lambda([this, ItemCopy](const FVector2D& Position)
			{
				if (ItemCopy.IsSet())
				{
					OnBagItem(ItemCopy.GetValue(), Position, false);
				}
			})
			.OnRightClicked_Lambda([this, ItemCopy](const FVector2D& Position)
			{
				if (ItemCopy.IsSet())
				{
					OnBagItem(ItemCopy.GetValue(), Position, true);
				}
			})
		];
	}
	if (Pages > 1)
	{
		Body->AddSlot().Position(FVector2D(370.0, 322.0)).Size(FVector2D(394.0, 26.0))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("ui.shop.prevPage", TEXT("< Prev")))
				.Kind(EAbyssButtonKind::Ghost).FontPx(11.f).Width(70.f).Height(24.f)
				.IsEnabled(BagPage > 0)
				.OnClicked_Lambda([this]()
				{
					--BagPage;
					MarkDirty();
				})
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				AbyssUi::Label(*Ctx, FText::AsCultureInvariant(FString::Printf(TEXT("%d/%d"), BagPage + 1, Pages)), 11.f, C.TextSoft)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("ui.shop.nextPage", TEXT("Next >")))
				.Kind(EAbyssButtonKind::Ghost).FontPx(11.f).Width(70.f).Height(24.f)
				.IsEnabled(BagPage + 1 < Pages)
				.OnClicked_Lambda([this]()
				{
					++BagPage;
					MarkDirty();
				})
			]
		];
	}
}

void SAbyssShopPanel::OnBagItem(const abyss::ItemInstance& Item, const FVector2D& AbsolutePosition, bool bRightClick)
{
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr || Ctx->IsHeroDying())
	{
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const bool bTouch = Ctx->IsTouch();
	const bool bForge = Tab == 1;
	if (bForge && !bRightClick)
	{
		const bool bGear = abyss::IsEquipmentItem(Item, *Data);
		const auto Place = [this, Context, Item, bGear]()
		{
			if (!bGear)
			{
				// 13.5: non-gear -> error sound + local log line
				Context->PlaySound(abyss::SfxId::Error);
				if (IAbyssUiHost* Host = Context->GetHost())
				{
					Host->AddLocalLog(Context->LocOrStr("ui.forge.notEquipment", TEXT("Only gear can go on the anvil")), abyss::LogType::System);
				}
				Context->Toast(Context->LocOr("ui.forge.notEquipment", TEXT("Only gear can go on the anvil")), Context->Style().Colors().Bad);
				return;
			}
			AnvilUid = AnvilUid == Item.uid ? std::string() : Item.uid;
			MarkDirty();
		};
		if (bTouch)
		{
			Ctx->ShowPopup(AbyssShop_ActionPopup(Context, Item, Ctx->LocOr("ui.forge.place", TEXT("Place on anvil")), EAbyssButtonKind::Primary, bGear, Place),
				AbsolutePosition);
		}
		else
		{
			Place();
		}
		return;
	}
	// sell (normal / magic / rare at once; legendary / set confirm)
	if (bTouch)
	{
		const int64 Price = abyss::ItemSellPrice(Item, *Data);
		Ctx->ShowPopup(AbyssShop_ActionPopup(Context, Item,
			Ctx->LocArgsOr("ui.shop.sellAction", TEXT("Sell {price}G"), { FAbyssUiContext::Arg("price", Price) }), EAbyssButtonKind::Success, true,
			[this, Item]() { Sell(Item); }), AbsolutePosition);
		return;
	}
	Sell(Item);
}

void SAbyssShopPanel::Sell(const abyss::ItemInstance& Item)
{
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr)
	{
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const std::string Uid = Item.uid;
	if (Item.uid == AnvilUid)
	{
		AnvilUid.clear();
	}
	if (Item.quality == abyss::ItemQuality::Legendary || Item.quality == abyss::ItemQuality::Set)
	{
		FAbyssConfirmRequest Request;
		Request.Body = Ctx->LocArgsOr("ui.shop.sellConfirm", TEXT("Sell {name} ({price}G)?"),
			{ FAbyssUiContext::Arg("name", Ctx->ItemName(Item)), FAbyssUiContext::Arg("price", abyss::ItemSellPrice(Item, *Data)) });
		Request.BodyColor = Ctx->Style().QualityColor(Item.quality);
		Request.ConfirmLabel = Ctx->LocOr("ui.shop.confirm", TEXT("Confirm"));
		Request.CancelLabel = Ctx->LocOr("ui.shop.cancel", TEXT("Cancel"));
		Request.bDanger = true;
		Request.OnConfirm = [Context, Uid]() { Context->Submit(abyss::CmdSell{ Uid }); };
		Ctx->Confirm(MoveTemp(Request));
		return;
	}
	Ctx->Submit(abyss::CmdSell{ Uid });
}
