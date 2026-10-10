// FAbyssUiContext: the services every UI widget uses (owned by UAbyssUiSubsystem, shared by reference with the widgets).
//
// * Core access: game instance, data store, i18n, the current snapshot (valid until the next call into the GameSim -
//   widgets copy what they keep), command submission, touch layout / unit scale.
// * Text: i18n keys only (save-ui-input 10). LocOr / LocArgsOr show an English fallback while a key is missing from the
//   exported tables (new UI keys requested from the data agent).
// * Presentation helpers: item / stat / monster / quest names in the current locale, number formats (JS String()).
// * Overlay services (IAbyssUiHost, implemented by SAbyssUiRoot): tooltips, popups, confirms, toasts, panel open / close.
// * Icons: brushes for item / skill / portrait / glyph textures (UI textures loaded by the subsystem; misses fall back to
//   procedural glyphs in the widgets).
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

#include <initializer_list>
#include <string>
#include <string_view>

#include "abyss/base/Enums.h"
#include "abyss/base/I18n.h"
#include "abyss/base/Stats.h"
#include "abyss/data/AudioData.h"
#include "abyss/data/QuestData.h"
#include "abyss/items/Item.h"
#include "abyss/sim/Commands.h"
#include "abyss/sim/SimTypes.h"
#include "abyss/sim/Snapshot.h"

#include "Input/AbyssInputTypes.h"

class FAbyssUiStyle;
class SWidget;
class UAbyssGameInstance;
class UAbyssUiSubsystem;

namespace abyss
{
	class DataStore;
	class I18n;
	struct SkillDef;
	struct ItemBaseDef;
}

/** A confirm dialog (U1 overwrite / delete, discard rare+, sell legendary / set, salvage, return to menu). */
struct FAbyssConfirmRequest
{
	FText Title;
	FText Body;
	FLinearColor BodyColor = FLinearColor::White;
	FText ConfirmLabel;
	FText CancelLabel;
	bool bDanger = false;
	/** Hide the cancel button (information dialogs). */
	bool bNoCancel = false;
	TFunction<void()> OnConfirm;
	TFunction<void()> OnCancel;
};

/** Overlay services implemented by the root widget. */
class IAbyssUiHost
{
public:
	virtual ~IAbyssUiHost() = default;

	/** Tooltip near an absolute (screen-space) anchor; Owner identifies the requester (hide only what you showed). */
	virtual void ShowTooltip(const TSharedRef<SWidget>& Content, const FVector2D& AbsoluteAnchor, const void* Owner) = 0;
	virtual void HideTooltip(const void* Owner) = 0;
	/** Context popup at an absolute anchor; closed by an outside click, Back, or ClosePopup. */
	virtual void ShowPopup(const TSharedRef<SWidget>& Content, const FVector2D& AbsoluteAnchor) = 0;
	virtual void ClosePopup() = 0;
	virtual void ShowConfirm(FAbyssConfirmRequest Request) = 0;
	/** Short toast in the upper centre (Glyph: a leading symbol such as a check mark). */
	virtual void ShowToast(const FText& Text, const FLinearColor& Color) = 0;

	// ---- panels (U4 / U7) ----
	virtual void TogglePanel(abyss::PanelId Panel) = 0;
	virtual void OpenPanel(abyss::PanelId Panel) = 0;
	virtual void ClosePanel(abyss::PanelId Panel) = 0;
	virtual bool IsPanelOpen(abyss::PanelId Panel) const = 0;
	virtual void OpenSocketPanel(abyss::EquipSlot Slot) = 0;
	/** Rebuild the open panels on the next sync (after a local view change). */
	virtual void MarkPanelsDirty() = 0;

	// ---- flow helpers ----
	/**
	 * Quest card turn-in click (quests-story-ch1.md 5.3, T17): remembers the giver; the chain is armed only when the core
	 * confirms the turn-in (EvQuestUpdate{TurnedIn, QuestId}; the web chains only after a successful turnInQuest) and is
	 * dropped if the steps that applied the command report none. Then the NPC's next card is offered after 900 ms, or
	 * 300 ms after the story director turns idle when the turn-in queued a cutscene (CmdQuestCardOpen; the core ignores
	 * it when a card or dialogue is open or the NPC has nothing to offer).
	 */
	virtual void RequestQuestChainOffer(const std::string& QuestId, const std::string& NpcId) = 0;
	/** A UI-side line in the combat log (local feedback the core does not log). */
	virtual void AddLocalLog(const FString& Text, abyss::LogType Type) = 0;
	/** The controls reference (menu "Controls", system menu "Controls"). */
	virtual void ShowHelp() = 0;
};

class FAbyssUiContext : public TSharedFromThis<FAbyssUiContext>
{
public:
	FAbyssUiContext(UAbyssUiSubsystem& InOwner, const TSharedRef<FAbyssUiStyle>& InStyle);

	// ---- core access ----
	UAbyssUiSubsystem* GetOwner() const { return Owner.Get(); }
	UAbyssGameInstance* GetGameInstance() const;
	const abyss::DataStore* GetData() const;
	const abyss::I18n* GetStrings() const;
	/** The running session's snapshot or nullptr (valid until the next call into the GameSim). */
	const abyss::Snapshot* GetSnapshot() const;
	const FAbyssUiStyle& Style() const { return *StylePtr; }
	bool IsTouch() const;
	/** Touch unit scale k (save-ui-input 5.7.1): game px per CSS px on this device, 1 on desktop. */
	float TouchScale() const;
	/** The hero is Dying (C12 / save-ui-input 5.1.1): panel actions read-only. */
	bool IsHeroDying() const;
	/** UI time in seconds (Slate real time). */
	double Now() const;

