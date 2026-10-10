#include "UI/Root/SAbyssDialogs.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "Framework/AbyssGameInstance.h"
#include "Input/AbyssInputSubsystem.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Core/AbyssUiDraw.h"
#include "UI/Widgets/AbyssUiWidgets.h"

// =====================================================================================================================
// SAbyssBackdrop
// =====================================================================================================================

void SAbyssBackdrop::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Context = InContext;
	OnPressed = InArgs._OnPressed;
	Alpha = InArgs._Alpha;
	SetVisibility(EVisibility::Visible);
}

FReply SAbyssBackdrop::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	OnPressed.ExecuteIfBound();
	return FReply::Handled();
}

FReply SAbyssBackdrop::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

FReply SAbyssBackdrop::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}

FReply SAbyssBackdrop::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	OnPressed.ExecuteIfBound();
	return FReply::Handled();
}

FReply SAbyssBackdrop::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return FReply::Handled();
}

int32 SAbyssBackdrop::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FAbyssPainter P(AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, Context->Style());
	const FVector2D Size(AllottedGeometry.GetLocalSize());
	// UiKit backdrop: rgba(0,0,0,0.25) at the centre to rgba(0,0,0,0.7) at the edges, scaled by the panel's alpha.
	const float Scale = FMath::Clamp(Alpha / 0.6f, 0.f, 1.6f);
	const float Centre = FMath::Clamp(0.25f * Scale + (Alpha > 0.6f ? (Alpha - 0.6f) : 0.f), 0.f, 0.98f);
	const float Edge = FMath::Clamp(0.7f * Scale, Centre, 0.98f);
	P.Box(FVector2D::ZeroVector, Size, FLinearColor(0.f, 0.f, 0.f, Centre));
	const FLinearColor Dark(0.f, 0.f, 0.f, Edge - Centre);
	const FLinearColor Clear(0.f, 0.f, 0.f, 0.f);
	const double BandY = Size.Y * 0.32;
	const double BandX = Size.X * 0.28;
	P.ColoredQuad(FVector2D::ZeroVector, FVector2D(Size.X, BandY), Dark, Dark, Clear, Clear);
	P.ColoredQuad(FVector2D(0.0, Size.Y - BandY), FVector2D(Size.X, BandY), Clear, Clear, Dark, Dark);
	P.ColoredQuad(FVector2D::ZeroVector, FVector2D(BandX, Size.Y), Dark, Clear, Clear, Dark);
	P.ColoredQuad(FVector2D(Size.X - BandX, 0.0), FVector2D(BandX, Size.Y), Clear, Dark, Dark, Clear);
	return P.Layer;
}

// =====================================================================================================================
// SAbyssConfirmDialog
// =====================================================================================================================

