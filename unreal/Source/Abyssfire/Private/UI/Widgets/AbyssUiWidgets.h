// The UiKit widget set (save-ui-input.md 8): painted canvases, buttons, close medallion, input blockers, frames, tabs,
// sliders, item slots and text helpers. Every widget takes the shared FAbyssUiContext (style, text, sounds, overlays).
//
// Input rules (save-ui-input 7.0 / 8.4): buttons are never keyboard-focusable (keys stay with the game viewport and its
// Enhanced Input bindings), fire on press with the mouse (UiKit parity) and on release without drag on touch
// (PreciseTap, safe inside scroll boxes); panel bodies swallow every click so the world never sees it (FIX Q12).
#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Layout/Margin.h"
#include "Templates/Function.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include <string>

#include "abyss/base/Enums.h"
#include "abyss/items/Item.h"
#include "abyss/sim/Snapshot.h"

#include "UI/AbyssUiStyle.h"
#include "UI/Core/AbyssUiContext.h"
#include "UI/Core/AbyssUiDraw.h"

class SButton;
class SScrollBox;
class STextBlock;

namespace abyss
{
	class DataStore;
}

DECLARE_DELEGATE_OneParam(FOnAbyssIndexSelected, int32);
DECLARE_DELEGATE_OneParam(FOnAbyssFloatChanged, float);
DECLARE_DELEGATE_OneParam(FOnAbyssPointerAction, const FVector2D& /*AbsolutePosition*/);

namespace AbyssUi
{
	/** UiKit btnLabel: strips legacy "[...]" / "【...】" wrappers from button labels. */
	FText StripButtonLabel(const FText& Label);
	/** A text block in the UiKit body font. Px = web px. */
	TSharedRef<STextBlock> Label(const FAbyssUiContext& Ctx, const TAttribute<FText>& Text, float Px, const FLinearColor& Color,
		bool bBold = false, int32 OutlinePx = 0, float WrapAt = 0.f);
	TSharedRef<STextBlock> TitleLabel(const FAbyssUiContext& Ctx, const TAttribute<FText>& Text, float Px, const FLinearColor& Color,
		bool bBold = true, int32 OutlinePx = 2);
	/** Styled vertical scroll box (UiKit thumb; drag-scroll on touch). */
	TSharedRef<SScrollBox> ScrollBox(const FAbyssUiContext& Ctx, bool bShowBar = true);
	/**
	 * Section header (UiKit addSectionHeader): bold label in the heading colour, then the gold rule 8 px after it: one
	 * smooth fade (8 segments, alpha 0.7 x (1 - i / 8)) over a 1 px black 0.5 underline 1 px below. FontPx = web px.
	 */
	TSharedRef<SWidget> SectionHeader(const TSharedRef<FAbyssUiContext>& Ctx, const FText& Text, float Width, float FontPx = 12.f);
	/** Paints the section rule from X0 to X1 at Y (shared by SectionHeader and painted headers). */
	void PaintSectionRule(FAbyssPainter& Painter, double X0, double X1, double Y);
	/** Horizontal gold divider (8.5). */
	TSharedRef<SWidget> Divider(const TSharedRef<FAbyssUiContext>& Ctx, float Width, bool bDiamond = true);
	FLinearColor Alpha(const FLinearColor& Color, float A);

	/** A fixed-size painted icon (glyph / quest item / item art); nullptr brush = an empty spacer of the same size. */
	TSharedRef<SWidget> Icon(const TSharedRef<FAbyssUiContext>& Ctx, const FSlateBrush* Brush, float Size,
		const FLinearColor& Tint = FLinearColor::White);

