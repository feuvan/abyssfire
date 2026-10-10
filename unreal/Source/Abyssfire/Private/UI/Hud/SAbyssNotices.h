// SAbyssNotices: the transient HUD notices (save-ui-input 6.13): loot notices, zone / quest-complete / discovery /
// coming-soon banners, the level-up banner, the death text, toasts (core EvBanner{Toast} and UI toasts) and queued
// achievement toasts (FIX Q20). Everything is painted from timed entries on the UI's real-time clock.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include <string>

#include "abyss/base/I18n.h"
#include "abyss/items/Item.h"
#include "abyss/sim/SimTypes.h"

#include "UI/Core/AbyssUiContext.h"

struct FAbyssPainter;

class SAbyssNotices : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssNotices) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	void AddLootNotice(const std::string& BaseId, abyss::ItemQuality Quality, int32 Quantity, const FString& DisplayName);
	void ShowBanner(abyss::BannerKind Kind, const abyss::LocText& Title, const abyss::LocText& Subtitle, int32 LevelMin, int32 LevelMax);
	void ShowLevelUp(int32 Level);
	void ShowDeath(bool bShow);
	void ShowToast(const FText& Text, const FLinearColor& Color);
	void ShowAchievement(const std::string& AchievementId);
	void Clear();

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	struct FLootNotice
	{
		abyss::ItemInstance Item;
		FString Label;
		FLinearColor Color;
		double Start = 0.0;
	};
	struct FBanner
	{
		abyss::BannerKind Kind = abyss::BannerKind::Toast;
		FString Title;
		FString Subtitle;
		double Start = 0.0;
		double Duration = 0.0;
	};
	struct FToast
	{
		FString Text;
		FLinearColor Color;
		double Start = 0.0;
	};
	struct FAchievementToast
	{
		FString Title;
		FString Detail;
		double Start = -1.0;
	};

	void PaintLoot(FAbyssPainter& P, const FVector2D& Size, double Now) const;
	void PaintBanners(FAbyssPainter& P, const FVector2D& Size, double Now) const;
	void PaintToasts(FAbyssPainter& P, const FVector2D& Size, double Now) const;
	void PaintAchievement(FAbyssPainter& P, const FVector2D& Size, double Now) const;

	TSharedPtr<FAbyssUiContext> Ctx;
	mutable TArray<FLootNotice> Loot;           // newest first
	mutable TArray<FBanner> Banners;            // centre banners (quest complete / discovery / coming soon), queued
	mutable TOptional<FBanner> ZoneBanner;
	mutable double LevelUpStart = -100.0;
	int32 LevelUpLevel = 0;
	mutable double DeathStart = -100.0;
	mutable double DeathEnd = -100.0;
	mutable TArray<FToast> Toasts;
	mutable TArray<FAchievementToast> Achievements;  // queue: the first plays
};