void SAbyssConfirmDialog::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const FAbyssConfirmRequest& Request)
{
	OnResult = InArgs._OnResult;
	const TSharedRef<FAbyssUiContext> Ctx = InContext;
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const bool bTouch = Ctx->IsTouch();
	const float Width = bTouch ? 460.f : 320.f;
	const float ButtonH = bTouch ? 52.f : 30.f;
	const float FontPx = bTouch ? 18.f : 13.f;

	const FText ConfirmLabel = Request.ConfirmLabel.IsEmpty() ? Ctx->LocOr("ui.shop.confirm", TEXT("Confirm")) : Request.ConfirmLabel;
	const FText CancelLabel = Request.CancelLabel.IsEmpty() ? Ctx->LocOr("ui.shop.cancel", TEXT("Cancel")) : Request.CancelLabel;
	const FLinearColor BodyColor = Request.BodyColor == FLinearColor::White ? C.Text : Request.BodyColor;

	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	Buttons->AddSlot()
		.AutoWidth()
		.Padding(FMargin(6.f, 0.f))
		[
			SNew(SAbyssButton, Ctx)
			.Text(ConfirmLabel)
			.Kind(Request.bDanger ? EAbyssButtonKind::Danger : EAbyssButtonKind::Primary)
			.FontPx(FontPx)
			.Width(bTouch ? 180.f : 118.f)
			.Height(ButtonH)
			.OnClicked_Lambda([this]() { OnResult.ExecuteIfBound(true); })
		];
	if (!Request.bNoCancel)
	{
		Buttons->AddSlot()
			.AutoWidth()
			.Padding(FMargin(6.f, 0.f))
			[
				SNew(SAbyssButton, Ctx)
				.Text(CancelLabel)
				.Kind(EAbyssButtonKind::Secondary)
				.FontPx(FontPx)
				.Width(bTouch ? 180.f : 118.f)
				.Height(ButtonH)
				.OnClicked_Lambda([this]() { OnResult.ExecuteIfBound(false); })
			];
	}

	TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
	if (!Request.Title.IsEmpty())
	{
		Lines->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 0.f, 0.f, 8.f))
			[
				AbyssUi::TitleLabel(*Ctx, Request.Title, bTouch ? 22.f : 17.f, C.Parchment, true, 2)
			];
	}
	if (!Request.Body.IsEmpty())
	{
		TSharedRef<STextBlock> Body = AbyssUi::Label(*Ctx, Request.Body, bTouch ? 17.f : 13.f, BodyColor, false, 1, Width - 40.f);
		Body->SetJustification(ETextJustify::Center);
		Lines->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 0.f, 0.f, 14.f))
			[
				Body
			];
	}
	Lines->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			Buttons
		];

	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SAbyssBlocker)
		[
			SNew(SBox)
			.WidthOverride(Width)
			[
				SNew(SAbyssCardFrame, Ctx)
				.Kind(1)
				.Accent(Request.bDanger ? FAbyssUiStyle::Rgb(0xc0503c) : C.Gold)
				.Padding(FMargin(20.f, 16.f, 20.f, 16.f))
				[
					Lines
				]
			]
		]
	];
}

// =====================================================================================================================
// SAbyssErrorDialog
// =====================================================================================================================

void SAbyssErrorDialog::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	const TSharedRef<FAbyssUiContext> Ctx = InContext;
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const FSimpleDelegate OnDismiss = InArgs._OnDismiss;
	const bool bTouch = Ctx->IsTouch();

	TSharedRef<SScrollBox> MessageScroll = AbyssUi::ScrollBox(*Ctx);
	MessageScroll->AddSlot()
	[
		AbyssUi::Label(*Ctx, InArgs._Message, 13.f, C.Text, false, 1, 500.f)
	];

	// An unrecoverable error (data tables) offers Quit where the platform allows it (not on iOS); everything else is
	// acknowledged with OK.
	const bool bOfferQuit = !PLATFORM_IOS && InArgs._ShowQuit;
	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	if (bOfferQuit)
	{
		Buttons->AddSlot()
			.AutoWidth()
			.Padding(FMargin(6.f, 0.f))
			[
				SNew(SAbyssButton, Ctx)
				.Text(Ctx->LocOr("menu.quit", TEXT("Quit")))
				.Kind(EAbyssButtonKind::Danger)
				.FontPx(bTouch ? 18.f : 13.f)
				.Width(150.f)
				.Height(bTouch ? 52.f : 32.f)
				.OnClicked_Lambda([Ctx]()
				{
					if (UAbyssGameInstance* GameInstance = Ctx->GetGameInstance())
					{
						GameInstance->QuitGame();
					}
				})
			];
	}
	else
	{
		Buttons->AddSlot()
			.AutoWidth()
			.Padding(FMargin(6.f, 0.f))
			[
				SNew(SAbyssButton, Ctx)
				.Text(Ctx->LocOr("ui.error.ok", TEXT("OK")))
				.Kind(EAbyssButtonKind::Primary)
				.FontPx(bTouch ? 18.f : 13.f)
				.Width(150.f)
				.Height(bTouch ? 52.f : 32.f)
				.OnClicked_Lambda([OnDismiss]() { OnDismiss.ExecuteIfBound(); })
			];
	}

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SAbyssBackdrop, Ctx).Alpha(0.8f)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SAbyssBlocker)
			[
				SNew(SBox)
				.WidthOverride(560.f)
				[
					SNew(SAbyssCardFrame, Ctx)
					.Kind(1)
					.Accent(FAbyssUiStyle::Rgb(0xc0503c))
					.Padding(FMargin(22.f, 18.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						.Padding(FMargin(0.f, 0.f, 0.f, 10.f))
						[
							AbyssUi::TitleLabel(*Ctx, InArgs._Title, 18.f, FAbyssUiStyle::Rgb(0xff8a72), true, 2)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.f, 0.f, 0.f, 16.f))
						[
							SNew(SBox)
							.MaxDesiredHeight(360.f)
							[
								MessageScroll
							]
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						[
							Buttons
						]
					]
				]
			]
		]
	];
}

