#include "UI/Widgets/AbyssUiWidgets.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include <optional>
#include <string>
#include <vector>

#include "abyss/data/DataStore.h"
#include "abyss/items/Crafting.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/ItemCompare.h"

#include "Framework/AbyssText.h"

// =====================================================================================================================
// Helpers
// =====================================================================================================================

namespace AbyssUi
{
	FText StripButtonLabel(const FText& InLabel)
	{
		FString Label = InLabel.ToString().TrimStartAndEnd();
		const auto StripPair = [&Label](const TCHAR* Open, const TCHAR* Close)
		{
			if (Label.StartsWith(Open) && Label.EndsWith(Close) && Label.Len() >= 2)
			{
				Label = Label.Mid(1, Label.Len() - 2).TrimStartAndEnd();
				return true;
			}
			return false;
		};
		if (!StripPair(TEXT("["), TEXT("]")))
		{
			StripPair(TEXT("\x3010"), TEXT("\x3011"));   // fullwidth lenticular brackets
		}
		return FText::AsCultureInvariant(Label);
	}

	TSharedRef<STextBlock> Label(const FAbyssUiContext& Ctx, const TAttribute<FText>& Text, float Px, const FLinearColor& Color, bool bBold,
		int32 OutlinePx, float WrapAt)
	{
		TSharedRef<STextBlock> Block = SNew(STextBlock)
			.Text(Text)
			.Font(Ctx.Style().Body(Px, bBold, OutlinePx))
			.ColorAndOpacity(FSlateColor(Color));
		if (WrapAt > 0.f)
		{
			Block->SetWrapTextAt(WrapAt);
			Block->SetAutoWrapText(false);
		}
		return Block;
	}

	TSharedRef<STextBlock> TitleLabel(const FAbyssUiContext& Ctx, const TAttribute<FText>& Text, float Px, const FLinearColor& Color, bool bBold,
		int32 OutlinePx)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(Ctx.Style().Font(EAbyssFontFace::Title, Px, bBold, OutlinePx, Ctx.Style().Colors().Ink))
			.ColorAndOpacity(FSlateColor(Color))
			.ShadowOffset(FVector2D(1.0, 2.0))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	}

	TSharedRef<SScrollBox> ScrollBox(const FAbyssUiContext& Ctx, bool bShowBar)
	{
		return SNew(SScrollBox)
			.Orientation(Orient_Vertical)
			.ScrollBarStyle(&Ctx.Style().ScrollBar())
			.ScrollBarThickness(FVector2D(6.0, 6.0))
			.ScrollBarVisibility(bShowBar ? EVisibility::Visible : EVisibility::Collapsed)
			.ScrollBarAlwaysVisible(false)
			.AllowOverscroll(EAllowOverscroll::No)
			.ConsumeMouseWheel(EConsumeMouseWheel::Always);
	}

	TSharedRef<SWidget> SectionHeader(const TSharedRef<FAbyssUiContext>& Ctx, const FText& Text, float Width)
	{
		const FAbyssUiPalette& C = Ctx->Style().Colors();
		const FSlateFontInfo Font = Ctx->Style().Body(12.f, true, 1);
		const FString Str = Text.ToString();
		return SNew(SAbyssCanvas, Ctx, [Str, Font, C](FAbyssPainter& P, const FVector2D& Size)
			{
				const FVector2D TextSize = FAbyssPainter::Measure(Str, Font);
				P.Text(FVector2D(0.0, (Size.Y - TextSize.Y) * 0.5), Str, Font, C.Heading);
				const double X0 = TextSize.X + 8.0;
				const double Y = Size.Y * 0.5;
				if (Size.X > X0 + 4.0)
				{
					const double Span = Size.X - X0;
					P.Line(FVector2D(X0, Y), FVector2D(X0 + Span * 0.5, Y), FAbyssUiStyle::WithAlpha(C.Gold, 0.7f), 1.f);
					P.Line(FVector2D(X0 + Span * 0.5, Y), FVector2D(Size.X, Y), FAbyssUiStyle::WithAlpha(C.Gold, 0.2f), 1.f);
				}
			})
			.Size(FVector2D(Width, 18.0));
	}

	TSharedRef<SWidget> Divider(const TSharedRef<FAbyssUiContext>& Ctx, float Width, bool bDiamond)
	{
		return SNew(SAbyssCanvas, Ctx, [bDiamond](FAbyssPainter& P, const FVector2D& Size)
			{
				P.Divider(FVector2D(Size.X * 0.5, Size.Y * 0.5), static_cast<float>(Size.X), bDiamond);
			})
			.Size(FVector2D(Width, 12.0));
	}

	FLinearColor Alpha(const FLinearColor& Color, float A)
	{
		return FAbyssUiStyle::WithAlpha(Color, A);
	}
}

// =====================================================================================================================
// SAbyssCanvas
// =====================================================================================================================

void SAbyssCanvas::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, FPaintFn InPaint)
{
	Context = InContext;
	Paint = MoveTemp(InPaint);
	bPaintAbove = InArgs._PaintAbove;
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(InArgs._Size.X > 0.0 ? FOptionalSize(static_cast<float>(InArgs._Size.X)) : FOptionalSize())
		.HeightOverride(InArgs._Size.Y > 0.0 ? FOptionalSize(static_cast<float>(InArgs._Size.Y)) : FOptionalSize())
		.Padding(InArgs._Padding)
		[
			InArgs._Content.Widget
		]
	];
}

int32 SAbyssCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = LayerId;
	const FVector2D LocalSize(AllottedGeometry.GetLocalSize());
	if (Paint && !bPaintAbove)
	{
		FAbyssPainter Painter(AllottedGeometry, OutDrawElements, Layer, InWidgetStyle, Context->Style());
		Paint(Painter, LocalSize);
		Layer = Painter.Layer;
	}
	Layer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, Layer + 1, InWidgetStyle, bParentEnabled);
	if (Paint && bPaintAbove)
	{
		FAbyssPainter Painter(AllottedGeometry, OutDrawElements, Layer + 1, InWidgetStyle, Context->Style());
		Paint(Painter, LocalSize);
		Layer = Painter.Layer;
	}
	return Layer;
}

// =====================================================================================================================
// SAbyssBlocker
// =====================================================================================================================

void SAbyssBlocker::Construct(const FArguments& InArgs)
{
	OnPressed = InArgs._OnPressed;
	bConsumeWheel = InArgs._ConsumeWheel;
	ChildSlot
	[
		InArgs._Content.Widget
	];
}

FReply SAbyssBlocker::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	OnPressed.ExecuteIfBound();
	return FReply::Handled();
}

FReply SAbyssBlocker::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

FReply SAbyssBlocker::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}

FReply SAbyssBlocker::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return bConsumeWheel ? FReply::Handled() : FReply::Unhandled();
}

FReply SAbyssBlocker::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	OnPressed.ExecuteIfBound();
	return FReply::Handled();
}

FReply SAbyssBlocker::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return FReply::Handled();
}

// =====================================================================================================================
// SAbyssHitArea
// =====================================================================================================================

