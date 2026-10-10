#include "UI/Menu/SAbyssMainMenu.h"

#include "Layout/Clipping.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include <span>
#include <string>

#include "abyss/data/DataStore.h"
#include "abyss/sim/Session.h"

#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssText.h"
#include "Platform/AbyssSettings.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Core/AbyssUiDraw.h"
#include "UI/Menu/SAbyssMenuBackground.h"
#include "UI/Root/SAbyssDialogs.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	constexpr abyss::ClassId GAbyssMenuClasses[3] = { abyss::ClassId::Warrior, abyss::ClassId::Mage, abyss::ClassId::Rogue };
	constexpr uint32 GAbyssMenuClassColors[3] = { 0xd0473a, 0x9b59d6, 0x3fb86a };
	constexpr uint32 GAbyssMenuClassText[3] = { 0xff8a72, 0xd0a0ff, 0x8ff0a8 };
	constexpr abyss::Difficulty GAbyssMenuDifficulties[3] = { abyss::Difficulty::Normal, abyss::Difficulty::Nightmare, abyss::Difficulty::Hell };
	constexpr uint32 GAbyssMenuDifficultyAccent[3] = { 0x4a8c4a, 0xc0392b, 0x8b0000 };
	constexpr uint32 GAbyssMenuDifficultyLabel[3] = { 0x4ade80, 0xef4444, 0xff4444 };
	constexpr int32 GAbyssMenuSlotCount = 3;

	int32 AbyssMenu_ClassIndex(abyss::ClassId Class)
	{
		return FMath::Clamp(static_cast<int32>(Class), 0, 2);
	}

	/** Title gradient (#fff3c4 -> #f0b84e @0.45 -> #c46a1c @0.7 -> #7a2a0c) sampled at T. */
	FLinearColor AbyssMenu_TitleGradient(float T)
	{
		struct FStop
		{
			float At;
			uint32 Rgb;
		};
		static const FStop Stops[4] = { { 0.f, 0xfff3c4 }, { 0.45f, 0xf0b84e }, { 0.7f, 0xc46a1c }, { 1.f, 0x7a2a0c } };
		for (int32 Index = 1; Index < 4; ++Index)
		{
			if (T <= Stops[Index].At)
			{
				const float Span = Stops[Index].At - Stops[Index - 1].At;
				const float Local = Span > 0.f ? (T - Stops[Index - 1].At) / Span : 0.f;
				return FMath::Lerp(FAbyssUiStyle::Rgb(Stops[Index - 1].Rgb), FAbyssUiStyle::Rgb(Stops[Index].Rgb), Local);
			}
		}
		return FAbyssUiStyle::Rgb(Stops[3].Rgb);
	}

	/**
	 * The ABYSSFIRE logo: dark stroke pass, then the fill in horizontal bands, each clipped to its slice and tinted with
	 * the gradient (gradient text without a font material, ue58-platform.md 9.1).
	 */
	void AbyssMenu_PaintLogo(FAbyssPainter& P, const FVector2D& Center, const FString& Text, float Px, float Scale)
	{
		const FAbyssUiStyle& UiStyle = P.Style;
		const FLinearColor StrokeColor = FAbyssUiStyle::Rgb(0x2a1606);
		const FSlateFontInfo Stroke = UiStyle.Font(EAbyssFontFace::Title, Px, true, 6, StrokeColor);
		P.TextCentered(Center + FVector2D(0.0, 5.0), Text, Stroke, FLinearColor(0.f, 0.f, 0.f, 0.5f), Scale);
		P.TextCentered(Center, Text, Stroke, StrokeColor, Scale);
		const FSlateFontInfo Fill = UiStyle.Font(EAbyssFontFace::Title, Px, true, 6, FLinearColor::Transparent);
		const FVector2D TextSize = FAbyssPainter::Measure(Text, Fill) * Scale;
		const double Top = Center.Y - TextSize.Y * 0.5;
		const double Left = Center.X - TextSize.X * 0.5 - 8.0;
		constexpr int32 Bands = 16;
		const double BandH = TextSize.Y / Bands;
		for (int32 Band = 0; Band < Bands; ++Band)
		{
			const FGeometry BandGeometry = P.Geometry.MakeChild(FVector2f(static_cast<float>(TextSize.X + 16.0), static_cast<float>(BandH + 0.6)),
				FSlateLayoutTransform(FVector2f(static_cast<float>(Left), static_cast<float>(Top + Band * BandH))));
			P.Elements.PushClip(FSlateClippingZone(BandGeometry));
			P.TextCentered(Center, Text, Fill, AbyssMenu_TitleGradient((Band + 0.5f) / Bands), Scale);
			P.Elements.PopClip();
		}
	}

	/** Title flourish (46 x 12 curl + diamond), pointing right (bMirror: left). */
	void AbyssMenu_Flourish(FAbyssPainter& P, const FVector2D& Anchor, bool bMirror)
	{
		const FLinearColor Gold = P.Style.Colors().Gold;
		const double Dir = bMirror ? -1.0 : 1.0;
		P.Line(Anchor, Anchor + FVector2D(38.0 * Dir, 0.0), FAbyssUiStyle::WithAlpha(Gold, 0.6f), 1.f);
		P.Diamond(Anchor + FVector2D(42.0 * Dir, 0.0), 7.f, Gold);
		P.CircleOutline(Anchor + FVector2D(10.0 * Dir, -4.0), 4.f, FAbyssUiStyle::WithAlpha(Gold, 0.45f), 1.f, 16);
	}

	FString AbyssMenu_PlayTime(double PlayTimeMs)
	{
		const int64 Total = static_cast<int64>(FMath::Max(0.0, PlayTimeMs) / 1000.0);
		return FString::Printf(TEXT("%lld:%02lld:%02lld"), static_cast<long long>(Total / 3600), static_cast<long long>((Total / 60) % 60),
			static_cast<long long>(Total % 60));
	}

	FString AbyssMenu_SavedAt(int64 UnixMs)
	{
		if (UnixMs <= 0)
		{
			return FString();
		}
		const FDateTime Utc = FDateTime::FromUnixTimestamp(UnixMs / 1000);
		const FDateTime Local = Utc + (FDateTime::Now() - FDateTime::UtcNow());
		return Local.ToString(TEXT("%Y-%m-%d %H:%M"));
	}

	TSharedRef<SWidget> AbyssMenu_Text(const TSharedRef<FAbyssUiContext>& Ctx, const FText& Text, float Px, const FLinearColor& Color,
		bool bBold = false, float Wrap = 0.f, ETextJustify::Type Justify = ETextJustify::Left)
	{
		TSharedRef<STextBlock> Block = AbyssUi::Label(*Ctx, Text, Px, Color, bBold, 2, Wrap);
		Block->SetJustification(Justify);
		return Block;
	}
}

