#include "UI/Panels/SAbyssSystemMenuPanel.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

#include "Framework/AbyssGameInstance.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	constexpr float GAbyssSystemMenuWidth = 360.f;
	constexpr float GAbyssSystemMenuRow = 50.f;
}

void SAbyssSystemMenuPanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	ButtonCount = PLATFORM_IOS ? 5 : 6;
	SetPanelContent(
		SNew(SAbyssPanelFrame, InContext)
		.Size(GetDesignSize())
		.Title(InContext->LocOr("ui.system.title", TEXT("Menu")))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SAssignNew(Buttons, SVerticalBox)
		]);
}

FVector2D SAbyssSystemMenuPanel::GetDesignSize() const
{
	return FVector2D(GAbyssSystemMenuWidth, 36.0 + 18.0 + ButtonCount * GAbyssSystemMenuRow + 16.0);
}

void SAbyssSystemMenuPanel::Refresh(const abyss::Snapshot& Snap)
{
	Buttons->ClearChildren();
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const float ButtonW = GAbyssSystemMenuWidth - 60.f;
	const float ButtonH = GAbyssSystemMenuRow - 12.f;

	const auto AddButton = [this, &Context, ButtonW, ButtonH](const FText& Label, EAbyssButtonKind Kind, TFunction<void()> Action)
	{
		Buttons->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 6.f))
			[
				SNew(SAbyssButton, Context)
				.Text(Label)
				.Kind(Kind)
				.FontPx(15.f)
				.Width(ButtonW)
				.Height(ButtonH)
				.OnClicked_Lambda([Action]()
				{
					if (Action)
					{
						Action();
					}
				})
			];
	};

	AddButton(Ctx->LocOr("ui.system.resume", TEXT("Resume")), EAbyssButtonKind::Primary, [this]() { RequestClose(); });
	AddButton(Ctx->LocOr("ui.system.settings", TEXT("Settings")), EAbyssButtonKind::Secondary, [Context]()
	{
		if (IAbyssUiHost* Host = Context->GetHost())
		{
			Host->OpenPanel(abyss::PanelId::Settings);
		}
	});
	AddButton(Ctx->LocOr("ui.system.achievements", TEXT("Achievements")), EAbyssButtonKind::Secondary, [Context]()
	{
		if (IAbyssUiHost* Host = Context->GetHost())
		{
			Host->OpenPanel(abyss::PanelId::Achievements);
		}
	});
	AddButton(Ctx->LocOr("ui.system.controls", TEXT("Controls")), EAbyssButtonKind::Secondary, [Context]()
	{
		if (IAbyssUiHost* Host = Context->GetHost())
		{
			Host->ShowHelp();
		}
	});
	// U4: "Save & return to menu" - the GameInstance resolves a pending death (C12), saves and ends the session.
	AddButton(Ctx->LocOr("ui.system.saveAndExit", TEXT("Save & Return to Title")), EAbyssButtonKind::Secondary, [Context]()
	{
		if (UAbyssGameInstance* GameInstance = Context->GetGameInstance())
		{
			GameInstance->ReturnToMainMenu();
		}
	});
#if !PLATFORM_IOS
	AddButton(Ctx->LocOr("ui.system.quit", TEXT("Save & Quit")), EAbyssButtonKind::Danger, [Context]()
	{
		if (UAbyssGameInstance* GameInstance = Context->GetGameInstance())
		{
			GameInstance->QuitGame();
		}
	});
#endif
}
