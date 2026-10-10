#include "UI/Panels/SAbyssSettingsPanel.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Framework/AbyssGameInstance.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	constexpr float GAbyssSettingsLabelW = 150.f;
	constexpr float GAbyssSettingsInnerW = 510.f;
}

void SAbyssSettingsPanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	SetPanelContent(
		SNew(SAbyssPanelFrame, InContext)
		.Size(GetDesignSize())
		.Title(InContext->LocOr("ui.settings.title", TEXT("Settings")))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SAssignNew(Scroll, SScrollBox)
			.ScrollBarStyle(&InContext->Style().ScrollBar())
			.ScrollBarThickness(FVector2D(6.0, 6.0))
			.ConsumeMouseWheel(EConsumeMouseWheel::Always)
		]);
}

FAbyssUserSettings SAbyssSettingsPanel::Current() const
{
	const UAbyssGameInstance* GameInstance = Ctx->GetGameInstance();
	return GameInstance != nullptr ? GameInstance->GetUserSettings() : FAbyssUserSettings();
}

void SAbyssSettingsPanel::Apply(const FAbyssUserSettings& NewSettings) const
{
	if (UAbyssGameInstance* GameInstance = Ctx->GetGameInstance())
	{
		if (!(GameInstance->GetUserSettings() == NewSettings))
		{
			GameInstance->ApplyUserSettings(NewSettings);
		}
	}
}