void SAbyssHitArea::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Context = InContext;
	OnPressed = InArgs._OnPressed;
	OnRightPressed = InArgs._OnRightPressed;
	OnHovered = InArgs._OnHovered;
	bClickSound = InArgs._ClickSound;
	ChildSlot
	[
		InArgs._Content.Widget
	];
}

FReply SAbyssHitArea::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!IsEnabled())
	{
		return FReply::Handled();
	}
	const bool bRight = !MouseEvent.IsTouchEvent() && MouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
	const FSimpleDelegate& Handler = bRight ? OnRightPressed : OnPressed;
	if (Handler.IsBound())
	{
		if (bClickSound && Context.IsValid())
		{
			Context->PlaySound(abyss::SfxId::Click);
		}
		Handler.Execute();
	}
	return FReply::Handled();
}

FReply SAbyssHitArea::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

FReply SAbyssHitArea::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}

void SAbyssHitArea::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	OnHovered.ExecuteIfBound();
}

FCursorReply SAbyssHitArea::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return OnPressed.IsBound() ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
}

// =====================================================================================================================
// SAbyssButton
// =====================================================================================================================

void SAbyssButton::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Context = InContext;
	Kind = InArgs._Kind;
	Selected = InArgs._Selected;
	LabelColor = InArgs._LabelColor;
	OnClicked = InArgs._OnClicked;
	bClickSound = InArgs._ClickSound;
	const bool bTouch = InContext->IsTouch();

	TSharedRef<SWidget> Content = InArgs._Content.Widget;
	if (Content == SNullWidget::NullWidget)
	{
		TAttribute<FText> Text;
		if (InArgs._Text.IsBound())
		{
			Text = TAttribute<FText>::CreateLambda([Source = InArgs._Text]() { return AbyssUi::StripButtonLabel(Source.Get()); });
		}
		else
		{
			Text = AbyssUi::StripButtonLabel(InArgs._Text.Get(FText::GetEmpty()));
		}
		Content = SAssignNew(LabelText, STextBlock)
			.Text(Text)
			.Font(InContext->Style().Body(InArgs._FontPx, true, 1))
			.Justification(ETextJustify::Center)
			.ColorAndOpacity(this, &SAbyssButton::GetLabelColor);
	}

	const TSharedRef<FAbyssUiContext> Ctx = InContext;
	TSharedRef<SWidget> ButtonWidget = SAssignNew(Button, SButton)
		.ButtonStyle(&InContext->Style().InvisibleButton())
		.ContentPadding(FMargin(0.f))
		.IsFocusable(false)
		.ClickMethod(bTouch ? EButtonClickMethod::DownAndUp : EButtonClickMethod::MouseDown)
		.TouchMethod(EButtonTouchMethod::PreciseTap)
		.PressMethod(EButtonPressMethod::DownAndUp)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.OnClicked_Lambda([this]()
		{
			if (bClickSound && Context.IsValid())
			{
				Context->PlaySound(abyss::SfxId::Click);
			}
			OnClicked.ExecuteIfBound();
			return FReply::Handled();
		})
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SAbyssCanvas, Ctx, [this](FAbyssPainter& Painter, const FVector2D& Size) { PaintFace(Painter, Size); })
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(8.f, 2.f, 8.f, 2.f))
			[
				SNew(SBox)
				.RenderTransform(this, &SAbyssButton::GetPressOffset)
				[
					Content
				]
			]
		];

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(InArgs._Width > 0.f ? FOptionalSize(InArgs._Width) : FOptionalSize())
		.HeightOverride(InArgs._Height > 0.f ? FOptionalSize(InArgs._Height) : FOptionalSize())
		.MinDesiredHeight(InArgs._Height > 0.f ? FOptionalSize() : FOptionalSize(24.f))
		[
			ButtonWidget
		]
	];
}

TOptional<FSlateRenderTransform> SAbyssButton::GetPressOffset() const
{
	// Pressed: label 1 px down (8.4).
	if (IsButtonPressed())
	{
		return FSlateRenderTransform(FVector2f(0.f, 1.f));
	}
	return TOptional<FSlateRenderTransform>();
}

void SAbyssButton::SetText(const FText& InText)
{
	if (LabelText.IsValid())
	{
		LabelText->SetText(AbyssUi::StripButtonLabel(InText));
	}
}

bool SAbyssButton::IsButtonHovered() const
{
	return Button.IsValid() && Button->IsHovered();
}

bool SAbyssButton::IsButtonPressed() const
{
	return Button.IsValid() && Button->IsPressed();
}

FSlateColor SAbyssButton::GetLabelColor() const
{
	if (!IsEnabled())
	{
		return FSlateColor(FAbyssUiStyle::Rgb(0x6a635c));
	}
	if (LabelColor.IsSet() || LabelColor.IsBound())
	{
		return LabelColor.Get();
	}
	const FAbyssButtonColors& Colors = Context->Style().ButtonColors(Kind);
	return FSlateColor(IsButtonHovered() ? Colors.LabelHover : Colors.Label);
}

void SAbyssButton::PaintFace(FAbyssPainter& P, const FVector2D& Size) const
{
	const FAbyssButtonColors& Colors = Context->Style().ButtonColors(Kind);
	const bool bEnabled = IsEnabled();
	const bool bHovered = bEnabled && IsButtonHovered();
	const bool bPressed = bEnabled && IsButtonPressed();
	const bool bSelected = Selected.Get(false);
	const float Radius = FMath::Min(5.f, static_cast<float>(Size.Y / 3.0));

	FLinearColor Top = Colors.Top;
	FLinearColor Bottom = Colors.Bottom;
	FLinearColor Border = Colors.Border;
	if (!bEnabled)
	{
		Top = FAbyssUiStyle::Rgb(0x2a282c);
		Bottom = FAbyssUiStyle::Rgb(0x141316);
		Border = FAbyssUiStyle::Rgb(0x3d3a40);
	}
	else if (bPressed)
	{
		Swap(Top, Bottom);
		Top = FAbyssUiStyle::Darken(Top, 0.25f);
	}
	else if (bHovered)
	{
		Top = FAbyssUiStyle::Lighten(Top, 0.18f);
		Bottom = FAbyssUiStyle::Lighten(Bottom, 0.12f);
		Border = FAbyssUiStyle::Lighten(Border, 0.3f);
	}
	if (bSelected && bEnabled)
	{
		Border = Context->Style().Colors().GoldBright;
	}

	const FVector2D Offset(0.0, bPressed ? 1.0 : 0.0);
	if (!bPressed)
	{
		P.RoundBox(FVector2D(0.0, 2.0), Size, FLinearColor(0.f, 0.f, 0.f, 0.45f), Radius);
	}
	if ((bHovered || bSelected) && bEnabled)
	{
		P.RoundBox(FVector2D(-2.0, -2.0) + Offset, Size + FVector2D(4.0, 4.0), FLinearColor::Transparent, Radius + 2.f,
			FAbyssUiStyle::WithAlpha(Border, 0.35f), 2.f);
	}
	P.RoundBox(Offset, Size, FMath::Lerp(Top, Bottom, 0.5f), Radius, FAbyssUiStyle::Rgb(0x050407), 1.f);
	if (Size.Y > Radius * 2.f + 1.f)
	{
		P.VerticalGradient(Offset + FVector2D(1.0, Radius), FVector2D(Size.X - 2.0, Size.Y - Radius * 2.f), Top, Bottom, 6);
	}
	if (bEnabled && !bPressed)
	{
		P.Box(Offset + FVector2D(Radius, 1.5), FVector2D(Size.X - Radius * 2.f, FMath::Max(1.0, Size.Y * 0.32)), FLinearColor(1.f, 0.9f, 0.8f, 0.10f));
	}
	P.RoundBox(Offset + FVector2D(0.5, 0.5), Size - FVector2D(1.0, 1.0), FLinearColor::Transparent, Radius, Border, 1.2f);
	if (Size.X >= 70.0 && Size.Y >= 22.0)
	{
		const FLinearColor Stud = bEnabled ? Border : FAbyssUiStyle::Rgb(0x3d3a40);
		P.Diamond(Offset + FVector2D(5.0, Size.Y * 0.5), 4.f, Stud);
		P.Diamond(Offset + FVector2D(Size.X - 5.0, Size.Y * 0.5), 4.f, Stud);
	}
}