// =====================================================================================================================
// Construction
// =====================================================================================================================

void SAbyssMainMenu::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Ctx = InContext;
	GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
	const TSharedRef<FAbyssUiContext> Context = InContext;

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SAbyssMenuBackground, Context)
		]
		+ SOverlay::Slot()
		[
			SNew(SSafeZone)
			.IsTitleSafe(false)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				.StretchDirection(EStretchDirection::DownOnly)
				[
					SNew(SBox)
					.WidthOverride(1280.f)
					.HeightOverride(720.f)
					[
						SNew(SOverlay)
						// logo block (y 60 .. 230) and the version line
						+ SOverlay::Slot()
						[
							SNew(SAbyssCanvas, Context, [this](FAbyssPainter& P, const FVector2D& Size)
							{
								const double T = Ctx->Now();
								const FAbyssUiPalette& C = P.Style.Colors();
								P.Divider(FVector2D(640.0, 78.0), 440.f, true);
								// breathing scale 1 <-> 1.015 (3000 ms yoyo)
								const float Breath = 1.f + 0.0075f * (1.f - FMath::Cos(static_cast<float>(T * UE_TWO_PI / 6.0)));
								AbyssMenu_PaintLogo(P, FVector2D(640.0, 140.0), Ctx->LocStr("menu.title"), 58.f, Breath);
								const FString Subtitle = Ctx->LocStr("menu.subtitle");
								const FSlateFontInfo SubFont = P.Style.Font(EAbyssFontFace::Title, 30.f, true, 2, C.Ink);
								const FVector2D SubSize = FAbyssPainter::Measure(Subtitle, SubFont);
								P.TextCentered(FVector2D(640.0, 190.0), Subtitle, SubFont, C.Heading);
								AbyssMenu_Flourish(P, FVector2D(640.0 + SubSize.X * 0.5 + 10.0, 190.0), false);
								AbyssMenu_Flourish(P, FVector2D(640.0 - SubSize.X * 0.5 - 10.0, 190.0), true);
								P.Divider(FVector2D(640.0, 222.0), 360.f, false);
								if (!Version.IsEmpty())
								{
									P.TextCentered(FVector2D(640.0, 702.0), FString(TEXT("v")) + Version,
										P.Style.Font(EAbyssFontFace::Title, 12.f, false, 1), FAbyssUiStyle::Rgb(0x6a5a48));
								}
							})
							.Visibility(EVisibility::HitTestInvisible)
						]
						+ SOverlay::Slot()
						[
							SAssignNew(ScreenHost, SBox)
						]
					]
				]
			]
		]
		+ SOverlay::Slot()
		[
			SAssignNew(ModalHost, SBox)
			.Visibility(EVisibility::Collapsed)
		]
	];
}

void SAbyssMainMenu::Activate()
{
	CloseCredits();
	RefreshSlots();
	ShowScreen(EScreen::Title);
}

void SAbyssMainMenu::RefreshLocale()
{
	if (bCreditsOpen)
	{
		ShowCredits();
	}
	RebuildScreen();
}

bool SAbyssMainMenu::HandleBack()
{
	if (bCreditsOpen)
	{
		CloseCredits();
		return true;
	}
	if (Screen != EScreen::Title)
	{
		RefreshSlots();
		ShowScreen(EScreen::Title);
		return true;
	}
	return false;
}

void SAbyssMainMenu::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (!ScreenHost.IsValid())
	{
		return;
	}
	// screen entrance: alpha 0 -> 1 and y +24 -> 0 over 360 ms (Cubic.easeOut)
	const float T = AbyssEase::Progress(Ctx->Now(), ScreenStart, 0.36);
	const float Eased = AbyssEase::CubicOut(T);
	ScreenHost->SetRenderOpacity(Eased);
	ScreenHost->SetRenderTransform(T >= 1.f ? TOptional<FSlateRenderTransform>()
		: TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(0.f, 24.f * (1.f - Eased)))));
}

void SAbyssMainMenu::ShowScreen(EScreen InScreen)
{
	Screen = InScreen;
	ScreenStart = Ctx->Now();
	RebuildScreen();
}

void SAbyssMainMenu::RebuildScreen()
{
	Ctx->HideTooltip(nullptr);
	switch (Screen)
	{
	case EScreen::Title: ScreenHost->SetContent(BuildTitleScreen()); break;
	case EScreen::ClassSelect: ScreenHost->SetContent(BuildClassSelect()); break;
	case EScreen::Difficulty: ScreenHost->SetContent(BuildDifficulty()); break;
	case EScreen::Language: ScreenHost->SetContent(BuildLanguage()); break;
	}
}

void SAbyssMainMenu::RefreshSlots()
{
	Slots.clear();
	if (UAbyssGameInstance* GameInstance = Ctx->GetGameInstance())
	{
		GameInstance->ListSlots(Slots);
	}
}

const abyss::SaveSlotInfo* SAbyssMainMenu::FindSlot(int32 SlotIndex) const
{
	for (const abyss::SaveSlotInfo& Info : Slots)
	{
		if (Info.slot == SlotIndex)
		{
			return Info.exists ? &Info : nullptr;
		}
	}
	return nullptr;
}

