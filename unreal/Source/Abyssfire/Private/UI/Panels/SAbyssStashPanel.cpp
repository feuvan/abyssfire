#include "UI/Panels/SAbyssStashPanel.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Text/STextBlock.h"

#include <span>

#include "abyss/data/AudioData.h"
#include "abyss/items/Inventory.h"
#include "abyss/sim/Commands.h"

#include "UI/AbyssUiStyle.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	constexpr int32 GAbyssStashColumns = 8;
	constexpr int32 GAbyssStashRows = 5;
	constexpr int32 GAbyssStashPerPage = GAbyssStashColumns * GAbyssStashRows;
	constexpr double GAbyssStashGap = 5.0;
	constexpr double GAbyssStashGridTop = 72.0;
	constexpr double GAbyssStashDivider = 420.0;

	/** Touch: the item card beside the Take / Store button (save-ui-input 7.8). */
	TSharedRef<SWidget> AbyssStash_ActionPopup(const TSharedRef<FAbyssUiContext>& Ctx, const abyss::ItemInstance& Item, const FText& Label,
		TFunction<void()> Action)
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
					.Kind(EAbyssButtonKind::Primary)
					.FontPx(18.f)
					.Width(180.f)
					.Height(54.f)
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
}

void SAbyssStashPanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const std::string& InNpcId)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	NpcId = InNpcId;
	const TSharedRef<FAbyssUiContext> Context = InContext;
	SetPanelContent(
		SNew(SAbyssPanelFrame, Context)
		.Size(GetDesignSize())
		.Title(Context->LocOr("ui.stash.title", TEXT("Stash")))
		.ContentPadding(FMargin(0.f))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SAssignNew(Body, SCanvas)
		]);
}

void SAbyssStashPanel::Refresh(const abyss::Snapshot& Snap)
{
	Body->ClearChildren();
	if (Snap.inventory == nullptr || Ctx->GetData() == nullptr)
	{
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();

	// vertical divider between the stash and the bag
	Body->AddSlot().Position(FVector2D(GAbyssStashDivider, 46.0)).Size(FVector2D(2.0, 380.0))
	[
		SNew(SAbyssCanvas, Context, [](FAbyssPainter& P, const FVector2D& Size)
		{
			const FLinearColor Gold = P.Style.Colors().Gold;
			const FLinearColor Clear = FAbyssUiStyle::WithAlpha(Gold, 0.f);
			const FLinearColor Mid = FAbyssUiStyle::WithAlpha(Gold, 0.55f);
			P.ColoredQuad(FVector2D::ZeroVector, FVector2D(1.0, Size.Y * 0.5), Clear, Clear, Mid, Mid);
			P.ColoredQuad(FVector2D(0.0, Size.Y * 0.5), FVector2D(1.0, Size.Y * 0.5), Mid, Mid, Clear, Clear);
		})
	];

	BuildGrid(Snap, true);
	BuildGrid(Snap, false);

	// bottom rule + hint
	Body->AddSlot().Position(FVector2D(18.0, 434.0)).Size(FVector2D(784.0, 12.0))
	[
		AbyssUi::Divider(Context, 784.f)
	];
	const FText Hint = Ctx->IsTouch()
		? Ctx->LocOr("ui.stash.hintTouch", TEXT("Tap an item to inspect, then store or take it"))
		: Ctx->LocOr("ui.stash.hint", TEXT("Click an item to move it between inventory and stash"));
	Body->AddSlot().Position(FVector2D(18.0, 448.0)).Size(FVector2D(784.0, 20.0))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			AbyssUi::Label(*Ctx, Hint, 11.f, C.Muted, false, 1)
		]
	];
}