// =====================================================================================================================
// SAbyssHelpSheet
// =====================================================================================================================

namespace
{
	struct FAbyssHelpRow
	{
		const char* DescKey;
		const TCHAR* DescFallback;
		EAbyssInputAction First;
		EAbyssInputAction Last;  // == First for a single binding
	};

	FString AbyssHelp_KeyText(const UAbyssInputSubsystem* Input, EAbyssInputAction First, EAbyssInputAction Last, EAbyssInputDevice Device)
	{
		if (Input == nullptr)
		{
			return FString();
		}
		const FString A = Input->GetKeyHint(First, Device);
		if (First == Last)
		{
			return A;
		}
		const FString B = Input->GetKeyHint(Last, Device);
		if (A.IsEmpty() || B.IsEmpty())
		{
			return A.IsEmpty() ? B : A;
		}
		return FString::Printf(TEXT("%s - %s"), *A, *B);
	}

	TSharedRef<SWidget> AbyssHelp_KeyCap(const TSharedRef<FAbyssUiContext>& Ctx, const FString& Text, float Width)
	{
		const FString Shown = Text.IsEmpty() ? FString(TEXT("\x2014")) : Text;
		return SNew(SBox)
			.WidthOverride(Width)
			.HAlign(HAlign_Center)
			[
				SNew(SAbyssCanvas, Ctx, [](FAbyssPainter& P, const FVector2D& Size)
				{
					P.Well(FVector2D::ZeroVector, Size, 4.f);
				})
				.Padding(FMargin(6.f, 2.f))
				[
					AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Shown), 12.f, Ctx->Style().Colors().GoldBright, true, 1)
				]
			];
	}
}