// =====================================================================================================================
// SAbyssCloseButton
// =====================================================================================================================

void SAbyssCloseButton::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Context = InContext;
	const FSimpleDelegate OnClicked = InArgs._OnClicked;
	const float Diameter = InArgs._Diameter;
	const TSharedRef<FAbyssUiContext> Ctx = InContext;
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(Diameter)
		.HeightOverride(Diameter)
		[
			SAssignNew(Button, SButton)
			.ButtonStyle(&InContext->Style().InvisibleButton())
			.ContentPadding(FMargin(0.f))
			.IsFocusable(false)
			.ClickMethod(InContext->IsTouch() ? EButtonClickMethod::DownAndUp : EButtonClickMethod::MouseDown)
			.TouchMethod(EButtonTouchMethod::PreciseTap)
			.OnClicked_Lambda([OnClicked, Ctx]()
			{
				Ctx->PlaySound(abyss::SfxId::Click);
				OnClicked.ExecuteIfBound();
				return FReply::Handled();
			})
			[
				SNew(SAbyssCanvas, Ctx, [this](FAbyssPainter& P, const FVector2D& Size)
				{
					const bool bHovered = Button.IsValid() && Button->IsHovered();
					const bool bPressed = Button.IsValid() && Button->IsPressed();
					const float Scale = bPressed ? 0.9f : 1.f;
					const FVector2D Center = Size * 0.5;
					const float R = static_cast<float>(FMath::Min(Size.X, Size.Y) * 0.5 * Scale);
					if (bHovered)
					{
						P.Circle(Center, R + 2.f, FLinearColor(0.88f, 0.12f, 0.08f, 0.25f));
					}
					P.Circle(Center + FVector2D(0.0, 1.5), R, FLinearColor(0.f, 0.f, 0.f, 0.5f));
					P.Circle(Center, R, FAbyssUiStyle::Rgb(0x2a252e), FAbyssUiStyle::Rgb(0x6d6573), 1.5f);
					P.Circle(Center, R * 0.72f, FAbyssUiStyle::Rgb(0x1a171d), FAbyssUiStyle::Rgb(0x050407), 1.f);
					const FLinearColor Cross = bHovered ? FAbyssUiStyle::Rgb(0xffb09a) : FAbyssUiStyle::Rgb(0xe0634e);
					const float Arm = R * 0.36f;
					P.Line(Center + FVector2D(-Arm, -Arm), Center + FVector2D(Arm, Arm), Cross, FMath::Max(2.f, R * 0.14f));
					P.Line(Center + FVector2D(-Arm, Arm), Center + FVector2D(Arm, -Arm), Cross, FMath::Max(2.f, R * 0.14f));
				})
			]
		]
	];
}

// =====================================================================================================================
// SAbyssTabBar
// =====================================================================================================================

void SAbyssTabBar::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Context = InContext;
	ActiveIndex = InArgs._ActiveIndex;
	OnTabSelected = InArgs._OnTabSelected;
	const TSharedRef<FAbyssUiContext> Ctx = InContext;
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	const float FontPx = InArgs._FontPx;
	const float TabH = InArgs._TabHeight;
	for (int32 Index = 0; Index < InArgs._Tabs.Num(); ++Index)
	{
		const FTab Tab = InArgs._Tabs[Index];
		TSharedPtr<SButton> TabButton;
		const TAttribute<int32> Active = ActiveIndex;
		const auto IsActive = [Active, Index]() { return Active.Get(0) == Index; };
		// The button content is filled after SAssignNew: the canvas captures a weak pointer to the button, and in
		// `SAssignNew(...)` (= `MakeTDecl(...).Expose(TabButton) <<= FArguments()...[...]`) the right operand of the
		// overloaded `<<=` is evaluated before Expose assigns TabButton (C++17 P0145), so capturing it inline would
		// always capture null (same pattern as SAbyssMainMenu's slot cards).
		TSharedRef<SOverlay> TabContent = SNew(SOverlay);
		TSharedRef<SWidget> TabWidget = SAssignNew(TabButton, SButton)
			.ButtonStyle(&Ctx->Style().InvisibleButton())
			.ContentPadding(FMargin(0.f))
			.IsFocusable(false)
			.ClickMethod(Ctx->IsTouch() ? EButtonClickMethod::DownAndUp : EButtonClickMethod::MouseDown)
			.TouchMethod(EButtonTouchMethod::PreciseTap)
			.OnClicked_Lambda([this, Index, Ctx]()
			{
				Ctx->PlaySound(abyss::SfxId::Click);
				OnTabSelected.ExecuteIfBound(Index);
				return FReply::Handled();
			})
			[
				TabContent
			];
		const TWeakPtr<SButton> WeakButton = TabButton;
		TabContent->AddSlot()
		[
			SNew(SAbyssCanvas, Ctx, [IsActive, Tab, WeakButton](FAbyssPainter& P, const FVector2D& Size)
			{
				const bool bActive = IsActive();
				const TSharedPtr<SButton> Pinned = WeakButton.Pin();
				const bool bHovered = Pinned.IsValid() && Pinned->IsHovered();
				const float Radius = 6.f;
				if (bActive)
				{
					const FLinearColor Top = FAbyssUiStyle::Lighten(FMath::Lerp(FAbyssUiStyle::Rgb(0x2a2430), Tab.Accent, 0.35f), 0.05f);
					const FLinearColor Bottom = FMath::Lerp(FAbyssUiStyle::Rgb(0x141117), Tab.Accent, 0.15f);
					P.RoundBox(FVector2D::ZeroVector, Size + FVector2D(0.0, Radius), FMath::Lerp(Top, Bottom, 0.5f), Radius);
					P.VerticalGradient(FVector2D(1.0, Radius), FVector2D(Size.X - 2.0, Size.Y - Radius), Top, Bottom, 6);
					P.RoundBox(FVector2D::ZeroVector, Size + FVector2D(0.0, Radius), FLinearColor::Transparent, Radius, Tab.Accent, 1.2f);
					P.Box(FVector2D(1.0, Size.Y - 2.5), FVector2D(Size.X - 2.0, 2.5), Tab.Accent);
					P.Box(FVector2D(Radius, 1.5), FVector2D(Size.X - Radius * 2.f, Size.Y * 0.3), FLinearColor(1.f, 0.9f, 0.8f, 0.08f));
				}
				else
				{
					const FLinearColor Top = bHovered ? FAbyssUiStyle::Rgb(0x221d25) : FAbyssUiStyle::Rgb(0x1a171d);
					P.RoundBox(FVector2D::ZeroVector, Size + FVector2D(0.0, Radius), Top, Radius);
					P.VerticalGradient(FVector2D(1.0, Radius), FVector2D(Size.X - 2.0, Size.Y - Radius), Top, FAbyssUiStyle::Rgb(0x0f0d11), 5);
					P.RoundBox(FVector2D::ZeroVector, Size + FVector2D(0.0, Radius), FLinearColor::Transparent, Radius,
						FAbyssUiStyle::Rgb(0x3a343f), 1.f);
				}
			})
		];
		TabContent->AddSlot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Tab.Label)
				.Font(Ctx->Style().Body(FontPx, true, 1))
				.ColorAndOpacity_Lambda([IsActive, Ctx]()
				{
					return FSlateColor(IsActive() ? Ctx->Style().Colors().Parchment : FAbyssUiStyle::Rgb(0x8a8290));
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(6.f, 0.f, 0.f, 0.f))
			[
				SNew(STextBlock)
				.Visibility(Tab.Badge.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
				.Text(Tab.Badge)
				.Font(Ctx->Style().Body(FontPx * 0.8f, true, 1))
				.ColorAndOpacity(FSlateColor(Ctx->Style().Colors().GoldBright))
			]
		];
		if (InArgs._TabWidth > 0.f)
		{
			Row->AddSlot()
				.AutoWidth()
				.Padding(FMargin(Index == 0 ? 0.f : 4.f, 0.f, 0.f, 0.f))
				[
					SNew(SBox).WidthOverride(InArgs._TabWidth).HeightOverride(TabH)[TabWidget]
				];
		}
		else
		{
			Row->AddSlot()
				.FillWidth(1.f)
				.Padding(FMargin(Index == 0 ? 0.f : 4.f, 0.f, 0.f, 0.f))
				[
					SNew(SBox).HeightOverride(TabH)[TabWidget]
				];
		}
	}
	ChildSlot
	[
		Row
	];
}