	// ---- actions ----
	void Submit(const abyss::Command& Command) const;
	void PlaySound(abyss::SfxId Cue) const;
	/** Injects an input action like its key (HUD buttons share the keyboard path, ue58-platform.md 8.2). */
	void PressAction(EAbyssInputAction Action) const;
	/** The key bound to an action on the last used device ("" when none / touch). */
	FString KeyHint(EAbyssInputAction Action) const;

	// ---- host ----
	void SetHost(IAbyssUiHost* InHost) { Host = InHost; }
	IAbyssUiHost* GetHost() const { return Host; }
	void ShowTooltip(const TSharedRef<SWidget>& Content, const FVector2D& AbsoluteAnchor, const void* InOwner) const;
	void HideTooltip(const void* InOwner) const;
	void ShowPopup(const TSharedRef<SWidget>& Content, const FVector2D& AbsoluteAnchor) const;
	void ClosePopup() const;
	void Confirm(FAbyssConfirmRequest Request) const;
	void Toast(const FText& Text, const FLinearColor& Color) const;

	// ---- text ----
	bool HasKey(std::string_view Key) const;
	FText Loc(std::string_view Key) const;
	FString LocStr(std::string_view Key) const;
	FText Loc(const abyss::LocText& Text) const;
	FString LocStr(const abyss::LocText& Text) const;
	/** The key, or Fallback (English) while the key is not in the tables. */
	FText LocOr(std::string_view Key, const TCHAR* Fallback) const;
	FString LocOrStr(std::string_view Key, const TCHAR* Fallback) const;
	FText LocArgs(std::string_view Key, std::initializer_list<abyss::I18nArg> Args) const;
	/** As LocArgs; a missing key substitutes the same {name} args into Fallback. */
	FText LocArgsOr(std::string_view Key, const TCHAR* Fallback, std::initializer_list<abyss::I18nArg> Args) const;
	FString LocArgsOrStr(std::string_view Key, const TCHAR* Fallback, std::initializer_list<abyss::I18nArg> Args) const;
	/** "key or fallback" accessor (save-ui-input 10.5): Key, else the raw data string. */
	FString NameOr(std::string_view Key, const std::string& RawFallback) const;
	/** I18nArg with a pre-formatted value. */
	static abyss::I18nArg Arg(const char* Name, const FString& Value);
	static abyss::I18nArg Arg(const char* Name, int64 Value);
	static abyss::I18nArg ArgKey(const char* Name, std::string Key);

	// ---- names ----
	FString ItemName(const abyss::ItemInstance& Item) const;
	FString ItemBaseName(const std::string& BaseId) const;
	FString StatLabel(abyss::Stat Stat) const;
	bool IsPercentStat(abyss::Stat Stat) const;
	/** "+12" / "+5%" (whole numbers stay integers, others one decimal). */
	FString StatValue(abyss::Stat Stat, double Value, bool bSigned = true) const;
	FString MonsterName(const std::string& DefId) const;
	FString NpcName(const std::string& NpcId) const;
	FString QuestName(const abyss::QuestDef& Quest) const;
	FString ZoneName(const std::string& MapId) const;
	FString ClassName(abyss::ClassId Class) const;
	FString SkillName(const abyss::SkillDef& Skill) const;
	FString SkillName(const std::string& SkillId) const;
	FString DamageTypeName(abyss::DamageType Type) const;
	FString QualityName(abyss::ItemQuality Quality) const;
	FString DifficultyName(abyss::Difficulty Difficulty) const;
	FString PetName(const std::string& PetId, int32 Evolved) const;
	FString ObjectiveTypeLabel(const abyss::QuestObjectiveDef& Objective, abyss::QuestType QuestType) const;
	FString ObjectiveTargetLabel(const abyss::QuestDef& Quest, int32 ObjectiveIndex) const;

	// ---- hero numbers ----
	/**
	 * The hero's bonus bag as the snapshot shows it: gear + achievements + the active ley-beast's passive. The core's
	 * merged EquipStats also holds the Ember Tower blessing and labyrinth boons (later milestones), which the snapshot
	 * does not expose yet (core API request: HeroView::equip).
	 */
	static abyss::EquipStats ApproxEquipStats(const abyss::Snapshot& Snap);

	// ---- numbers (JS String / toFixed parity) ----
	static FString Num(double Value);
	static FString Fixed(double Value, int32 Decimals);
	static FString Int(int64 Value);

	// ---- icons (nullptr = not available; widgets draw a fallback glyph) ----
	const FSlateBrush* ItemIcon(const std::string& BaseId) const;
	const FSlateBrush* SkillIcon(const std::string& SkillId) const;
	const FSlateBrush* QuestItemIcon(const std::string& Kind) const;
	/** Portrait by story art id (class id, NPC sprite id, monster sprite key, emblem_villain, emblem_generic). */
	const FSlateBrush* Portrait(const std::string& ArtId) const;
	const FSlateBrush* HeroPortrait(abyss::ClassId Class) const;
	const FSlateBrush* PetPortrait(const std::string& PetId) const;
	const FSlateBrush* Glyph(const TCHAR* GlyphId) const;
	/** Any UI texture by asset name (cached brush). */
	const FSlateBrush* TextureBrush(FName AssetName) const;
	/** Brush around a runtime texture (minimap); the caller keeps the texture alive. */
	const FSlateBrush* RuntimeTextureBrush(class UTexture2D* Texture, FName Key) const;

private:
	TWeakObjectPtr<UAbyssUiSubsystem> Owner;
	TSharedRef<FAbyssUiStyle> StylePtr;
	IAbyssUiHost* Host = nullptr;
	mutable TMap<FName, TUniquePtr<FSlateBrush>> Brushes;
	mutable TSet<FName> MissingBrushes;
};