// =====================================================================================================================
// Title screen
// =====================================================================================================================

TSharedRef<SWidget> SAbyssMainMenu::BuildTitleScreen()
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const bool bTouch = Ctx->IsTouch();
	TSharedRef<SCanvas> Canvas = SNew(SCanvas);

	// U1: three character slots, 372 x 172, gap 18, centred
	constexpr double CardW = 372.0;
	constexpr double CardH = 172.0;
	constexpr double Gap = 18.0;
	const double Left = (1280.0 - (CardW * 3.0 + Gap * 2.0)) * 0.5;
	for (int32 SlotIndex = 0; SlotIndex < GAbyssMenuSlotCount; ++SlotIndex)
	{
		Canvas->AddSlot()
			.Position(FVector2D(Left + SlotIndex * (CardW + Gap), 262.0))
			.Size(FVector2D(CardW, CardH))
			[
				BuildSlotCard(SlotIndex)
			];
	}

	// menu buttons (ghost, 1.2): settings, controls, language, credits, quit
	struct FMenuButton
	{
		FText Label;
		TFunction<void()> Action;
	};
	TArray<FMenuButton> Buttons;
	Buttons.Add({ Ctx->LocOr("menu.settings", TEXT("Settings")), [Context]()
	{
		if (IAbyssUiHost* Host = Context->GetHost())
		{
			Host->OpenPanel(abyss::PanelId::Settings);
		}
	} });
	Buttons.Add({ Ctx->LocOr("menu.help", TEXT("Controls")), [Context]()
	{
		if (IAbyssUiHost* Host = Context->GetHost())
		{
			Host->ShowHelp();
		}
	} });
	Buttons.Add({ Ctx->LocOr("menu.language", TEXT("Language")), [this]() { ShowScreen(EScreen::Language); } });
	Buttons.Add({ Ctx->LocOr("menu.credits", TEXT("Credits")), [this]() { ShowCredits(); } });
#if !PLATFORM_IOS
	Buttons.Add({ Ctx->LocOr("menu.quit", TEXT("Quit")), [Context]()
	{
		if (UAbyssGameInstance* GameInstance = Context->GetGameInstance())
		{
			GameInstance->QuitGame();
		}
	} });
#endif
	const double ButtonW = bTouch ? 210.0 : 196.0;
	const double ButtonH = bTouch ? 54.0 : 40.0;
	const double ButtonGap = 12.0;
	const double RowW = Buttons.Num() * ButtonW + (Buttons.Num() - 1) * ButtonGap;
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		TFunction<void()> Action = Buttons[Index].Action;
		Canvas->AddSlot()
			.Position(FVector2D((1280.0 - RowW) * 0.5 + Index * (ButtonW + ButtonGap), 470.0))
			.Size(FVector2D(ButtonW, ButtonH))
			[
				SNew(SAbyssButton, Context)
				.Text(Buttons[Index].Label)
				.Kind(EAbyssButtonKind::Ghost)
				.FontPx(bTouch ? 18.f : 15.f)
				.Width(static_cast<float>(ButtonW))
				.Height(static_cast<float>(ButtonH))
				.OnClicked_Lambda([Action]()
				{
					if (Action)
					{
						Action();
					}
				})
			];
	}
	return Canvas;
}