// =====================================================================================================================
// SAbyssSlider
// =====================================================================================================================

void SAbyssSlider::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Context = InContext;
	Value = InArgs._Value;
	OnValueChanged = InArgs._OnValueChanged;
	OnValueCommitted = InArgs._OnValueCommitted;
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(InArgs._Width)
		.HeightOverride(InArgs._Height)
	];
}

float SAbyssSlider::ValueAt(const FGeometry& MyGeometry, const FVector2D& ScreenPosition) const
{
	const FVector2D Local(MyGeometry.AbsoluteToLocal(ScreenPosition));
	const double Width = FMath::Max(1.0, static_cast<double>(MyGeometry.GetLocalSize().X) - 16.0);
	return FMath::Clamp(static_cast<float>((Local.X - 8.0) / Width), 0.f, 1.f);
}

FReply SAbyssSlider::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!IsEnabled())
	{
		return FReply::Handled();
	}
	bDragging = true;
	DragValue = ValueAt(MyGeometry, MouseEvent.GetScreenSpacePosition());
	OnValueChanged.ExecuteIfBound(DragValue);
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SAbyssSlider::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bDragging && HasMouseCapture())
	{
		DragValue = ValueAt(MyGeometry, MouseEvent.GetScreenSpacePosition());
		OnValueChanged.ExecuteIfBound(DragValue);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SAbyssSlider::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bDragging)
	{
		bDragging = false;
		OnValueCommitted.ExecuteIfBound(DragValue);
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

void SAbyssSlider::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	if (bDragging)
	{
		bDragging = false;
		OnValueCommitted.ExecuteIfBound(DragValue);
	}
}

int32 SAbyssSlider::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FAbyssPainter P(AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, Context->Style());
	const FVector2D Size(AllottedGeometry.GetLocalSize());
	const float V = bDragging ? DragValue : FMath::Clamp(Value.Get(0.f), 0.f, 1.f);
	const double TrackX = 8.0;
	const double TrackW = FMath::Max(1.0, Size.X - 16.0);
	const double TrackY = Size.Y * 0.5 - 4.0;
	const FAbyssUiPalette& C = Context->Style().Colors();
	P.RoundBox(FVector2D(TrackX, TrackY), FVector2D(TrackW, 8.0), FAbyssUiStyle::Rgb(0x0c0b0e), 4.f, FAbyssUiStyle::Rgb(0x3a343f), 1.f);
	if (V > 0.f)
	{
		P.RoundBox(FVector2D(TrackX, TrackY), FVector2D(FMath::Max(8.0, TrackW * V), 8.0), C.Gold, 4.f);
	}
	const FVector2D Handle(TrackX + TrackW * V, Size.Y * 0.5);
	P.Circle(Handle + FVector2D(0.0, 1.5), 8.f, FLinearColor(0.f, 0.f, 0.f, 0.5f));
	P.Circle(Handle, 8.f, IsEnabled() ? C.Heading : C.Dim, FAbyssUiStyle::Rgb(0x5a3a10), 1.5f);
	return P.Layer;
}

// =====================================================================================================================
// SAbyssItemSlot
// =====================================================================================================================

void SAbyssItemSlot::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Context = InContext;
	Entry = InArgs._Entry;
	OnClicked = InArgs._OnClicked;
	OnRightClicked = InArgs._OnRightClicked;
	SlotSize = InArgs._SlotSize;
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(SlotSize)
		.HeightOverride(SlotSize)
	];
}

SAbyssItemSlot::~SAbyssItemSlot()
{
	if (Context.IsValid())
	{
		Context->HideTooltip(this);
	}
}

FReply SAbyssItemSlot::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (Entry.bLocked)
	{
		return FReply::Handled();
	}
	if (MouseEvent.IsTouchEvent())
	{
		// Touch: act on release without drag (a drag scrolls the surrounding list).
		bPressedTouch = true;
		TouchStart = FVector2D(MouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (OnRightClicked.IsBound())
		{
			Context->HideTooltip(this);
			OnRightClicked.Execute(FVector2D(MouseEvent.GetScreenSpacePosition()));
		}
		return FReply::Handled();
	}
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && OnClicked.IsBound())
	{
		Context->HideTooltip(this);
		OnClicked.Execute(FVector2D(MouseEvent.GetScreenSpacePosition()));
	}
	return FReply::Handled();
}