	/** The web skill-icon backdrop (SkillIcons.ts backdropFor): damage-type colours, physical skills by tree. */
	struct FSkillBackdrop
	{
		FLinearColor Mid;
		FLinearColor Deep;
		FLinearColor Rim;
	};
	FSkillBackdrop SkillBackdrop(const abyss::DataStore* Data, const std::string& SkillId);
	/** U11 fallback monogram: the first glyph of a CJK name, else the first two letters. */
	FString SkillMonogram(const FString& Name);
	/**
	 * U11 skill tile (HUD slots, skill-tree cards, the panel hotbar): T_UI_SkillIcon_<id> (the emblem plate, inset 2/96,
	 * corner 15/96) under the Slate frame the manifest asks for (SkillIcons.ts drawSkillIcon): dark iron stroke #17121C
	 * 3.2 u, inner hairline in the backdrop's rim colour at 0.55, top-left bevel rgba(255,240,210,0.35); units are 1/96 of
	 * Size. Tint multiplies icon and frame (unlearned, unaffordable). Missing art: a tile in the backdrop hue with the
	 * monogram (bMonogram = false leaves the letters to the caller). Returns true when the icon art drew.
	 */
	bool PaintSkillTile(FAbyssPainter& Painter, const FAbyssUiContext& Ctx, const std::string& SkillId, const FString& Name,
		const FVector2D& Pos, float Size, const FLinearColor& Tint = FLinearColor::White, bool bMonogram = true);
}

/** A widget painted by a function (under or over its optional content). */
class SAbyssCanvas : public SCompoundWidget
{
public:
	using FPaintFn = TFunction<void(FAbyssPainter& /*Painter*/, const FVector2D& /*LocalSize*/)>;

	SLATE_BEGIN_ARGS(SAbyssCanvas)
		: _Size(FVector2D::ZeroVector)
		, _Padding(FMargin(0.f))
		, _PaintAbove(false)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
		/** Fixed size (0 = the content's size). */
		SLATE_ARGUMENT(FVector2D, Size)
		SLATE_ARGUMENT(FMargin, Padding)
		/** Paint over the content instead of under it. */
		SLATE_ARGUMENT(bool, PaintAbove)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, FPaintFn InPaint);
	void SetPainter(FPaintFn InPaint) { Paint = MoveTemp(InPaint); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TSharedPtr<FAbyssUiContext> Context;
	FPaintFn Paint;
	bool bPaintAbove = false;
};

/** Swallows clicks, wheel and touches (panel bodies, HUD plates); optional callback for backdrops. */
class SAbyssBlocker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssBlocker)
		: _ConsumeWheel(true)
	{
	}
		SLATE_ARGUMENT(bool, ConsumeWheel)
		/** Fired on a press that reached the blocker itself (backdrops close their panel). */
		SLATE_EVENT(FSimpleDelegate, OnPressed)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;

private:
	FSimpleDelegate OnPressed;
	bool bConsumeWheel = true;
};

/**
 * An invisible clickable rect (HUD hit areas over painted elements): left press, right press, hover; swallows the
 * pointer so the world never sees it. Mouse presses fire on press (UiKit parity), touch presses too (web HUD buttons).
 */
class SAbyssHitArea : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssHitArea)
		: _ClickSound(true)
	{
	}
		SLATE_ARGUMENT(bool, ClickSound)
		SLATE_EVENT(FSimpleDelegate, OnPressed)
		SLATE_EVENT(FSimpleDelegate, OnRightPressed)
		SLATE_EVENT(FSimpleDelegate, OnHovered)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	TSharedPtr<FAbyssUiContext> Context;
	FSimpleDelegate OnPressed;
	FSimpleDelegate OnRightPressed;
	FSimpleDelegate OnHovered;
	bool bClickSound = true;
};

/** UiKit button (8.4): painted gradient face per variant and state, label or custom content. */
class SAbyssButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssButton)
		: _Kind(EAbyssButtonKind::Secondary)
		, _FontPx(13.f)
		, _Icon(nullptr)
		, _IconSize(14.f)
		, _Width(0.f)
		, _Height(0.f)
		, _Selected(false)
		, _ClickSound(true)
	{
	}
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_ARGUMENT(EAbyssButtonKind, Kind)
		SLATE_ARGUMENT(float, FontPx)
		/** Optional icon left of the label (a glyph such as T_UI_Glyph_sort); nullptr = the label alone (the fallback). */
		SLATE_ARGUMENT(const FSlateBrush*, Icon)
		SLATE_ARGUMENT(float, IconSize)
		/** Fixed size (0 = content). */
		SLATE_ARGUMENT(float, Width)
		SLATE_ARGUMENT(float, Height)
		/** Draws the selected / active highlight (option rows). */
		SLATE_ATTRIBUTE(bool, Selected)
		/** Optional label colour override. */
		SLATE_ATTRIBUTE(FSlateColor, LabelColor)
		SLATE_ARGUMENT(bool, ClickSound)
		SLATE_EVENT(FSimpleDelegate, OnClicked)
		SLATE_NAMED_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);
	void SetText(const FText& InText);
	void SetKind(EAbyssButtonKind InKind) { Kind = InKind; }
	bool IsButtonHovered() const;
	bool IsButtonPressed() const;