void SAbyssStashPanel::BuildGrid(const abyss::Snapshot& Snap, bool bStash)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const std::span<const abyss::ItemInstance> Items = bStash ? Snap.inventory->Stash() : Snap.inventory->Bag();
	const int32 Count = static_cast<int32>(Items.size());
	const int32 Capacity = bStash ? FMath::Max(Snap.stashCapacity, 0) : Snap.inventory->Capacity();
	// The stash shows max(capacity, stored) cells (overflow from quest turn-ins can exceed the capacity); the bag shows
	// max(one page, held) like the web.
	const int32 SlotsTotal = bStash ? FMath::Max(Capacity, Count) : FMath::Max(GAbyssStashPerPage, Count);
	const int32 Pages = FMath::Max(1, (SlotsTotal + GAbyssStashPerPage - 1) / GAbyssStashPerPage);
	int32& Page = bStash ? StashPage : BagPage;
	Page = FMath::Clamp(Page, 0, Pages - 1);

	const double X = bStash ? 18.0 : GAbyssStashDivider + 16.0;
	const double W = bStash ? GAbyssStashDivider - 34.0 : 820.0 - (GAbyssStashDivider + 16.0) - 18.0;
	const double Cell = FMath::FloorToDouble((W - GAbyssStashGap * (GAbyssStashColumns - 1)) / GAbyssStashColumns);

	// header: "Stash (n/cap)" / "Inventory (n)" + Sort
	const FText Header = bStash
		? Ctx->LocArgsOr("ui.stash.stored", TEXT("Stash ({count}/{cap})"),
			{ FAbyssUiContext::Arg("count", static_cast<int64>(Count)), FAbyssUiContext::Arg("cap", static_cast<int64>(Capacity)) })
		: Ctx->LocArgsOr("ui.stash.bag", TEXT("Inventory ({count})"), { FAbyssUiContext::Arg("count", static_cast<int64>(Count)) });
	Body->AddSlot().Position(FVector2D(X, 48.0)).Size(FVector2D(W - 70.0, 18.0))
	[
		AbyssUi::SectionHeader(Context, Header, static_cast<float>(W - 70.0))
	];
	Body->AddSlot().Position(FVector2D(X + W - 58.0, 46.0)).Size(FVector2D(56.0, 22.0))
	[
		SNew(SAbyssButton, Context)
		.Text(Ctx->LocOr("ui.stash.sort", TEXT("Sort")))
		.Kind(EAbyssButtonKind::Secondary)
		.FontPx(11.f)
		.Width(56.f)
		.Height(22.f)
		.IsEnabled(Count > 1 && !Ctx->IsHeroDying())
		.OnClicked_Lambda([this, bStash]()
		{
			if (bStash)
			{
				Ctx->Submit(abyss::CmdSortStash{});
			}
			else
			{
				Ctx->Submit(abyss::CmdSortBag{});
			}
			MarkDirty();
		})
	];

	for (int32 CellIndex = 0; CellIndex < GAbyssStashPerPage; ++CellIndex)
	{
		const int32 Column = CellIndex % GAbyssStashColumns;
		const int32 Row = CellIndex / GAbyssStashColumns;
		const int32 Index = Page * GAbyssStashPerPage + CellIndex;
		FAbyssSlotItem Entry;
		if (Index < Count)
		{
			Entry.Item = Items[static_cast<size_t>(Index)];
		}
		else if (Index < SlotsTotal)
		{
			Entry.bDimmed = true;  // free cell
		}
		else
		{
			Entry.bLocked = true;  // past the capacity
		}
		const TOptional<abyss::ItemInstance> ItemCopy = Entry.Item;
		Body->AddSlot()
			.Position(FVector2D(X + Column * (Cell + GAbyssStashGap), GAbyssStashGridTop + Row * (Cell + GAbyssStashGap)))
			.Size(FVector2D(Cell, Cell))
		[
			SNew(SAbyssItemSlot, Context)
			.SlotSize(static_cast<float>(Cell))
			.Entry(Entry)
			.OnClicked_Lambda([this, ItemCopy, bStash](const FVector2D& Position)
			{
				if (ItemCopy.IsSet())
				{
					OnItem(ItemCopy.GetValue(), Position, bStash);
				}
			})
			.OnRightClicked_Lambda([this, ItemCopy, bStash](const FVector2D& Position)
			{
				if (ItemCopy.IsSet())
				{
					OnItem(ItemCopy.GetValue(), Position, bStash);
				}
			})
		];
	}

	if (Pages > 1)
	{
		const double PagerY = GAbyssStashGridTop + GAbyssStashRows * (Cell + GAbyssStashGap);
		Body->AddSlot().Position(FVector2D(X + W * 0.5 - 102.0, PagerY)).Size(FVector2D(204.0, 24.0))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("ui.shop.prevPage", TEXT("< Prev")))
				.Kind(EAbyssButtonKind::Ghost).FontPx(11.f).Width(64.f).Height(24.f)
				.IsEnabled(Page > 0)
				.OnClicked_Lambda([this, bStash]()
				{
					int32& Target = bStash ? StashPage : BagPage;
					--Target;
					MarkDirty();
				})
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				AbyssUi::Label(*Ctx, FText::AsCultureInvariant(FString::Printf(TEXT("%d/%d"), Page + 1, Pages)), 12.f, C.TextSoft)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("ui.shop.nextPage", TEXT("Next >")))
				.Kind(EAbyssButtonKind::Ghost).FontPx(11.f).Width(64.f).Height(24.f)
				.IsEnabled(Page + 1 < Pages)
				.OnClicked_Lambda([this, bStash]()
				{
					int32& Target = bStash ? StashPage : BagPage;
					++Target;
					MarkDirty();
				})
			]
		];
	}
}

void SAbyssStashPanel::OnItem(const abyss::ItemInstance& Item, const FVector2D& AbsolutePosition, bool bStash)
{
	if (Ctx->IsHeroDying())
	{
		return;
	}
	if (!Ctx->IsTouch())
	{
		Move(Item, bStash);
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const TWeakPtr<SAbyssStashPanel> WeakSelf = SharedThis(this);
	const FText Label = bStash ? Ctx->LocOr("ui.stash.withdraw", TEXT("Take")) : Ctx->LocOr("ui.stash.deposit", TEXT("Store"));
	Ctx->HideTooltip(nullptr);
	Ctx->ShowPopup(AbyssStash_ActionPopup(Context, Item, Label, [WeakSelf, Item, bStash]()
	{
		if (const TSharedPtr<SAbyssStashPanel> Self = WeakSelf.Pin())
		{
			Self->Move(Item, bStash);
		}
	}), AbsolutePosition);
}

void SAbyssStashPanel::Move(const abyss::ItemInstance& Item, bool bStash)
{
	const abyss::Snapshot* Snap = Ctx->GetSnapshot();
	if (Snap == nullptr || Snap->inventory == nullptr)
	{
		return;
	}
	// The core refuses a move into a full bag / stash and logs why (ui.stash.bagFull, sys.inventory.stashFull); the UI
	// adds the error cue so the refused click is felt (web: the click sound plays only on success).
	const bool bFits = bStash
		? Snap->inventory->CanAdd(Item)
		: static_cast<int32>(Snap->inventory->Stash().size()) < Snap->stashCapacity;
	Ctx->PlaySound(bFits ? abyss::SfxId::Click : abyss::SfxId::Error);
	if (bStash)
	{
		Ctx->Submit(abyss::CmdStashTake{ Item.uid });
	}
	else
	{
		Ctx->Submit(abyss::CmdStashPut{ Item.uid });
	}
	MarkDirty();
}