FReply SAbyssItemSlot::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bPressedTouch && MouseEvent.IsTouchEvent())
	{
		bPressedTouch = false;
		const FVector2D End(MouseEvent.GetScreenSpacePosition());
		const float Threshold = FSlateApplication::Get().GetDragTriggerDistance();
		if (FVector2D::Distance(End, TouchStart) <= Threshold * 1.5f && MyGeometry.IsUnderLocation(End) && OnClicked.IsBound())
		{
			OnClicked.Execute(End);
		}
	}
	return FReply::Handled();
}

void SAbyssItemSlot::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	if (MouseEvent.IsTouchEvent() || Context->IsTouch() || !Entry.Item.IsSet())
	{
		return;
	}
	AbyssItemTooltip::FOptions Options;
	Options.bCompare = Entry.bCompare;
	Options.PriceOverride = Entry.PriceOverride;
	Options.Footer = Entry.TooltipFooter;
	Context->ShowTooltip(AbyssItemTooltip::MakeTooltip(Context.ToSharedRef(), Entry.Item.GetValue(), Options),
		FVector2D(MouseEvent.GetScreenSpacePosition()), this);
}

void SAbyssItemSlot::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);
	bPressedTouch = false;
	Context->HideTooltip(this);
}

void SAbyssItemSlot::PaintItem(FAbyssPainter& P, const FAbyssUiContext& Ctx, const FVector2D& Pos, float Size, const abyss::ItemInstance* Item,
	bool bHovered, const abyss::EquipSlot* Ghost)
{
	const FAbyssUiStyle& UiStyle = Ctx.Style();
	const FLinearColor QualityColor = Item ? UiStyle.QualityColor(Item->quality) : FAbyssUiStyle::Rgb(0x4a4350);
	const bool bFancy = Item && Item->quality != abyss::ItemQuality::Normal;
	const FVector2D Box(Size, Size);
	// glow for legendary / set and hover
	if (Item && (Item->quality == abyss::ItemQuality::Legendary || Item->quality == abyss::ItemQuality::Set || bHovered))
	{
		P.RoundBox(Pos - FVector2D(2.0, 2.0), Box + FVector2D(4.0, 4.0), FLinearColor::Transparent, 6.f,
			FAbyssUiStyle::WithAlpha(bHovered && !bFancy ? UiStyle.Colors().GoldBright : QualityColor, 0.35f), 2.f);
	}
	// recessed body tinted by quality
	P.RoundBox(Pos, Box, FAbyssUiStyle::Rgb(0x0a090c), 4.f);
	if (bFancy)
	{
		P.Glow(Pos + Box * 0.5, Size * 0.5f, FAbyssUiStyle::WithAlpha(QualityColor, 0.28f), 5);
	}
	P.Box(Pos + FVector2D(3.0, 1.0), FVector2D(Size - 6.0, 3.0), FLinearColor(0.f, 0.f, 0.f, 0.45f));
	// frame
	const float FrameW = !Item ? 1.f : (bFancy ? 1.6f : 1.2f);
	const FLinearColor FrameColor = Item ? QualityColor : FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0x4a4350), 0.85f);
	P.RoundBox(Pos, Box, FLinearColor::Transparent, 4.f, FrameColor, FrameW);
	P.Line(Pos + FVector2D(Size - 2.0, 4.0), Pos + FVector2D(Size - 2.0, Size - 3.0), FLinearColor(1.f, 1.f, 1.f, 0.06f), 1.f);
	if (Item && (Item->quality == abyss::ItemQuality::Rare || Item->quality == abyss::ItemQuality::Legendary || Item->quality == abyss::ItemQuality::Set))
	{
		const float Tick = Size * 0.18f;
		P.Line(Pos + FVector2D(2.0, 2.0), Pos + FVector2D(2.0 + Tick, 2.0), QualityColor, 1.5f);
		P.Line(Pos + FVector2D(2.0, 2.0), Pos + FVector2D(2.0, 2.0 + Tick), QualityColor, 1.5f);
		P.Line(Pos + FVector2D(Size - 2.0, Size - 2.0), Pos + FVector2D(Size - 2.0 - Tick, Size - 2.0), QualityColor, 1.5f);
		P.Line(Pos + FVector2D(Size - 2.0, Size - 2.0), Pos + FVector2D(Size - 2.0, Size - 2.0 - Tick), QualityColor, 1.5f);
	}
	if (Item)
	{
		const float IconSize = Size * 0.84f;
		const FVector2D IconPos = Pos + FVector2D((Size - IconSize) * 0.5f, (Size - IconSize) * 0.5f);
		if (const FSlateBrush* Icon = Ctx.ItemIcon(Item->baseId))
		{
			P.Brush(IconPos, FVector2D(IconSize, IconSize), Icon);
		}
		else
		{
			// Fallback glyph: the first two characters of the display name in the quality colour.
			const FString Name = Ctx.ItemName(*Item).Left(2);
			P.TextCentered(Pos + Box * 0.5, Name, UiStyle.Body(FMath::Max(9.f, Size * 0.3f), true, 1), QualityColor);
		}
		if (Item->quantity > 1)
		{
			const FString Qty = FAbyssUiContext::Int(Item->quantity);
			const FSlateFontInfo Font = UiStyle.Body(FMath::Max(9.f, Size * 0.24f), true, 2);
			const FVector2D QtySize = FAbyssPainter::Measure(Qty, Font);
			P.Text(Pos + FVector2D(Size - QtySize.X - 2.0, Size - QtySize.Y), Qty, Font, UiStyle.Colors().Text);
		}
	}
	else if (Ghost != nullptr)
	{
		// Empty paper-doll slot: a faint ghost of the slot's item kind (save-ui-input 7.1, tint 0x6a6070 alpha 0.28).
		static const TCHAR* GhostIcons[] = { TEXT("a_helm"), TEXT("a_armor"), TEXT("a_gloves"), TEXT("a_boots"), TEXT("w_sword"),
			TEXT("w_shield"), TEXT("j_amulet"), TEXT("j_ring"), TEXT("j_ring"), TEXT("a_belt") };
		const int32 GhostIndex = static_cast<int32>(*Ghost);
		const FSlateBrush* GhostBrush = GhostIndex >= 0 && GhostIndex < 10
			? Ctx.TextureBrush(FName(*(FString(TEXT("T_UI_ItemIcon_")) + GhostIcons[GhostIndex]))) : nullptr;
		const FLinearColor GhostTint = FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0x6a6070), 0.28f);
		if (GhostBrush != nullptr)
		{
			const float IconSize = Size * 0.78f;
			P.Brush(Pos + FVector2D((Size - IconSize) * 0.5f, (Size - IconSize) * 0.5f), FVector2D(IconSize, IconSize), GhostBrush, GhostTint);
		}
		else
		{
			P.Diamond(Pos + Box * 0.5, Size * 0.3f, GhostTint);
		}
	}
}