private:
	void PaintFace(FAbyssPainter& Painter, const FVector2D& Size) const;
	FSlateColor GetLabelColor() const;
	TOptional<FSlateRenderTransform> GetPressOffset() const;

	TSharedPtr<FAbyssUiContext> Context;
	TSharedPtr<SButton> Button;
	TSharedPtr<STextBlock> LabelText;
	TAttribute<bool> Selected;
	TAttribute<FSlateColor> LabelColor;
	FSimpleDelegate OnClicked;
	EAbyssButtonKind Kind = EAbyssButtonKind::Secondary;
	bool bClickSound = true;
};

/** Panel close: the T_UI_Glyph_close medallion (fallback: a painted iron medallion with a red X). */
class SAbyssCloseButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssCloseButton)
		: _Diameter(30.f)
	{
	}
		SLATE_ARGUMENT(float, Diameter)
		SLATE_EVENT(FSimpleDelegate, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

private:
	TSharedPtr<FAbyssUiContext> Context;
	TSharedPtr<SButton> Button;
};

/**
 * Tab row (8.5 tabs): rounded tops (each tab clipped to its box, so only the top corners round), accent underline on the
 * active tab, label centred, count badge centred 12 px from the tab's right edge (web tx + tabW - 12) in the accent on
 * the active tab and #6e665c on the others.
 */
class SAbyssTabBar : public SCompoundWidget
{
public:
	struct FTab
	{
		FText Label;
		FLinearColor Accent = FLinearColor::White;
		FText Badge;
	};

	SLATE_BEGIN_ARGS(SAbyssTabBar)
		: _TabHeight(30.f)
		, _TabWidth(0.f)
		, _FontPx(13.f)
	{
	}
		SLATE_ARGUMENT(TArray<FTab>, Tabs)
		SLATE_ATTRIBUTE(int32, ActiveIndex)
		SLATE_ARGUMENT(float, TabHeight)
		/** 0 = equal shares of the available width. */
		SLATE_ARGUMENT(float, TabWidth)
		SLATE_ARGUMENT(float, FontPx)
		SLATE_EVENT(FOnAbyssIndexSelected, OnTabSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

private:
	TSharedPtr<FAbyssUiContext> Context;
	TAttribute<int32> ActiveIndex;
	FOnAbyssIndexSelected OnTabSelected;
};

/** Horizontal 0..1 slider (save-ui-input 7.13): press sets the value, drag while held. */
class SAbyssSlider : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssSlider)
		: _Width(180.f)
		, _Height(28.f)
	{
	}
		SLATE_ATTRIBUTE(float, Value)
		SLATE_ARGUMENT(float, Width)
		SLATE_ARGUMENT(float, Height)
		SLATE_EVENT(FOnAbyssFloatChanged, OnValueChanged)
		/** Fired once when the drag ends (persist then). */
		SLATE_EVENT(FOnAbyssFloatChanged, OnValueCommitted)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	float ValueAt(const FGeometry& MyGeometry, const FVector2D& ScreenPosition) const;

	TSharedPtr<FAbyssUiContext> Context;
	TAttribute<float> Value;
	FOnAbyssFloatChanged OnValueChanged;
	FOnAbyssFloatChanged OnValueCommitted;
	float DragValue = 0.f;
	bool bDragging = false;
};

