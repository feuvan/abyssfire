// HUD widgets other than the bottom group and the minimap (save-ui-input 6.7-6.12): combat log, quest tracker, target
// frame, boss bar, dodge plate, info plate, buff / status row and the pet medallion.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include <string>

#include "abyss/base/I18n.h"
#include "abyss/sim/SimTypes.h"
#include "abyss/sim/Snapshot.h"

#include "UI/Core/AbyssUiContext.h"
#include "UI/Hud/AbyssHudState.h"

class SBox;
class SVerticalBox;

/** 6.10 combat log: last 8 lines; desktop framed + expanded, touch collapsed (3 newest < 12 s) unless toggled. */
class SAbyssCombatLog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssCombatLog) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	void AddLine(const abyss::LocText& Text, abyss::LogType Type);
	/** Adds an already resolved UI line (local feedback such as "only gear can go on the anvil"). */
	void AddLocalLine(const FString& Text, abyss::LogType Type);
	void SetExpanded(bool bInExpanded);
	bool IsExpanded() const { return bExpanded; }
	void SetTouch(bool bInTouch);
	void Rebuild();
	void Clear();
	/** Size of the widget in the current mode (the HUD positions it). */
	FVector2D GetDesignSize() const;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	struct FLine
	{
		abyss::LocText Loc;
		FString Resolved;
		abyss::LogType Type = abyss::LogType::System;
		double Time = 0.0;
	};

	FLinearColor TypeColor(abyss::LogType Type) const;
	float LineAlpha(int32 IndexFromNewest, double Age) const;

	TSharedPtr<FAbyssUiContext> Ctx;
	TSharedPtr<SVerticalBox> Lines;
	TArray<FLine> Buffer;
	bool bExpanded = true;
	bool bTouch = false;
	double NextCollapsedRefresh = 0.0;
};

/** 6.12 quest tracker (QuestTrackerHUD): guided / zone / other sorting, main before side, collapsed lines, expansion. */
class SAbyssQuestTracker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssQuestTracker) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	/** Rebuilds when the signature changed (called every 250 ms and on quest events). */
	void Refresh(const abyss::Snapshot& Snap, bool bForce);
	void SetTouch(bool bInTouch);
	float GetWidth() const { return bTouch ? 330.f : 218.f; }

private:
	struct FEntry
	{
		std::string QuestId;
		FString Title;
		FLinearColor TitleColor;
		FString Summary;
		FLinearColor SummaryColor;
		TArray<TPair<FString, bool>> Objectives;
		bool bCompleted = false;
		bool bMain = false;
		bool bGuided = false;
	};

	TSharedPtr<FAbyssUiContext> Ctx;
	TSharedPtr<SBox> Content;
	TSet<FString> ExpandedQuests;
	FString Signature;
	bool bTouch = false;
};

/** 6.7 target frame + 6.13 boss bar + 6.8 dodge plate + 6.9 info plate + buffs + pet medallion: painted from the state. */
class SAbyssTargetFrame : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssTargetFrame) {}
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TSharedPtr<FAbyssUiContext> Ctx;
	TSharedPtr<FAbyssHudState> State;
};

class SAbyssBossBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssBossBar) {}
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TSharedPtr<FAbyssUiContext> Ctx;
	TSharedPtr<FAbyssHudState> State;
	float Alpha = 0.f;
	float ShownFraction = 1.f;
};

class SAbyssInfoPlate : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssInfoPlate) {}
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TSharedPtr<FAbyssUiContext> Ctx;
	TSharedPtr<FAbyssHudState> State;
};

class SAbyssDodgePlate : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssDodgePlate) {}
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TSharedPtr<FAbyssUiContext> Ctx;
	TSharedPtr<FAbyssHudState> State;
};

/** Hero statuses (burn .. stun) and buffs with remaining-time sweeps. */
class SAbyssBuffRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssBuffRow) {}
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TSharedPtr<FAbyssUiContext> Ctx;
	TSharedPtr<FAbyssHudState> State;
};

/** 18.8 desktop pet medallion: active beast portrait + "P" badge; click toggles the pets panel. */
class SAbyssPetMedallion : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssPetMedallion) {}
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState);
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TSharedPtr<FAbyssUiContext> Ctx;
	TSharedPtr<FAbyssHudState> State;
};