void SAbyssSettingsPanel::Refresh(const abyss::Snapshot& Snap)
{
	// Every value is bound live to the GameInstance's settings: build once.
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	Scroll->ClearChildren();
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();

	const auto Section = [this, &Context](const char* Key, const TCHAR* Fallback)
	{
		Scroll->AddSlot()
			.Padding(FMargin(0.f, 10.f, 0.f, 6.f))
			[
				AbyssUi::SectionHeader(Context, Ctx->LocOr(Key, Fallback), GAbyssSettingsInnerW)
			];
	};
	const auto Row = [this](const TSharedRef<SWidget>& Widget)
	{
		Scroll->AddSlot()
			.Padding(FMargin(0.f, 3.f))
			[
				Widget
			];
	};

	// ---- audio (A3 defaults music 0.6, SFX 0.8) ----
	Section("ui.settings.audio", TEXT("Audio"));
	Row(MakeSliderRow(Ctx->LocOr("ui.settings.master", TEXT("Master")),
		[](const FAbyssUserSettings& S) { return S.MasterVolume; },
		[](FAbyssUserSettings& S, float V) { S.MasterVolume = V; }, 0.f, 1.f));
	Row(MakeSliderRow(Ctx->LocOr("ui.audio.bgm", TEXT("Music")),
		[](const FAbyssUserSettings& S) { return S.MusicVolume; },
		[](FAbyssUserSettings& S, float V) { S.MusicVolume = V; }, 0.f, 1.f));
	Row(MakeSliderRow(Ctx->LocOr("ui.audio.sfx", TEXT("SFX")),
		[](const FAbyssUserSettings& S) { return S.SfxVolume; },
		[](FAbyssUserSettings& S, float V) { S.SfxVolume = V; }, 0.f, 1.f));

	// ---- display ----
	Section("ui.settings.display", TEXT("Display"));
	Row(MakeChoiceRow(Ctx->LocOr("menu.language", TEXT("Language")),
		{ Ctx->LocOr("menu.langSelect.zhCN", TEXT("Simplified Chinese")), Ctx->LocOr("menu.langSelect.zhTW", TEXT("Traditional Chinese")),
			Ctx->LocOr("menu.langSelect.en", TEXT("English")) },
		[](const FAbyssUserSettings& S)
		{
			switch (S.Locale)
			{
			case abyss::LocaleId::ZhCN: return 0;
			case abyss::LocaleId::ZhTW: return 1;
			case abyss::LocaleId::En: return 2;
			}
			return 0;
		},
		[](FAbyssUserSettings& S, int32 Index)
		{
			S.Locale = Index == 1 ? abyss::LocaleId::ZhTW : (Index == 2 ? abyss::LocaleId::En : abyss::LocaleId::ZhCN);
		}));
	Row(MakeChoiceRow(Ctx->LocOr("ui.settings.quality", TEXT("Graphics")),
		{ Ctx->LocOr("ui.settings.quality.auto", TEXT("Auto")), Ctx->LocOr("ui.settings.quality.low", TEXT("Low")),
			Ctx->LocOr("ui.settings.quality.balanced", TEXT("Medium")), Ctx->LocOr("ui.settings.quality.high", TEXT("High")) },
		[](const FAbyssUserSettings& S) { return static_cast<int32>(S.Quality); },
		[](FAbyssUserSettings& S, int32 Index) { S.Quality = static_cast<EAbyssQualitySetting>(FMath::Clamp(Index, 0, 3)); }));
	Row(MakeChoiceRow(Ctx->LocOr("ui.settings.controls", TEXT("Controls")),
		{ Ctx->LocOr("ui.settings.controls.auto", TEXT("Auto")), Ctx->LocOr("ui.settings.controls.desktop", TEXT("Mouse & Keys")),
			Ctx->LocOr("ui.settings.controls.touch", TEXT("Touch")) },
		[](const FAbyssUserSettings& S) { return static_cast<int32>(S.ControlLayout); },
		[](FAbyssUserSettings& S, int32 Index) { S.ControlLayout = static_cast<EAbyssControlLayout>(FMath::Clamp(Index, 0, 2)); }));

	// ---- touch controls (save-ui-input 5.7) ----
	Section("ui.settings.touch", TEXT("Touch Controls"));
	Row(MakeSliderRow(Ctx->LocOr("ui.settings.touchScale", TEXT("Size")),
		[](const FAbyssUserSettings& S) { return S.TouchControlScale; },
		[](FAbyssUserSettings& S, float V) { S.TouchControlScale = V; }, 0.75f, 1.5f));
	Row(MakeSliderRow(Ctx->LocOr("ui.settings.touchOpacity", TEXT("Opacity")),
		[](const FAbyssUserSettings& S) { return S.TouchControlOpacity; },
		[](FAbyssUserSettings& S, float V) { S.TouchControlOpacity = V; }, 0.3f, 1.f));

	// ---- feel ----
	Section("ui.settings.feel", TEXT("Gameplay"));
	Row(MakeToggleRow(Ctx->LocOr("ui.settings.cameraShake", TEXT("Camera shake")),
		[](const FAbyssUserSettings& S) { return S.bCameraShake; },
		[](FAbyssUserSettings& S, bool bOn) { S.bCameraShake = bOn; }));
	Row(MakeToggleRow(Ctx->LocOr("ui.settings.damageNumbers", TEXT("Damage numbers")),
		[](const FAbyssUserSettings& S) { return S.bDamageNumbers; },
		[](FAbyssUserSettings& S, bool bOn) { S.bDamageNumbers = bOn; }));

	Scroll->AddSlot()
		.Padding(FMargin(0.f, 14.f, 0.f, 4.f))
		.HAlign(HAlign_Center)
		[
			SNew(SAbyssButton, Context)
			.Text(Ctx->LocOr("ui.settings.defaults", TEXT("Restore defaults")))
			.Kind(EAbyssButtonKind::Ghost)
			.FontPx(12.f)
			.Width(180.f)
			.Height(Ctx->IsTouch() ? 44.f : 28.f)
			.OnClicked_Lambda([this]()
			{
				FAbyssUserSettings Defaults;
				Defaults.Locale = Current().Locale;  // the language is not a "default" to reset
				Apply(Defaults);
			})
		];
}