/** What an item slot shows. */
struct FAbyssSlotItem
{
	TOptional<abyss::ItemInstance> Item;
	/** Equipment slot ghost when empty (paper doll), else none. */
	TOptional<abyss::EquipSlot> GhostSlot;
	/** Socket badge "filled/max" (equipped items with capacity). */
	int32 SocketsFilled = 0;
	int32 SocketsMax = 0;
	bool bDimmed = false;
	bool bLocked = false;
	bool bSelected = false;
	/** Hover tooltip compares against the worn item (bag / shop / stash slots). */
	bool bCompare = true;
	/** The tooltip shows this sell price instead of the item's (0 = the item's). */
	int64 PriceOverride = 0;
	/** Extra tooltip line (e.g. a buy price). */
	FText TooltipFooter;
};

/** Item slot (8.5): quality frame, icon (or glyph fallback), quantity, sockets, hover tooltip, clicks. */
class SAbyssItemSlot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssItemSlot)
		: _SlotSize(41.f)
	{
	}
		SLATE_ARGUMENT(float, SlotSize)
		SLATE_ARGUMENT(FAbyssSlotItem, Entry)
		SLATE_EVENT(FOnAbyssPointerAction, OnClicked)
		SLATE_EVENT(FOnAbyssPointerAction, OnRightClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);
	virtual ~SAbyssItemSlot() override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** Paints one item cell (shared by slots drawn inside other canvases). */
	static void PaintItem(FAbyssPainter& Painter, const FAbyssUiContext& Ctx, const FVector2D& Pos, float Size,
		const abyss::ItemInstance* Item, bool bHovered, const abyss::EquipSlot* Ghost = nullptr);

private:
	TSharedPtr<FAbyssUiContext> Context;
	FAbyssSlotItem Entry;
	FOnAbyssPointerAction OnClicked;
	FOnAbyssPointerAction OnRightClicked;
	float SlotSize = 41.f;
	bool bPressedTouch = false;
	FVector2D TouchStart = FVector2D::ZeroVector;
};

/** Item tooltip content (loot-items-inventory 15.2 + 14 comparison). */
namespace AbyssItemTooltip
{
	struct FOptions
	{
		bool bCompare = true;
		bool bEquippedTag = false;
		int64 PriceOverride = 0;
		FText Footer;
		/** Touch cards are scaled x1.45 (save-ui-input 7.2). */
		float Scale = 1.f;
	};
	/** One item card. */
	TSharedRef<SWidget> MakeCard(const TSharedRef<FAbyssUiContext>& Ctx, const abyss::ItemInstance& Item, const FOptions& Options);
	/** The card plus, on desktop, the worn item's card (tagged) when the item is gear and that slot is occupied. */
	TSharedRef<SWidget> MakeTooltip(const TSharedRef<FAbyssUiContext>& Ctx, const abyss::ItemInstance& Item, const FOptions& Options);
}

/** A panel frame (8.3 panel variant) with a title band, close medallion and a content slot. */
class SAbyssPanelFrame : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssPanelFrame)
		: _Size(FVector2D(400.0, 300.0))
		, _HeaderHeight(36.f)
		, _Accent(FLinearColor(0.f, 0.f, 0.f, 0.f))
		, _ShowClose(true)
	{
	}
		SLATE_ARGUMENT(FVector2D, Size)
		SLATE_ATTRIBUTE(FText, Title)
		SLATE_ARGUMENT(float, HeaderHeight)
		/** Hairline colour (transparent = gold). */
		SLATE_ARGUMENT(FLinearColor, Accent)
		SLATE_ARGUMENT(bool, ShowClose)
		/** Content inset (unset = 14 / header + 6 / 14 / 12; FMargin(0) = absolute layout over the whole panel). */
		SLATE_ARGUMENT(TOptional<FMargin>, ContentPadding)
		SLATE_EVENT(FSimpleDelegate, OnClose)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

private:
	TSharedPtr<FAbyssUiContext> Context;
};

/** A small framed card (tooltip variant) around content. */
class SAbyssCardFrame : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssCardFrame)
		: _Accent(FLinearColor(0.f, 0.f, 0.f, 0.f))
		, _Padding(FMargin(10.f, 8.f))
		, _Kind(1)
	{
	}
		SLATE_ARGUMENT(FLinearColor, Accent)
		SLATE_ARGUMENT(FMargin, Padding)
		/** 1 = tooltip frame, 2 = plate. */
		SLATE_ARGUMENT(int32, Kind)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);
};