int32 SAbyssItemSlot::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FWidgetStyle SlotStyle = InWidgetStyle;
	if (Entry.bLocked)
	{
		SlotStyle.BlendColorAndOpacityTint(FLinearColor(1.f, 1.f, 1.f, 0.25f));
	}
	else if (Entry.bDimmed)
	{
		SlotStyle.BlendColorAndOpacityTint(FLinearColor(1.f, 1.f, 1.f, 0.4f));
	}
	FAbyssPainter P(AllottedGeometry, OutDrawElements, LayerId, SlotStyle, Context->Style());
	const FVector2D Size(AllottedGeometry.GetLocalSize());
	const float Cell = static_cast<float>(FMath::Min(Size.X, Size.Y));
	const abyss::ItemInstance* Item = Entry.Item.IsSet() ? &Entry.Item.GetValue() : nullptr;
	const abyss::EquipSlot* Ghost = Entry.GhostSlot.IsSet() ? &Entry.GhostSlot.GetValue() : nullptr;
	PaintItem(P, *Context, FVector2D::ZeroVector, Cell, Item, IsHovered() && !Entry.bLocked, Ghost);
	if (Entry.bSelected)
	{
		P.RoundBox(FVector2D(-1.0, -1.0), FVector2D(Cell + 2.0, Cell + 2.0), FLinearColor::Transparent, 5.f,
			Context->Style().Colors().GoldBright, 2.f);
	}
	if (Item != nullptr && Entry.SocketsMax > 0)
	{
		// "diamond n/max" in #8be9fd at the top right (save-ui-input 7.1).
		const FString Text = FString::Printf(TEXT("\x25C6%d/%d"), Entry.SocketsFilled, Entry.SocketsMax);
		const FSlateFontInfo Font = Context->Style().Body(9.f, true, 1);
		const FVector2D TextSize = FAbyssPainter::Measure(Text, Font);
		P.Text(FVector2D(Cell - TextSize.X - 1.0, 0.0), Text, Font, FAbyssUiStyle::Rgb(0x8be9fd));
	}
	return P.Layer;
}

// =====================================================================================================================
// Item tooltip (loot 15.2)
// =====================================================================================================================

namespace
{
	void AbyssTooltip_AddLine(const TSharedRef<SVerticalBox>& Box, const TSharedRef<FAbyssUiContext>& Ctx, const FString& Text, float Px,
		const FLinearColor& Color, bool bBold = false, float Wrap = 236.f, float TopPad = 1.f)
	{
		if (Text.IsEmpty())
		{
			return;
		}
		Box->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, TopPad, 0.f, 0.f))
			[
				AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Text), Px, Color, bBold, 1, Wrap)
			];
	}

	FString AbyssTooltip_CompareLabel(const FAbyssUiContext& Ctx, const abyss::CompareKey& Key)
	{
		switch (Key.kind)
		{
		case abyss::CompareKey::Kind::AvgDamage: return Ctx.LocOrStr("ui.compare.avgDamage", TEXT("Average damage"));
		case abyss::CompareKey::Kind::BaseDefense: return Ctx.LocOrStr("ui.compare.defense", TEXT("Defense"));
		case abyss::CompareKey::Kind::StatKey: return Ctx.StatLabel(Key.stat);
		}
		return FString();
	}
}