void SAbyssHelpSheet::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	const TSharedRef<FAbyssUiContext> Ctx = InContext;
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const FSimpleDelegate OnClose = InArgs._OnClose;
	const UAbyssGameInstance* GameInstance = Ctx->GetGameInstance();
	const UAbyssInputSubsystem* Input = GameInstance ? GameInstance->GetSubsystem<UAbyssInputSubsystem>() : nullptr;

	using A = EAbyssInputAction;
	static const FAbyssHelpRow Movement[] = {
		{ "menu.helpPanel.movement.wasd", TEXT("Move"), A::Move, A::Move },
		{ "menu.helpPanel.movement.mouse", TEXT("Click to move / attack / interact"), A::Click, A::Click },
		{ "menu.helpPanel.movement.interact", TEXT("Talk / use"), A::Interact, A::Interact },
	};
	static const FAbyssHelpRow Combat[] = {
		{ "menu.helpPanel.combat.skills", TEXT("Use skills"), A::Skill1, A::Skill6 },
		{ "menu.helpPanel.combat.dodge", TEXT("Dodge with invulnerability"), A::Dodge, A::Dodge },
		{ "menu.helpPanel.combat.target", TEXT("Cycle target lock"), A::TargetCycle, A::TargetCycle },
		{ "menu.helpPanel.combat.autoCombat", TEXT("Toggle auto-combat"), A::ToggleAutoCombat, A::ToggleAutoCombat },
		{ "menu.helpPanel.combat.autoLoot", TEXT("Cycle auto-loot"), A::CycleAutoLoot, A::CycleAutoLoot },
		{ "menu.helpPanel.combat.potions", TEXT("Drink a potion"), A::PotionHp, A::PotionMp },
		{ "menu.helpPanel.combat.teleport", TEXT("Teleport to camp"), A::TownPortal, A::TownPortal },
	};
	static const FAbyssHelpRow UiRows[] = {
		{ "menu.helpPanel.ui.inventory", TEXT("Inventory"), A::PanelInventory, A::PanelInventory },
		{ "menu.helpPanel.ui.character", TEXT("Character"), A::PanelCharacter, A::PanelCharacter },
		{ "menu.helpPanel.ui.skillTree", TEXT("Skill tree"), A::PanelSkills, A::PanelSkills },
		{ "menu.helpPanel.ui.questLog", TEXT("Quest log"), A::PanelQuestLog, A::PanelQuestLog },
		{ "menu.helpPanel.ui.map", TEXT("Map"), A::PanelWorldMap, A::PanelWorldMap },
		{ "menu.helpPanel.ui.achievements", TEXT("Achievements"), A::PanelAchievements, A::PanelAchievements },
		{ "menu.helpPanel.ui.pets", TEXT("Ley-beasts"), A::PanelPets, A::PanelPets },
		{ "menu.helpPanel.ui.settings", TEXT("Settings"), A::PanelSettings, A::PanelSettings },
		{ "menu.helpPanel.ui.menu", TEXT("Close panel / system menu"), A::Back, A::Back },
	};

	const float KeyW = 112.f;
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	// column header: action | keyboard & mouse | gamepad
	Rows->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(KeyW).HAlign(HAlign_Center)
				[
					AbyssUi::Label(*Ctx, Ctx->LocOr("menu.helpPanel.col.keyboard", TEXT("Keyboard")), 11.f, C.Muted, true, 1)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8.f, 0.f, 0.f, 0.f))
			[
				SNew(SBox).WidthOverride(KeyW).HAlign(HAlign_Center)
				[
					AbyssUi::Label(*Ctx, Ctx->LocOr("menu.helpPanel.col.gamepad", TEXT("Gamepad")), 11.f, C.Muted, true, 1)
				]
			]
		];
	const auto AddCategory = [&Rows, &Ctx, Input, KeyW](const char* TitleKey, const TCHAR* TitleFallback, const FAbyssHelpRow* Items, int32 Count)
	{
		Rows->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 8.f, 0.f, 4.f))
			[
				AbyssUi::SectionHeader(Ctx, Ctx->LocOr(TitleKey, TitleFallback), 470.f)
			];
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FAbyssHelpRow& Row = Items[Index];
			Rows->AddSlot()
				.AutoHeight()
				.Padding(FMargin(0.f, 2.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						AbyssUi::Label(*Ctx, Ctx->LocOr(Row.DescKey, Row.DescFallback), 12.f, Ctx->Style().Colors().Text, false, 1, 230.f)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						AbyssHelp_KeyCap(Ctx, AbyssHelp_KeyText(Input, Row.First, Row.Last, EAbyssInputDevice::KeyboardMouse), KeyW)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(8.f, 0.f, 0.f, 0.f))
					[
						AbyssHelp_KeyCap(Ctx, AbyssHelp_KeyText(Input, Row.First, Row.Last, EAbyssInputDevice::Gamepad), KeyW)
					]
				];
		}
	};
	AddCategory("menu.helpPanel.cat.movement", TEXT("Movement"), Movement, UE_ARRAY_COUNT(Movement));
	AddCategory("menu.helpPanel.cat.combat", TEXT("Combat"), Combat, UE_ARRAY_COUNT(Combat));
	AddCategory("menu.helpPanel.cat.ui", TEXT("Interface"), UiRows, UE_ARRAY_COUNT(UiRows));
	Rows->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 10.f, 0.f, 0.f))
		[
			AbyssUi::Label(*Ctx, Ctx->LocOr("menu.helpPanel.touchNote",
				TEXT("Touch: joystick to move, the skill fan to attack, the top row opens panels.")), 11.f, C.Muted, false, 1, 470.f)
		];

	TSharedRef<SScrollBox> Scroll = AbyssUi::ScrollBox(*Ctx);
	Scroll->AddSlot()[Rows];

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SAbyssBackdrop, Ctx)
			.Alpha(0.72f)
			.OnPressed(OnClose)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SAbyssPanelFrame, Ctx)
			.Size(FVector2D(540.0, 560.0))
			.Title(Ctx->LocOr("menu.helpPanel.title", TEXT("Controls")))
			.OnClose(OnClose)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					Scroll
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
				[
					SNew(SAbyssButton, Ctx)
					.Text(Ctx->LocOr("menu.backShort", TEXT("Back")))
					.Kind(EAbyssButtonKind::Ghost)
					.FontPx(13.f)
					.Width(140.f)
					.Height(Ctx->IsTouch() ? 48.f : 32.f)
					.OnClicked(OnClose)
				]
			]
		]
	];
}

