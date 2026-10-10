// Core-owned conversation modals (SimTypes.h ownership: opened by the core, closed through CmdClosePanel):
// * SAbyssDialoguePanel   NPC dialogue tree / linear dialogue (save-ui-input 7.10, quests-story-ch1 7.1 / 7.2): NPC
//                         portrait and name, the node text (scrolls when long), the core's buttons (choices with the
//                         in-progress suffix, Continue, Back, Leave) -> CmdDialogueChoose.
// * SAbyssMiniBossPanel   mini-boss pre-fight lines (7.11, monsters-ai 8.3 / M8): red frame, every line, Fight ->
//                         CmdMiniBossDialogueDismiss (the backdrop does the same).
// * SAbyssLorePanel       lore popup (7.12): entry name, zone, text in a parchment well; auto-closes after 8 s, the timer
//                         restarts when a newer pickup replaces the text (FIX quests Q15).
// * SAbyssPuzzlePanel     environmental puzzle prompt (world 13.3): the prompt, Solve (CmdPuzzleAnswer 0) / Leave (1).
#pragma once

#include "CoreMinimal.h"

#include <string>
#include <vector>

#include "UI/Panels/SAbyssPanelBase.h"

class SBox;
class SVerticalBox;

class SAbyssDialoguePanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssDialoguePanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const std::string& InNpcId);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::Dialogue; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(480.0, Height); }
	virtual bool IsModal() const override { return true; }
	virtual float GetBackdropAlpha() const override { return 0.7f; }
	virtual std::string GetNpcId() const override { return NpcId; }

private:
	TSharedPtr<SBox> Content;
	std::string NpcId;
	double Height = 360.0;
};

class SAbyssMiniBossPanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssMiniBossPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const std::string& InNameKey,
		const std::string& InMonsterId, const std::vector<std::string>& InLineKeys);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::MiniBossDialogue; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(500.0, Height); }
	virtual bool IsModal() const override { return true; }
	virtual float GetBackdropAlpha() const override { return 0.95f; }

private:
	TSharedPtr<SBox> Content;
	std::string NameKey;
	std::string MonsterId;
	std::vector<std::string> LineKeys;
	double Height = 260.0;
	bool bBuilt = false;
};

class SAbyssLorePanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssLorePanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::LoreText; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(440.0, 260.0); }
	virtual bool IsModal() const override { return true; }
	virtual float GetBackdropAlpha() const override { return 0.75f; }
	virtual void TickPanel(const abyss::Snapshot& Snap, double Now) override;

private:
	TSharedPtr<SBox> Content;
	std::string LoreId;
	double ShownAt = 0.0;
};

class SAbyssPuzzlePanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssPuzzlePanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::Puzzle; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(420.0, 260.0); }
	virtual bool IsModal() const override { return true; }
	virtual float GetBackdropAlpha() const override { return 0.6f; }

private:
	TSharedPtr<SBox> Content;
};