namespace AbyssItemTooltip
{
	TSharedRef<SWidget> MakeCard(const TSharedRef<FAbyssUiContext>& Ctx, const abyss::ItemInstance& Item, const FOptions& Options)
	{
		const abyss::DataStore* Data = Ctx->GetData();
		const abyss::Snapshot* Snap = Ctx->GetSnapshot();
		const FAbyssUiStyle& UiStyle = Ctx->Style();
		const FAbyssUiPalette& C = UiStyle.Colors();
		const abyss::ItemBaseDef* Base = Data ? Data->FindItemBase(Item.baseId) : nullptr;
		const FLinearColor QualityColor = UiStyle.QualityColor(Item.quality);
		const float Wrap = 236.f;

		TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
		if (Options.bEquippedTag)
		{
			AbyssTooltip_AddLine(Lines, Ctx, Ctx->LocOrStr("ui.compare.equippedTag", TEXT("Equipped")), 10.f, C.Muted, true);
		}
		// icon + name + quality line
		{
			abyss::ItemInstance IconItem = Item;
			const TSharedRef<FAbyssUiContext> IconCtx = Ctx;
			Lines->AddSlot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Top)
					.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
					[
						SNew(SAbyssCanvas, Ctx, [IconItem, IconCtx](FAbyssPainter& P, const FVector2D& Size)
						{
							SAbyssItemSlot::PaintItem(P, *IconCtx, FVector2D::ZeroVector, static_cast<float>(Size.X), &IconItem, false);
						})
						.Size(FVector2D(36.0, 36.0))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Ctx->ItemName(Item)), 14.f, QualityColor, true, 1, Wrap - 44.f)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							AbyssUi::Label(*Ctx, FText::AsCultureInvariant(FString::Printf(TEXT("%s \x00B7 Lv.%d"), *Ctx->QualityName(Item.quality), Item.level)),
								11.f, C.TextSoft)
						]
					]
				];
		}
		if (Base != nullptr)
		{
			// type line: ui.tooltip.type.<type> (ui.tooltip.slot.<slot>)
			FString TypeLine = Ctx->NameOr("ui.tooltip.type." + std::string(abyss::EnumName(Base->type)), std::string(abyss::EnumName(Base->type)));
			if (Base->hasSlot)
			{
				TypeLine += FString::Printf(TEXT(" (%s)"),
					*Ctx->NameOr("ui.tooltip.slot." + std::string(abyss::EnumName(Base->slot)), std::string(abyss::EnumName(Base->slot))));
			}
			AbyssTooltip_AddLine(Lines, Ctx, TypeLine, 11.f, C.Muted, false, Wrap, 4.f);

			// C10: weapon base damage / shield defense are inert in milestone 1: shown dimmed with the "(not active)" tag.
			const FString Inert = Ctx->LocOrStr("ui.tooltip.inert", TEXT("(inactive)"));
			const bool bShield = Base->hasWeaponType && Base->weaponType == abyss::WeaponType::Shield;
			if (Base->hasBaseDamage)
			{
				AbyssTooltip_AddLine(Lines, Ctx,
					Ctx->LocArgsOrStr("ui.tooltip.damage", TEXT("Damage: {min}-{max}"),
						{ FAbyssUiContext::Arg("min", Base->baseDamageMin), FAbyssUiContext::Arg("max", Base->baseDamageMax) }) + TEXT(" ") + Inert,
					12.f, C.Dim);
			}
			if (Base->hasBaseDefense && Base->baseDefense != 0)
			{
				FString DefenseLine = Ctx->LocArgsOrStr("ui.tooltip.defense", TEXT("Defense: {value}"),
					{ FAbyssUiContext::Arg("value", Base->baseDefense) });
				if (bShield)
				{
					DefenseLine += TEXT(" ") + Inert;
				}
				AbyssTooltip_AddLine(Lines, Ctx, DefenseLine, 12.f, bShield ? C.Dim : C.Text);
			}
			// level requirement (I3: enforced on equip)
			if (Base->hasSlot && Base->levelReq > 1)
			{
				const bool bTooLow = Snap != nullptr && Snap->hero.level < Base->levelReq;
				AbyssTooltip_AddLine(Lines, Ctx,
					Ctx->LocArgsOrStr("ui.tooltip.levelReq", TEXT("Requires level {level}"), { FAbyssUiContext::Arg("level", Base->levelReq) }),
					11.f, bTooLow ? C.Bad : C.TextSoft);
			}
			const bool bEnglish = Data->Strings().Current() == abyss::LocaleId::En;
			const FString Description = Ctx->NameOr("data.item." + Item.baseId + ".desc", bEnglish ? std::string() : Base->description);
			if (Description != AbyssText::ToFString("data.item." + Item.baseId + ".desc"))
			{
				AbyssTooltip_AddLine(Lines, Ctx, Description, 11.f, C.TextSoft, false, Wrap, 3.f);
			}
		}

		// affixes
		if (!Item.affixes.empty())
		{
			Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, 5.f, 0.f, 3.f))[AbyssUi::Divider(Ctx, Wrap, false)];
			for (const abyss::ItemAffix& Affix : Item.affixes)
			{
				AbyssTooltip_AddLine(Lines, Ctx, FString::Printf(TEXT("%s %s"), *Ctx->StatValue(Affix.stat, Affix.value), *Ctx->StatLabel(Affix.stat)),
					12.f, FAbyssUiStyle::Rgb(0x7fb0ff));
			}
		}
		// legendary effect (orange)
		if (!Item.legendaryId.empty() || !Item.legendaryEffect.empty())
		{
			FString Effect;
			if (!Item.legendaryId.empty() && Ctx->HasKey("data.legendary." + Item.legendaryId + ".effect"))
			{
				Effect = Ctx->LocStr("data.legendary." + Item.legendaryId + ".effect");
			}
			else if (!Item.legendaryEffect.empty())
			{
				Effect = AbyssText::ToFString(Item.legendaryEffect);
			}
			else
			{
				Effect = Ctx->LocOrStr("ui.tooltip.legendaryEffect", TEXT("Holds an unknown power"));
			}
			AbyssTooltip_AddLine(Lines, Ctx, Effect, 12.f, FAbyssUiStyle::Rgb(0xff8a2a), false, Wrap, 4.f);
		}
		// gem effect (the item is a gem)
		if (Base != nullptr && Base->isGem)
		{
			AbyssTooltip_AddLine(Lines, Ctx,
				Ctx->LocArgsOrStr("ui.tooltip.gemEffect", TEXT("Socket effect: +{value}{suffix} {label}"),
					{ FAbyssUiContext::Arg("value", FAbyssUiContext::Num(Base->gemValue)),
						FAbyssUiContext::Arg("suffix", Ctx->IsPercentStat(Base->gemStat) ? FString(TEXT("%")) : FString()),
						FAbyssUiContext::Arg("label", Ctx->StatLabel(Base->gemStat)) }),
				12.f, FAbyssUiStyle::Rgb(0x8be9fd), false, Wrap, 4.f);
		}
		// socketed gems + socket count
		const int32 Capacity = Data ? abyss::ItemSocketCapacity(Item, *Data) : 0;
		if (!Item.sockets.empty())
		{
			for (const abyss::GemInstance& Gem : Item.sockets)
			{
				AbyssTooltip_AddLine(Lines, Ctx,
					FString::Printf(TEXT("\x25C6 %s: %s %s"), *Ctx->ItemBaseName(Gem.gemId), *Ctx->StatValue(Gem.stat, Gem.value), *Ctx->StatLabel(Gem.stat)),
					11.f, FAbyssUiStyle::Rgb(0x8be9fd));
			}
		}
		if (Capacity > 0)
		{
			AbyssTooltip_AddLine(Lines, Ctx,
				Ctx->LocArgsOrStr("ui.tooltip.socketCount", TEXT("Sockets: {filled}/{max}"),
					{ FAbyssUiContext::Arg("filled", static_cast<int64>(Item.sockets.size())), FAbyssUiContext::Arg("max", Capacity) }),
				11.f, C.Muted);
		}
		// set block
		if (!Item.setId.empty() && Data != nullptr)
		{
			if (const abyss::SetDef* Set = Data->Items().FindSet(Item.setId))
			{
				const int32 Equipped = Snap && Snap->inventory ? Snap->inventory->EquippedSetCount(Item.setId) : 0;
				Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, 5.f, 0.f, 3.f))[AbyssUi::Divider(Ctx, Wrap, false)];
				const bool bEnglish = Data->Strings().Current() == abyss::LocaleId::En;
				AbyssTooltip_AddLine(Lines, Ctx,
					FString::Printf(TEXT("%s (%d/%d)"), *Ctx->NameOr("data.set." + Set->id + ".name", bEnglish ? Set->nameEn : Set->name), Equipped,
						static_cast<int32>(Set->pieces.size())),
					12.f, C.Quality[4], true);
				for (const abyss::SetBonusDef& Bonus : Set->bonuses)
				{
					// FIX loot Q15: the bonus text is keyed by the piece count.
					const bool bActive = Equipped >= Bonus.count;
					const FString Text = Ctx->NameOr("data.set." + Set->id + ".bonus." + std::to_string(Bonus.count), Bonus.description);
					AbyssTooltip_AddLine(Lines, Ctx, FString::Printf(TEXT("%s (%d) %s"), bActive ? TEXT("\x2713") : TEXT("\x25CB"), Bonus.count, *Text),
						11.f, bActive ? C.Good : C.Dim);
				}
			}
		}
		// comparison (loot 14)
		if (Options.bCompare && Data != nullptr && Snap != nullptr && Snap->inventory != nullptr)
		{
			const std::optional<abyss::CompareTarget> Target = abyss::FindCompareTarget(*Data, Item, Snap->inventory->Equipment());
			if (Target.has_value())
			{
				Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, 5.f, 0.f, 3.f))[AbyssUi::Divider(Ctx, Wrap, false)];
				const FString SlotName = Ctx->NameOr("ui.tooltip.slot." + std::string(abyss::EnumName(Target->slot)),
					std::string(abyss::EnumName(Target->slot)));
				AbyssTooltip_AddLine(Lines, Ctx,
					Ctx->LocArgsOrStr("ui.compare.header", TEXT("Compared with equipped ({slot})"), { FAbyssUiContext::Arg("slot", SlotName) }),
					11.f, C.Heading, true);
				if (Target->equipped == nullptr)
				{
					AbyssTooltip_AddLine(Lines, Ctx, Ctx->LocOrStr("ui.compare.emptySlot", TEXT("\x25B2 Empty slot: equipping it is an upgrade")), 11.f,
						FAbyssUiStyle::Rgb(0x7ee07e));
				}
				else
				{
					const std::vector<abyss::StatDelta> Deltas = abyss::StatDeltas(*Data, Item, Target->equipped);
					if (Deltas.empty())
					{
						AbyssTooltip_AddLine(Lines, Ctx, Ctx->LocOrStr("ui.compare.same", TEXT("Same stats as the equipped item")), 11.f, C.Muted);
					}
					for (const abyss::StatDelta& Delta : Deltas)
					{
						const bool bUp = Delta.delta > 0.0;
						const bool bPercent = Delta.key.kind == abyss::CompareKey::Kind::StatKey && Ctx->IsPercentStat(Delta.key.stat);
						const FString Value = FString::Printf(TEXT("%s%s%s"), bUp ? TEXT("+") : TEXT("\x2212"),
							*FAbyssUiContext::Num(FMath::Abs(Delta.delta)), bPercent ? TEXT("%") : TEXT(""));
						AbyssTooltip_AddLine(Lines, Ctx,
							FString::Printf(TEXT("%s %s %s"), bUp ? TEXT("\x25B2") : TEXT("\x25BC"), *Value, *AbyssTooltip_CompareLabel(*Ctx, Delta.key)),
							11.f, bUp ? FAbyssUiStyle::Rgb(0x7ee07e) : FAbyssUiStyle::Rgb(0xff6b5a));
					}
				}
			}
		}
		// price
		if (Data != nullptr)
		{
			const int64 Price = Options.PriceOverride > 0 ? Options.PriceOverride : abyss::ItemSellPrice(Item, *Data);
			Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, 5.f, 0.f, 0.f))[AbyssUi::Divider(Ctx, Wrap, false)];
			AbyssTooltip_AddLine(Lines, Ctx,
				Ctx->LocArgsOrStr("ui.tooltip.sellPrice", TEXT("Sell: {price}G"), { FAbyssUiContext::Arg("price", Price) }), 11.f,
				FAbyssUiStyle::Rgb(0xffd35a));
		}
		if (!Options.Footer.IsEmpty())
		{
			AbyssTooltip_AddLine(Lines, Ctx, Options.Footer.ToString(), 11.f, C.GoldBright, true);
		}

		return SNew(SBox)
			.WidthOverride(256.f)
			[
				SNew(SAbyssCardFrame, Ctx)
				.Accent(QualityColor)
				.Padding(FMargin(10.f, 9.f, 10.f, 10.f))
				[
					Lines
				]
			];
	}

	TSharedRef<SWidget> MakeTooltip(const TSharedRef<FAbyssUiContext>& Ctx, const abyss::ItemInstance& Item, const FOptions& Options)
	{
		TSharedRef<SWidget> Card = MakeCard(Ctx, Item, Options);
		const abyss::DataStore* Data = Ctx->GetData();
		const abyss::Snapshot* Snap = Ctx->GetSnapshot();
		// Desktop: the worn item's card on the far side (save-ui-input 7.2); touch shows only the candidate.
		if (!Ctx->IsTouch() && Options.bCompare && Data != nullptr && Snap != nullptr && Snap->inventory != nullptr)
		{
			const std::optional<abyss::CompareTarget> Target = abyss::FindCompareTarget(*Data, Item, Snap->inventory->Equipment());
			if (Target.has_value() && Target->equipped != nullptr)
			{
				FOptions WornOptions;
				WornOptions.bCompare = false;
				WornOptions.bEquippedTag = true;
				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Card]
					+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8.f, 0.f, 0.f, 0.f))[MakeCard(Ctx, *Target->equipped, WornOptions)];
			}
		}
		return Card;
	}
}