// =====================================================================================================================
// SAbyssCreditsSheet
// =====================================================================================================================

void SAbyssCreditsSheet::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	const TSharedRef<FAbyssUiContext> Ctx = InContext;
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const FSimpleDelegate OnClose = InArgs._OnClose;

	TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
	const auto AddBlock = [&Lines, &Ctx, &C](const FText& Heading, const TArray<FText>& Body)
	{
		Lines->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 10.f, 0.f, 3.f))
			[
				AbyssUi::TitleLabel(*Ctx, Heading, 15.f, C.Heading, true, 2)
			];
		for (const FText& Line : Body)
		{
			TSharedRef<STextBlock> Text = AbyssUi::Label(*Ctx, Line, 12.f, C.Text, false, 1, 440.f);
			Text->SetJustification(ETextJustify::Center);
			Lines->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 1.f))[Text];
		}
	};
	// Proper names and licence titles are not translated; headings come from the string tables.
	AddBlock(Ctx->LocOr("menu.creditsPanel.engine", TEXT("Game Engine")),
		{ FText::AsCultureInvariant(TEXT("Unreal\x00AE Engine")),
			Ctx->LocOr("menu.creditsPanel.engineNotice",
				TEXT("Unreal\x00AE is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere.")) });
	AddBlock(Ctx->LocOr("menu.creditsPanel.fonts", TEXT("Fonts")),
		{ FText::AsCultureInvariant(TEXT("Noto Sans SC / TC, Noto Serif SC / TC \x00B7 SIL Open Font License 1.1")),
			FText::AsCultureInvariant(TEXT("Cinzel \x00B7 SIL Open Font License 1.1")) });
	AddBlock(Ctx->LocOr("menu.creditsPanel.art", TEXT("Art")),
		{ Ctx->LocOr("menu.creditsPanel.artPipeline", TEXT("Characters, props, icons and portraits built with the Blender pipeline")) });
	AddBlock(Ctx->LocOr("menu.creditsPanel.audio", TEXT("Music & Sound")),
		{ Ctx->LocOr("menu.creditsPanel.audioPipeline", TEXT("Procedural scores and effects rendered by the Abyssfire synthesiser")) });
	AddBlock(Ctx->LocOr("menu.creditsPanel.design", TEXT("Design & Development")),
		{ FText::AsCultureInvariant(TEXT("Abyssfire")) });

	TSharedRef<SScrollBox> Scroll = AbyssUi::ScrollBox(*Ctx);
	Scroll->AddSlot()[Lines];

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SAbyssBackdrop, Ctx)
			.Alpha(0.72f)
			.OnPressed(OnClose)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SAbyssPanelFrame, Ctx)
			.Size(FVector2D(500.0, 560.0))
			.Title(Ctx->LocOr("menu.creditsPanel.title", TEXT("Credits")))
			.OnClose(OnClose)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					Scroll
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
				[
					SNew(SAbyssButton, Ctx)
					.Text(Ctx->LocOr("menu.backShort", TEXT("Back")))
					.Kind(EAbyssButtonKind::Ghost)
					.FontPx(13.f)
					.Width(140.f)
					.Height(Ctx->IsTouch() ? 48.f : 32.f)
					.OnClicked(OnClose)
				]
			]
		]
	];
}