TSharedRef<SWidget> SAbyssSettingsPanel::MakeSliderRow(const FText& Label, TFunction<float(const FAbyssUserSettings&)> Get,
	TFunction<void(FAbyssUserSettings&, float)> Set, float Min, float Max)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	// While dragging, the slider and the value text show the dragged value; the settings change when the drag ends.
	const TSharedRef<float> Live = MakeShared<float>(-1.f);
	const float Span = FMath::Max(0.0001f, Max - Min);
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(GAbyssSettingsLabelW)
			[
				AbyssUi::Label(*Ctx, Label, 14.f, C.Text, false, 1)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SAbyssSlider, Context)
			.Width(Ctx->IsTouch() ? 280.f : 260.f)
			.Height(Ctx->IsTouch() ? 44.f : 28.f)
			.Value_Lambda([this, Get, Live, Min, Span]()
			{
				return *Live >= 0.f ? *Live : FMath::Clamp((Get(Current()) - Min) / Span, 0.f, 1.f);
			})
			.OnValueChanged_Lambda([Live](float V) { *Live = V; })
			.OnValueCommitted_Lambda([this, Set, Live, Min, Span](float V)
			{
				*Live = -1.f;
				FAbyssUserSettings NewSettings = Current();
				Set(NewSettings, Min + V * Span);
				Apply(NewSettings);
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(12.f, 0.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Font(Ctx->Style().Body(13.f, true, 1))
			.ColorAndOpacity(FSlateColor(C.GoldBright))
			.Text_Lambda([this, Get, Live, Min, Span]()
			{
				const float Value = *Live >= 0.f ? Min + *Live * Span : Get(Current());
				return FText::AsCultureInvariant(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.f)));
			})
		];
}

TSharedRef<SWidget> SAbyssSettingsPanel::MakeChoiceRow(const FText& Label, const TArray<FText>& Options,
	TFunction<int32(const FAbyssUserSettings&)> Get, TFunction<void(FAbyssUserSettings&, int32)> Set)
{
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const bool bTouch = Ctx->IsTouch();
	TSharedRef<SHorizontalBox> Choices = SNew(SHorizontalBox);
	const float OptionW = FMath::Min(110.f, (GAbyssSettingsInnerW - GAbyssSettingsLabelW - 8.f) / FMath::Max(1, Options.Num()) - 6.f);
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		Choices->AddSlot()
			.AutoWidth()
			.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
			[
				SNew(SAbyssButton, Context)
				.Text(Options[Index])
				.Kind(EAbyssButtonKind::Secondary)
				.FontPx(bTouch ? 14.f : 12.f)
				.Width(OptionW)
				.Height(bTouch ? 44.f : 28.f)
				.Selected_Lambda([this, Get, Index]() { return Get(Current()) == Index; })
				.LabelColor_Lambda([this, Get, Index, Context]()
				{
					return FSlateColor(Get(Current()) == Index ? Context->Style().Colors().GoldBright : Context->Style().Colors().TextSoft);
				})
				.OnClicked_Lambda([this, Set, Index]()
				{
					FAbyssUserSettings NewSettings = Current();
					Set(NewSettings, Index);
					Apply(NewSettings);
				})
			];
	}
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(GAbyssSettingsLabelW)
			[
				AbyssUi::Label(*Ctx, Label, 14.f, C.Text, false, 1)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			Choices
		];
}

TSharedRef<SWidget> SAbyssSettingsPanel::MakeToggleRow(const FText& Label, TFunction<bool(const FAbyssUserSettings&)> Get,
	TFunction<void(FAbyssUserSettings&, bool)> Set)
{
	return MakeChoiceRow(Label, { Ctx->LocOr("ui.settings.on", TEXT("On")), Ctx->LocOr("ui.settings.off", TEXT("Off")) },
		[Get](const FAbyssUserSettings& S) { return Get(S) ? 0 : 1; },
		[Set](FAbyssUserSettings& S, int32 Index) { Set(S, Index == 0); });
}