// =====================================================================================================================
// Frames
// =====================================================================================================================

void SAbyssPanelFrame::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Context = InContext;
	const TSharedRef<FAbyssUiContext> Ctx = InContext;
	const FLinearColor Accent = InArgs._Accent.A > 0.f ? InArgs._Accent : InContext->Style().Colors().Gold;
	const float Header = InArgs._HeaderHeight;
	const FVector2D Size = InArgs._Size;
	const FSimpleDelegate OnClose = InArgs._OnClose;
	const TAttribute<FText> Title = InArgs._Title;
	const float CloseSize = InContext->IsTouch() ? 42.f : 30.f;

	TSharedRef<SOverlay> Overlay = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SAbyssCanvas, Ctx, [Accent, Header](FAbyssPainter& P, const FVector2D& LocalSize)
			{
				P.Frame(FVector2D::ZeroVector, LocalSize, 0, Accent, Header);
			})
		]
		+ SOverlay::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		.Padding(FMargin(40.f, 0.f, 40.f, 0.f))
		[
			SNew(SBox)
			.HeightOverride(Header)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
				[
					SNew(SAbyssCanvas, Ctx, [](FAbyssPainter& P, const FVector2D& S)
					{
						// title flourish (8.5): curl + diamond
						const FLinearColor Gold = P.Style.Colors().Gold;
						P.Line(FVector2D(0.0, S.Y * 0.5), FVector2D(S.X - 8.0, S.Y * 0.5), FAbyssUiStyle::WithAlpha(Gold, 0.6f), 1.f);
						P.Diamond(FVector2D(S.X - 4.0, S.Y * 0.5), 6.f, Gold);
					})
					.Size(FVector2D(46.0, 12.0))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					AbyssUi::TitleLabel(*Ctx, Title, 18.f, Ctx->Style().Colors().Parchment, true, 3)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(8.f, 0.f, 0.f, 0.f))
				[
					SNew(SAbyssCanvas, Ctx, [](FAbyssPainter& P, const FVector2D& S)
					{
						const FLinearColor Gold = P.Style.Colors().Gold;
						P.Line(FVector2D(8.0, S.Y * 0.5), FVector2D(S.X, S.Y * 0.5), FAbyssUiStyle::WithAlpha(Gold, 0.6f), 1.f);
						P.Diamond(FVector2D(4.0, S.Y * 0.5), 6.f, Gold);
					})
					.Size(FVector2D(46.0, 12.0))
				]
			]
		]
		+ SOverlay::Slot()
		.Padding(InArgs._ContentPadding.IsSet() ? InArgs._ContentPadding.GetValue() : FMargin(14.f, Header + 6.f, 14.f, 12.f))
		[
			InArgs._Content.Widget
		];
	if (InArgs._ShowClose)
	{
		Overlay->AddSlot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(FMargin(0.f, (Header - CloseSize) * 0.5f + 1.f, 7.f, 0.f))
			[
				SNew(SAbyssCloseButton, Ctx)
				.Diameter(CloseSize)
				.OnClicked(OnClose)
			];
	}
	ChildSlot
	[
		SNew(SAbyssBlocker)
		[
			SNew(SBox)
			.WidthOverride(static_cast<float>(Size.X))
			.HeightOverride(static_cast<float>(Size.Y))
			[
				Overlay
			]
		]
	];
}

void SAbyssCardFrame::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	const FLinearColor Accent = InArgs._Accent.A > 0.f ? InArgs._Accent : InContext->Style().Colors().Gold;
	const int32 Kind = InArgs._Kind;
	ChildSlot
	[
		SNew(SAbyssCanvas, InContext, [Accent, Kind](FAbyssPainter& P, const FVector2D& Size)
		{
			P.Frame(FVector2D::ZeroVector, Size, Kind, Accent, 0.f, Kind == 2 ? 0.92f : 1.f);
		})
		.Padding(InArgs._Padding)
		[
			InArgs._Content.Widget
		]
	];
}