TSharedRef<SWidget> SAbyssMainMenu::BuildSlotCard(int32 SlotIndex)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const bool bTouch = Ctx->IsTouch();
	const abyss::SaveSlotInfo* Info = FindSlot(SlotIndex);
	const bool bExists = Info != nullptr;
	const FString SlotLabel = Ctx->LocArgsOrStr("menu.slot.label", TEXT("Slot {n}"), { FAbyssUiContext::Arg("n", SlotIndex + 1) });

	TSharedPtr<SButton> CardButton;
	TSharedRef<SOverlay> Content = SNew(SOverlay);

	// card body (tooltip frame, accent 0xffd98a; hover fill 0xffc860 a 0.08, frame tint 0xfff0d0)
	TSharedRef<SWidget> Card = SAssignNew(CardButton, SButton)
		.ButtonStyle(&Ctx->Style().InvisibleButton())
		.ContentPadding(FMargin(0.f))
		.IsFocusable(false)
		.ClickMethod(bTouch ? EButtonClickMethod::DownAndUp : EButtonClickMethod::MouseDown)
		.TouchMethod(EButtonTouchMethod::PreciseTap)
		.OnClicked_Lambda([this, SlotIndex, bExists]()
		{
			Ctx->PlaySound(abyss::SfxId::Click);
			if (bExists)
			{
				OnSlotContinue(SlotIndex);
			}
			else
			{
				OnSlotNew(SlotIndex);
			}
			return FReply::Handled();
		})
		[
			Content
		];
	const TWeakPtr<SButton> WeakCard = CardButton;
	Content->AddSlot()
	[
		SNew(SAbyssCanvas, Context, [WeakCard, bExists](FAbyssPainter& P, const FVector2D& Size)
		{
			const TSharedPtr<SButton> Pinned = WeakCard.Pin();
			const bool bHovered = Pinned.IsValid() && Pinned->IsHovered();
			const FLinearColor Accent = bHovered ? FAbyssUiStyle::Rgb(0xfff0d0) : (bExists ? FAbyssUiStyle::Rgb(0xffd98a) : P.Style.Colors().Gold);
			P.Frame(FVector2D::ZeroVector, Size, 1, Accent);
			if (bHovered)
			{
				P.RoundBox(FVector2D(3.0, 3.0), Size - FVector2D(6.0, 6.0), FAbyssUiStyle::Rgb(0xffc860, 0.08f), 4.f);
			}
		})
	];

	if (bExists)
	{
		const int32 ClassIndex = AbyssMenu_ClassIndex(Info->classId);
		const abyss::ClassId Class = Info->classId;
		const FString ClassName = Ctx->ClassName(Info->classId);
		const FText Line1 = Ctx->LocArgsOr("menu.continue", TEXT("Continue - {class} Lv.{level}"),
			{ FAbyssUiContext::Arg("class", ClassName), FAbyssUiContext::Arg("level", Info->level) });
		const FString Zone = Info->mapId.empty() ? FString() : Ctx->ZoneName(Info->mapId);
		const FText Line2 = FText::AsCultureInvariant(FString::Printf(TEXT("%s  \x00B7  %s"), *Zone, *Ctx->DifficultyName(Info->difficulty)));
		FString Line3 = Ctx->LocArgsOrStr("menu.slot.playTime", TEXT("Played {time}"), { FAbyssUiContext::Arg("time", AbyssMenu_PlayTime(Info->playTimeMs)) });
		const FString Saved = AbyssMenu_SavedAt(Info->timestamp);
		if (!Saved.IsEmpty())
		{
			Line3 += TEXT("   ") + Saved;
		}
		Content->AddSlot()
		.Padding(FMargin(14.f, 12.f, 14.f, 12.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			.Padding(FMargin(0.f, 6.f, 12.f, 0.f))
			[
				// round portrait well r 33 (class art; fallback: the class colour and initial)
				SNew(SAbyssCanvas, Context, [Context, Class, ClassIndex, ClassName](FAbyssPainter& P, const FVector2D& Size)
				{
					const FVector2D Center = Size * 0.5;
					P.Circle(Center, 36.f, FAbyssUiStyle::Rgb(0x0c0b0e), P.Style.Colors().Gold, 1.5f);
					P.Glow(Center, 34.f, FAbyssUiStyle::Rgb(GAbyssMenuClassColors[ClassIndex], 0.35f), 6);
					if (const FSlateBrush* Portrait = Context->HeroPortrait(Class))
					{
						P.Brush(Center - FVector2D(33.0, 33.0), FVector2D(66.0, 66.0), Portrait);
					}
					else
					{
						P.TextCentered(Center, ClassName.Left(1), P.Style.Font(EAbyssFontFace::Title, 30.f, true, 2),
							FAbyssUiStyle::Rgb(GAbyssMenuClassText[ClassIndex]));
					}
				})
				.Size(FVector2D(76.0, 76.0))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					AbyssMenu_Text(Context, FText::AsCultureInvariant(SlotLabel), 11.f, C.Muted, true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))
				[
					AbyssMenu_Text(Context, Line1, 16.f, C.Parchment, true, 240.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 0.f))
				[
					AbyssMenu_Text(Context, Line2, 13.f, C.TextSoft, false, 240.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 0.f))
				[
					AbyssMenu_Text(Context, FText::AsCultureInvariant(Line3), 11.f, C.Muted, false, 240.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 0.f))
				[
					AbyssMenu_Text(Context, Ctx->LocOr("menu.continueSubtitle", TEXT("Continue your adventure")), 12.f, C.Heading, false, 240.f)
				]
			]
		];
	}
	else
	{
		Content->AddSlot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 0.f, 36.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				AbyssMenu_Text(Context, FText::AsCultureInvariant(SlotLabel), 12.f, C.Muted, true)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 6.f, 0.f, 0.f))
			[
				AbyssMenu_Text(Context, Ctx->LocOr("menu.slot.empty", TEXT("Empty slot")), 16.f, C.Dim, true)
			]
		];
	}

	// action buttons along the bottom edge (siblings above the card: they claim their own presses)
	TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox);
	const float ActionH = bTouch ? 44.f : 30.f;
	if (bExists)
	{
		Actions->AddSlot()
			.AutoWidth()
			.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("menu.slot.continue", TEXT("Continue")))
				.Kind(EAbyssButtonKind::Primary)
				.FontPx(bTouch ? 16.f : 13.f)
				.Width(bTouch ? 150.f : 132.f)
				.Height(ActionH)
				.OnClicked_Lambda([this, SlotIndex]() { OnSlotContinue(SlotIndex); })
			];
		Actions->AddSlot()
			.AutoWidth()
			.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("menu.slot.overwrite", TEXT("New")))
				.Kind(EAbyssButtonKind::Ghost)
				.FontPx(bTouch ? 15.f : 12.f)
				.Width(bTouch ? 92.f : 84.f)
				.Height(ActionH)
				.OnClicked_Lambda([this, SlotIndex]() { OnSlotNew(SlotIndex); })
			];
		Actions->AddSlot()
			.AutoWidth()
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("menu.slot.delete", TEXT("Delete")))
				.Kind(EAbyssButtonKind::Danger)
				.FontPx(bTouch ? 15.f : 12.f)
				.Width(bTouch ? 92.f : 84.f)
				.Height(ActionH)
				.OnClicked_Lambda([this, SlotIndex]() { OnSlotDelete(SlotIndex); })
			];
	}
	else
	{
		Actions->AddSlot()
			.AutoWidth()
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("menu.newGame", TEXT("New Journey")))
				.Kind(EAbyssButtonKind::Primary)
				.FontPx(bTouch ? 18.f : 16.f)
				.Width(240.f)
				.Height(bTouch ? 48.f : 36.f)
				.OnClicked_Lambda([this, SlotIndex]() { OnSlotNew(SlotIndex); })
			];
	}

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			Card
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 10.f))
		[
			Actions
		];
}

void SAbyssMainMenu::OnSlotContinue(int32 SlotIndex)
{
	const abyss::SaveSlotInfo* Info = FindSlot(SlotIndex);
	if (Info == nullptr)
	{
		RefreshSlots();
		RebuildScreen();
		return;
	}
	// 1.2: the difficulty selector appears once a difficulty has been completed (or the save is above normal).
	if (Info->showDifficultySelector)
	{
		DifficultySlot = SlotIndex;
		ShowScreen(EScreen::Difficulty);
		return;
	}
	LoadSlot(SlotIndex, std::nullopt, false);
}

