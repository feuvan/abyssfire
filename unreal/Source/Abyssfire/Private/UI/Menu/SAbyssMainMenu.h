// SAbyssMainMenu: the title screen and its sub-screens (save-ui-input.md 1.2 with the port decisions U1 / U4 / U9 / Q2 /
// Q3 / Q25 / Q33):
//   Title       logo block, the three save slots (U1: continue / new journey / overwrite / delete, confirmed), settings,
//               controls, language, credits, quit (not on iOS)
//   ClassSelect three class cards (hero portrait, name, description, first skills) -> UAbyssGameInstance::StartNewGame
//   Difficulty  the Continue difficulty selector when SaveSlotInfo::showDifficultySelector (Q33: same hero and position)
//   Language    zh-CN / zh-TW / en (persisted in settings.json, U9)
// Load errors are explained (save from a newer build vs a damaged save; a damaged save offers its .bak).
// The layout is the web's 1280x720 design, scaled down only when the safe area is smaller (notched phones).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include <optional>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/save/SaveIO.h"

#include "UI/Core/AbyssUiContext.h"

class SBox;

class SAbyssMainMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssMainMenu) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	/** Back to the title screen with freshly listed slots (app state MainMenu). */
	void Activate();
	/** Esc / Android back: modal -> sub-screen -> nothing (false on the title screen). */
	bool HandleBack();
	/** Locale or control layout changed: rebuild the current screen. */
	void RefreshLocale();

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	enum class EScreen : uint8
	{
		Title,
		ClassSelect,
		Difficulty,
		Language,
	};

	void ShowScreen(EScreen InScreen);
	void RebuildScreen();
	void RefreshSlots();
	const abyss::SaveSlotInfo* FindSlot(int32 SlotIndex) const;

	TSharedRef<SWidget> BuildTitleScreen();
	TSharedRef<SWidget> BuildSlotCard(int32 SlotIndex);
	TSharedRef<SWidget> BuildClassSelect();
	TSharedRef<SWidget> BuildDifficulty();
	TSharedRef<SWidget> BuildLanguage();
	TSharedRef<SWidget> BuildBackButton(float Width, float Height);

	void OnSlotContinue(int32 SlotIndex);
	void OnSlotNew(int32 SlotIndex);
	void OnSlotDelete(int32 SlotIndex);
	void LoadSlot(int32 SlotIndex, std::optional<abyss::Difficulty> DifficultyOverride, bool bFromBackup);
	void StartGame(abyss::ClassId Class);
	void SetLocale(abyss::LocaleId Locale);
	void ShowCredits();
	void CloseCredits();

	TSharedPtr<FAbyssUiContext> Ctx;
	TSharedPtr<SBox> ScreenHost;
	TSharedPtr<SBox> ModalHost;
	std::vector<abyss::SaveSlotInfo> Slots;
	EScreen Screen = EScreen::Title;
	int32 PendingSlot = 0;
	int32 DifficultySlot = 0;
	double ScreenStart = 0.0;
	bool bCreditsOpen = false;
	FString Version;
};