void SAbyssMainMenu::OnSlotNew(int32 SlotIndex)
{
	if (FindSlot(SlotIndex) == nullptr)
	{
		PendingSlot = SlotIndex;
		ShowScreen(EScreen::ClassSelect);
		return;
	}
	// U1 / FIX Q2: never overwrite a hero silently.
	const TWeakPtr<SAbyssMainMenu> WeakMenu = SharedThis(this);
	FAbyssConfirmRequest Request;
	Request.Title = Ctx->LocOr("menu.slot.overwriteTitle", TEXT("Start a new journey here?"));
	Request.Body = Ctx->LocOr("menu.slot.overwriteBody", TEXT("The hero saved in this slot will be replaced once the new journey starts."));
	Request.ConfirmLabel = Ctx->LocOr("menu.slot.overwrite", TEXT("New"));
	Request.CancelLabel = Ctx->LocOr("menu.backShort", TEXT("Back"));
	Request.bDanger = true;
	Request.OnConfirm = [WeakMenu, SlotIndex]()
	{
		if (const TSharedPtr<SAbyssMainMenu> Menu = WeakMenu.Pin())
		{
			Menu->PendingSlot = SlotIndex;
			Menu->ShowScreen(EScreen::ClassSelect);
		}
	};
	Ctx->Confirm(MoveTemp(Request));
}

void SAbyssMainMenu::OnSlotDelete(int32 SlotIndex)
{
	const abyss::SaveSlotInfo* Info = FindSlot(SlotIndex);
	if (Info == nullptr)
	{
		return;
	}
	const TWeakPtr<SAbyssMainMenu> WeakMenu = SharedThis(this);
	FAbyssConfirmRequest Request;
	Request.Title = Ctx->LocOr("menu.slot.deleteTitle", TEXT("Delete this save?"));
	Request.Body = Ctx->LocArgsOr("menu.slot.deleteBody", TEXT("{class} Lv.{level} will be lost forever."),
		{ FAbyssUiContext::Arg("class", Ctx->ClassName(Info->classId)), FAbyssUiContext::Arg("level", Info->level) });
	Request.ConfirmLabel = Ctx->LocOr("menu.slot.delete", TEXT("Delete"));
	Request.CancelLabel = Ctx->LocOr("menu.backShort", TEXT("Back"));
	Request.bDanger = true;
	Request.OnConfirm = [WeakMenu, SlotIndex]()
	{
		const TSharedPtr<SAbyssMainMenu> Menu = WeakMenu.Pin();
		if (!Menu.IsValid())
		{
			return;
		}
		if (UAbyssGameInstance* GameInstance = Menu->Ctx->GetGameInstance())
		{
			GameInstance->DeleteSlot(SlotIndex);
		}
		Menu->RefreshSlots();
		Menu->RebuildScreen();
	};
	Ctx->Confirm(MoveTemp(Request));
}

void SAbyssMainMenu::LoadSlot(int32 SlotIndex, std::optional<abyss::Difficulty> DifficultyOverride, bool bFromBackup)
{
	UAbyssGameInstance* GameInstance = Ctx->GetGameInstance();
	if (GameInstance == nullptr)
	{
		return;
	}
	const FAbyssLoadResult Result = GameInstance->ContinueSlot(SlotIndex, DifficultyOverride, bFromBackup);
	if (Result.bOk || Result.bDeferred)
	{
		return;  // the app state switches to InGame
	}
	if (Result.bSlotMissing)
	{
		RefreshSlots();
		ShowScreen(EScreen::Title);
		return;
	}
	const TWeakPtr<SAbyssMainMenu> WeakMenu = SharedThis(this);
	FAbyssConfirmRequest Request;
	Request.Title = Ctx->LocOr("menu.slot.loadFailedTitle", TEXT("Cannot load this save"));
	Request.bDanger = false;
	if (Result.Error == abyss::SaveError::VersionTooNew)
	{
		Request.Body = Ctx->LocOr("menu.slot.versionTooNew", TEXT("This save was made by a newer version of the game. Update the game to continue it."));
		Request.ConfirmLabel = Ctx->LocOr("ui.error.ok", TEXT("OK"));
		Request.bNoCancel = true;
	}
	else if (Result.bBackupAvailable && !bFromBackup)
	{
		Request.Body = Ctx->LocOr("menu.slot.corruptBackup", TEXT("This save file is damaged. Load the previous save of this slot instead?"));
		Request.ConfirmLabel = Ctx->LocOr("menu.slot.loadBackup", TEXT("Load previous save"));
		Request.CancelLabel = Ctx->LocOr("menu.backShort", TEXT("Back"));
		Request.OnConfirm = [WeakMenu, SlotIndex, DifficultyOverride]()
		{
			if (const TSharedPtr<SAbyssMainMenu> Menu = WeakMenu.Pin())
			{
				Menu->LoadSlot(SlotIndex, DifficultyOverride, true);
			}
		};
	}
	else
	{
		Request.Body = Ctx->LocOr("menu.slot.corrupt", TEXT("This save file is damaged and cannot be loaded."));
		Request.ConfirmLabel = Ctx->LocOr("ui.error.ok", TEXT("OK"));
		Request.bNoCancel = true;
	}
	Ctx->Confirm(MoveTemp(Request));
}

void SAbyssMainMenu::StartGame(abyss::ClassId Class)
{
	if (UAbyssGameInstance* GameInstance = Ctx->GetGameInstance())
	{
		GameInstance->StartNewGame(Class, PendingSlot, abyss::Difficulty::Normal);
	}
}

// =====================================================================================================================
// Class select (1.2)
// =====================================================================================================================

TSharedRef<SWidget> SAbyssMainMenu::BuildClassSelect()
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const abyss::DataStore* Data = Ctx->GetData();
	const bool bTouch = Ctx->IsTouch();
	TSharedRef<SCanvas> Canvas = SNew(SCanvas);

	Canvas->AddSlot()
		.Position(FVector2D(0.0, 238.0))
		.Size(FVector2D(1280.0, 28.0))
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			[
				AbyssUi::TitleLabel(*Ctx, Ctx->LocOr("menu.classSelect.title", TEXT("Choose a Class")), 20.f, C.Parchment, true, 2)
			]
		];

	constexpr double CardW = 250.0;
	constexpr double CardH = 356.0;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const abyss::ClassId Class = GAbyssMenuClasses[Index];
		const std::string ClassId(abyss::EnumName(Class));
		const FLinearColor ClassColor = FAbyssUiStyle::Rgb(GAbyssMenuClassColors[Index]);
		const FLinearColor AccentText = FAbyssUiStyle::Rgb(GAbyssMenuClassText[Index]);
		const FString Name = Ctx->NameOr("menu.classSelect." + ClassId + ".name", AbyssText::ToStd(Ctx->ClassName(Class)));
		const FText Desc = Ctx->Loc("menu.classSelect." + ClassId + ".desc");
		const double CenterX = 640.0 + (Index - 1) * 276.0;
		const double Delay = 0.08 * Index;
		const double Start = Ctx->Now();

		// first four class skills that have an icon (fallback: the first four)
		TArray<std::string> SkillIds;
		if (const abyss::ClassDef* Def = Data ? Data->Classes().Find(Class) : nullptr)
		{
			for (const abyss::SkillDef& Skill : Def->skills)
			{
				if (SkillIds.Num() < 4 && !Skill.passive && Ctx->SkillIcon(Skill.id) != nullptr)
				{
					SkillIds.Add(Skill.id);
				}
			}
			for (const abyss::SkillDef& Skill : Def->skills)
			{
				if (SkillIds.Num() >= 4)
				{
					break;
				}
				if (!Skill.passive && !SkillIds.Contains(Skill.id))
				{
					SkillIds.Add(Skill.id);
				}
			}
		}

		TSharedPtr<SButton> CardButton;
		TSharedRef<SOverlay> Body = SNew(SOverlay);
		TSharedRef<SWidget> Card = SAssignNew(CardButton, SButton)
			.ButtonStyle(&Ctx->Style().InvisibleButton())
			.ContentPadding(FMargin(0.f))
			.IsFocusable(false)
			.ClickMethod(bTouch ? EButtonClickMethod::DownAndUp : EButtonClickMethod::MouseDown)
			.TouchMethod(EButtonTouchMethod::PreciseTap)
			.OnClicked_Lambda([this, Class]()
			{
				Ctx->PlaySound(abyss::SfxId::Click);
				StartGame(Class);
				return FReply::Handled();
			})
			[
				Body
			];
		const TWeakPtr<SButton> WeakCard = CardButton;
		Body->AddSlot()
		[
			SNew(SAbyssCanvas, Context, [Context, WeakCard, ClassColor, Class, Name, AccentText](FAbyssPainter& P, const FVector2D& Size)
			{
				const TSharedPtr<SButton> Pinned = WeakCard.Pin();
				const bool bHovered = Pinned.IsValid() && Pinned->IsHovered();
				P.Frame(FVector2D::ZeroVector, Size, 1, bHovered ? FAbyssUiStyle::Lighten(ClassColor, 0.3f) : ClassColor);
				// coloured radial light + pedestal
				P.Glow(FVector2D(Size.X * 0.5, 120.0), 120.f, FAbyssUiStyle::WithAlpha(ClassColor, bHovered ? 0.35f : 0.22f), 10);
				P.Circle(FVector2D(Size.X * 0.5, 205.0), 70.f, FLinearColor(0.f, 0.f, 0.f, 0.35f));
				P.Ring(FVector2D(Size.X * 0.5, 205.0), 70.f, FAbyssUiStyle::WithAlpha(ClassColor, 0.5f), 1.5f);
				// hero portrait (170 px tall preview area)
				if (const FSlateBrush* Portrait = Context->HeroPortrait(Class))
				{
					P.Brush(FVector2D(Size.X * 0.5 - 85.0, 30.0), FVector2D(170.0, 170.0), Portrait);
				}
				else
				{
					P.Diamond(FVector2D(Size.X * 0.5, 115.0), 96.f, FAbyssUiStyle::WithAlpha(ClassColor, 0.35f));
					P.TextCentered(FVector2D(Size.X * 0.5, 115.0), Name.Left(1), P.Style.Font(EAbyssFontFace::Title, 64.f, true, 3), AccentText);
				}
			})
		];
		TSharedRef<SHorizontalBox> Skills = SNew(SHorizontalBox);
		for (const std::string& SkillId : SkillIds)
		{
			const abyss::SkillDef* Skill = Data ? Data->FindSkill(SkillId) : nullptr;
			const uint32 TypeColor = Data && Skill ? Data->Classes().damageTypeColors[static_cast<size_t>(Skill->damageType)] : 0xcccccc;
			const FString SkillName = Ctx->SkillName(SkillId);
			Skills->AddSlot()
				.AutoWidth()
				.Padding(FMargin(4.f, 0.f))
				[
					SNew(SAbyssCanvas, Context, [Context, SkillId, TypeColor, SkillName](FAbyssPainter& P, const FVector2D& Size)
					{
						P.RoundBox(FVector2D::ZeroVector, Size, FAbyssUiStyle::Rgb(0x0c0b0e), 4.f, FAbyssUiStyle::Rgb(TypeColor), 1.2f);
						if (const FSlateBrush* Icon = Context->SkillIcon(SkillId))
						{
							P.Brush(FVector2D(2.0, 2.0), Size - FVector2D(4.0, 4.0), Icon);
						}
						else
						{
							P.TextCentered(Size * 0.5, SkillName.Left(1), P.Style.Body(14.f, true, 1), FAbyssUiStyle::Rgb(TypeColor));
						}
					})
					.Size(FVector2D(30.0, 30.0))
				];
		}
		Body->AddSlot()
		.Padding(FMargin(18.f, 222.f, 18.f, 0.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				AbyssMenu_Text(Context, FText::AsCultureInvariant(Name), 20.f, AccentText, true)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 6.f, 0.f, 0.f))
			[
				AbyssMenu_Text(Context, Desc, 13.f, C.TextSoft, false, static_cast<float>(CardW - 36.0), ETextJustify::Center)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 10.f, 0.f, 0.f))
			[
				Skills
			]
		];

		TSharedRef<SWidget> Column = SNew(SOverlay)
			+ SOverlay::Slot()
			[
				Card
			]
			+ SOverlay::Slot()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 0.f, 0.f, 13.f))
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocOr("menu.classSelect.confirm", TEXT("Start Adventure")))
				.Kind(EAbyssButtonKind::Primary)
				.FontPx(bTouch ? 17.f : 14.f)
				.Width(static_cast<float>(CardW - 60.0))
				.Height(bTouch ? 46.f : 34.f)
				.OnClicked_Lambda([this, Class]() { StartGame(Class); })
			];
		// hover: the card rises 8 px (160 ms Quad.easeOut); entrance: alpha 0 -> 1, y +24 -> 0 over 360 ms, delay 80 ms each
		TSharedPtr<SBox> Animated;
		Canvas->AddSlot()
			.Position(FVector2D(CenterX - CardW * 0.5, 274.0))
			.Size(FVector2D(CardW, CardH))
			[
				SAssignNew(Animated, SBox)
				.RenderTransform_Lambda([Context, WeakCard, Start, Delay]()
				{
					const float Entrance = AbyssEase::CubicOut(AbyssEase::Progress(Context->Now(), Start + Delay, 0.36));
					const TSharedPtr<SButton> Pinned = WeakCard.Pin();
					const float Rise = Pinned.IsValid() && Pinned->IsHovered() ? -8.f : 0.f;
					return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(0.f, 24.f * (1.f - Entrance) + Rise)));
				})
				[
					Column
				]
			];
	}
	Canvas->AddSlot()
		.Position(FVector2D(560.0, 650.0))
		.Size(FVector2D(160.0, 40.0))
		[
			BuildBackButton(160.f, bTouch ? 40.f : 34.f)
		];
	return Canvas;
}

TSharedRef<SWidget> SAbyssMainMenu::BuildBackButton(float Width, float Height)
{
	return SNew(SAbyssButton, Ctx.ToSharedRef())
		.Text(Ctx->LocOr("menu.back", TEXT("Back")))
		.Kind(EAbyssButtonKind::Ghost)
		.FontPx(Ctx->IsTouch() ? 16.f : 14.f)
		.Width(Width)
		.Height(Height)
		.OnClicked_Lambda([this]()
		{
			RefreshSlots();
			ShowScreen(EScreen::Title);
		});
}

// =====================================================================================================================
// Difficulty (1.2 / Q33)
// =====================================================================================================================

TSharedRef<SWidget> SAbyssMainMenu::BuildDifficulty()
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	TSharedRef<SCanvas> Canvas = SNew(SCanvas);
	const abyss::SaveSlotInfo* Info = FindSlot(DifficultySlot);
	if (Info == nullptr)
	{
		return Canvas;
	}
	const std::vector<abyss::Difficulty> Completed = Info->completedDifficulties;
	const abyss::DifficultyStates States = abyss::GetDifficultyStates(std::span<const abyss::Difficulty>(Completed.data(), Completed.size()));
	const abyss::Difficulty Current = Info->difficulty;
	const int32 SlotIndex = DifficultySlot;

	Canvas->AddSlot()
		.Position(FVector2D(0.0, 258.0))
		.Size(FVector2D(1280.0, 30.0))
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			[
				AbyssUi::TitleLabel(*Ctx, Ctx->LocOr("menu.difficulty.title", TEXT("Select Difficulty")), 22.f, C.Parchment, true, 2)
			]
		];
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const abyss::Difficulty Difficulty = GAbyssMenuDifficulties[Index];
		const abyss::DifficultyState State = States.Of(Difficulty);
		const bool bLocked = State == abyss::DifficultyState::Locked;
		const bool bCompleted = State == abyss::DifficultyState::Completed;
		const bool bCurrent = Difficulty == Current;
		const std::string Id(abyss::EnumName(Difficulty));
		FString Label = Ctx->DifficultyName(Difficulty);
		if (bCompleted)
		{
			Label = TEXT("\x2713 ") + Label;
		}
		else if (bLocked)
		{
			Label = TEXT("\x2716 ") + Label;
		}
		const FLinearColor LabelColor = bLocked ? FAbyssUiStyle::Rgb(0x6a635c)
			: (bCurrent ? FLinearColor::White : FAbyssUiStyle::Rgb(GAbyssMenuDifficultyLabel[Index]));
		const FText Desc = bLocked ? Ctx->LocOr("menu.difficulty.locked", TEXT("Locked"))
			: FText::AsCultureInvariant(Ctx->NameOr("menu.difficulty.desc." + Id, Id));
		const FLinearColor Accent = bLocked ? FAbyssUiStyle::Rgb(0x3f3845) : FAbyssUiStyle::Rgb(GAbyssMenuDifficultyAccent[Index]);

		TSharedPtr<SButton> CardButton;
		TSharedRef<SOverlay> Body = SNew(SOverlay);
		const TSharedRef<SWidget> CardWidget = SAssignNew(CardButton, SButton)
			.ButtonStyle(&Ctx->Style().InvisibleButton())
			.ContentPadding(FMargin(0.f))
			.IsFocusable(false)
			.IsEnabled(!bLocked)
			.ClickMethod(Ctx->IsTouch() ? EButtonClickMethod::DownAndUp : EButtonClickMethod::MouseDown)
			.TouchMethod(EButtonTouchMethod::PreciseTap)
			.OnClicked_Lambda([this, SlotIndex, Difficulty]()
			{
				Ctx->PlaySound(abyss::SfxId::Click);
				LoadSlot(SlotIndex, Difficulty, false);
				return FReply::Handled();
			})
			[
				Body
			];
		const TWeakPtr<SButton> WeakCard = CardButton;
		Body->AddSlot()
		[
			SNew(SAbyssCanvas, Context, [WeakCard, Accent, bLocked](FAbyssPainter& P, const FVector2D& Size)
			{
				const TSharedPtr<SButton> Pinned = WeakCard.Pin();
				const bool bHovered = Pinned.IsValid() && Pinned->IsHovered() && !bLocked;
				P.Frame(FVector2D::ZeroVector, Size, 1, bHovered ? FAbyssUiStyle::Lighten(Accent, 0.3f) : Accent, 0.f, bLocked ? 0.6f : 1.f);
				if (bHovered)
				{
					P.RoundBox(FVector2D(3.0, 3.0), Size - FVector2D(6.0, 6.0), FAbyssUiStyle::WithAlpha(Accent, 0.12f), 4.f);
				}
			})
		];
		Body->AddSlot()
		.Padding(FMargin(16.f, 8.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				AbyssMenu_Text(Context, FText::AsCultureInvariant(Label), 20.f, LabelColor, true)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				AbyssMenu_Text(Context, Desc, 13.f, bLocked ? C.Dim : C.TextSoft, false, 320.f)
			]
		];
		if (bCurrent && !bLocked)
		{
			Body->AddSlot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(FMargin(0.f, 6.f, 10.f, 0.f))
			[
				AbyssMenu_Text(Context, Ctx->LocOr("menu.difficulty.current", TEXT("Current")), 11.f, FAbyssUiStyle::Rgb(0xffe7a0), true)
			];
		}
		Canvas->AddSlot()
			.Position(FVector2D(640.0 - 180.0, 308.0 + Index * 76.0))
			.Size(FVector2D(360.0, 64.0))
			[
				CardWidget
			];
	}
	Canvas->AddSlot()
		.Position(FVector2D(550.0, 557.0))
		.Size(FVector2D(180.0, 40.0))
		[
			BuildBackButton(180.f, Ctx->IsTouch() ? 40.f : 38.f)
		];
	return Canvas;
}

// =====================================================================================================================
// Language (1.2, U9)
// =====================================================================================================================

TSharedRef<SWidget> SAbyssMainMenu::BuildLanguage()
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	TSharedRef<SCanvas> Canvas = SNew(SCanvas);
	const UAbyssGameInstance* GameInstance = Ctx->GetGameInstance();
	const abyss::LocaleId Current = GameInstance != nullptr ? GameInstance->GetUserSettings().Locale : abyss::LocaleId::ZhCN;

	Canvas->AddSlot()
		.Position(FVector2D(0.0, 276.0))
		.Size(FVector2D(1280.0, 30.0))
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			[
				AbyssUi::TitleLabel(*Ctx, Ctx->LocOr("menu.language", TEXT("Language")), 22.f, C.Parchment, true, 2)
			]
		];
	struct FLanguage
	{
		abyss::LocaleId Locale;
		const char* Key;
		const TCHAR* Fallback;
	};
	static const FLanguage Languages[3] = {
		{ abyss::LocaleId::ZhCN, "menu.langSelect.zhCN", TEXT("Simplified Chinese") },
		{ abyss::LocaleId::ZhTW, "menu.langSelect.zhTW", TEXT("Traditional Chinese") },
		{ abyss::LocaleId::En, "menu.langSelect.en", TEXT("English") },
	};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FLanguage& Language = Languages[Index];
		const abyss::LocaleId Locale = Language.Locale;
		const bool bCurrent = Locale == Current;
		Canvas->AddSlot()
			.Position(FVector2D(480.0, 336.0 + Index * 62.0))
			.Size(FVector2D(320.0, 46.0))
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SAbyssButton, Context)
					.Text(Ctx->LocOr(Language.Key, Language.Fallback))
					.Kind(bCurrent ? EAbyssButtonKind::Primary : EAbyssButtonKind::Secondary)
					.FontPx(17.f)
					.Width(320.f)
					.Height(46.f)
					.OnClicked_Lambda([this, Locale]() { SetLocale(Locale); })
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.f, 0.f, 22.f, 0.f))
				[
					SNew(SBox)
					.Visibility(bCurrent ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
					[
						AbyssMenu_Text(Context, Ctx->LocOr("menu.difficulty.current", TEXT("Current")), 11.f, FAbyssUiStyle::Rgb(0xffe7a0), true)
					]
				]
			];
	}
	Canvas->AddSlot()
		.Position(FVector2D(550.0, 541.0))
		.Size(FVector2D(180.0, 40.0))
		[
			BuildBackButton(180.f, 38.f)
		];
	return Canvas;
}

void SAbyssMainMenu::SetLocale(abyss::LocaleId Locale)
{
	UAbyssGameInstance* GameInstance = Ctx->GetGameInstance();
	if (GameInstance == nullptr || GameInstance->GetUserSettings().Locale == Locale)
	{
		return;
	}
	FAbyssUserSettings Settings = GameInstance->GetUserSettings();
	Settings.Locale = Locale;
	// Persists settings.json and switches the core tables; the UI rebuilds on the next tick (stays on this screen).
	GameInstance->ApplyUserSettings(Settings);
}

// =====================================================================================================================
// Credits
// =====================================================================================================================

void SAbyssMainMenu::ShowCredits()
{
	ModalHost->SetContent(SNew(SAbyssCreditsSheet, Ctx.ToSharedRef()).OnClose_Lambda([this]() { CloseCredits(); }));
	ModalHost->SetVisibility(EVisibility::Visible);
	bCreditsOpen = true;
}

void SAbyssMainMenu::CloseCredits()
{
	if (!bCreditsOpen || !ModalHost.IsValid())
	{
		return;
	}
	bCreditsOpen = false;
	ModalHost->SetContent(SNullWidget::NullWidget);
	ModalHost->SetVisibility(EVisibility::Collapsed);
}
