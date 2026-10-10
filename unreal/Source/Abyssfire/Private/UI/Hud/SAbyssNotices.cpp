#include "UI/Hud/SAbyssNotices.h"

#include "Widgets/SNullWidget.h"

#include "abyss/data/DataStore.h"

#include "Framework/AbyssText.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Core/AbyssUiDraw.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	/** Fade in / hold / fade out envelope (seconds). */
	float AbyssNotices_Envelope(double T, double In, double Hold, double Out)
	{
		if (T < 0.0)
		{
			return 0.f;
		}
		if (T < In)
		{
			return static_cast<float>(T / In);
		}
		if (T < Hold)
		{
			return 1.f;
		}
		if (T < Hold + Out)
		{
			return static_cast<float>(1.0 - (T - Hold) / Out);
		}
		return 0.f;
	}
}

void SAbyssNotices::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Ctx = InContext;
	SetVisibility(EVisibility::HitTestInvisible);
	ChildSlot
	[
		SNullWidget::NullWidget
	];
}

void SAbyssNotices::Clear()
{
	Loot.Reset();
	Banners.Reset();
	ZoneBanner.Reset();
	LevelUpStart = -100.0;
	DeathStart = DeathEnd = -100.0;
	Toasts.Reset();
	Achievements.Reset();
}

void SAbyssNotices::AddLootNotice(const std::string& BaseId, abyss::ItemQuality Quality, int32 Quantity, const FString& DisplayName)
{
	// 6.13: newest at the anchor, max 4 (oldest dropped), 3200 ms then a 400 ms fade.
	FLootNotice Notice;
	Notice.Item.baseId = BaseId;
	Notice.Item.quality = Quality;
	Notice.Item.quantity = 1;
	Notice.Label = Quantity > 1 ? FString::Printf(TEXT("%s x%d"), *DisplayName, Quantity) : DisplayName;
	Notice.Color = Ctx->Style().QualityColor(Quality);
	Notice.Start = Ctx->Now();
	Loot.Insert(MoveTemp(Notice), 0);
	if (Loot.Num() > 4)
	{
		Loot.SetNum(4);
	}
}

void SAbyssNotices::ShowBanner(abyss::BannerKind Kind, const abyss::LocText& Title, const abyss::LocText& Subtitle, int32 LevelMin, int32 LevelMax)
{
	const double Now = Ctx->Now();
	FBanner Banner;
	Banner.Kind = Kind;
	Banner.Title = Title.Empty() ? FString() : Ctx->LocStr(Title);
	Banner.Subtitle = Subtitle.Empty() ? FString() : Ctx->LocStr(Subtitle);
	Banner.Start = Now;
	switch (Kind)
	{
	case abyss::BannerKind::Zone:
		// zone name + "Lv.{min}-{max}": fade in 800, hold to 3000, fade out 800
		Banner.Subtitle = FString::Printf(TEXT("Lv.%d-%d"), LevelMin, LevelMax);
		Banner.Duration = 3.8;
		ZoneBanner = Banner;
		return;
	case abyss::BannerKind::Death:
		ShowDeath(true);
		return;
	case abyss::BannerKind::Toast:
		ShowToast(FText::AsCultureInvariant(Banner.Title), Ctx->Style().Colors().Good);
		return;
	case abyss::BannerKind::LevelUp:
		return;   // EvLevelUp drives the level-up banner
	case abyss::BannerKind::Achievement:
		ShowToast(FText::AsCultureInvariant(Banner.Title), Ctx->Style().Colors().GoldBright);
		return;
	case abyss::BannerKind::QuestComplete:
		Banner.Duration = 4.1;   // in 500, hold 3000, out 600
		break;
	case abyss::BannerKind::Discovery:
	case abyss::BannerKind::ComingSoon:
		Banner.Duration = 3.6;
		break;
	}
	// queue: a banner starts when the previous one ends
	if (Banners.Num() > 0)
	{
		const FBanner& Last = Banners.Last();
		Banner.Start = FMath::Max(Now, Last.Start + Last.Duration);
	}
	Banners.Add(MoveTemp(Banner));
}

void SAbyssNotices::ShowLevelUp(int32 Level)
{
	LevelUpStart = Ctx->Now();
	LevelUpLevel = Level;
}

void SAbyssNotices::ShowDeath(bool bShow)
{
	const double Now = Ctx->Now();
	if (bShow)
	{
		DeathStart = Now;
		DeathEnd = -100.0;
	}
	else if (DeathStart > 0.0 && DeathEnd < DeathStart)
	{
		DeathEnd = Now;
	}
}

void SAbyssNotices::ShowToast(const FText& Text, const FLinearColor& Color)
{
	if (Text.IsEmpty())
	{
		return;
	}
	FToast Toast;
	Toast.Text = Text.ToString();
	Toast.Color = Color;
	Toast.Start = Ctx->Now();
	Toasts.Add(MoveTemp(Toast));
	if (Toasts.Num() > 4)
	{
		Toasts.RemoveAt(0);
	}
}

void SAbyssNotices::ShowAchievement(const std::string& AchievementId)
{
	const abyss::DataStore* Data = Ctx->GetData();
	const abyss::AchievementDef* Def = Data ? Data->Quests().FindAchievement(AchievementId) : nullptr;
	FAchievementToast Toast;
	const FString Name = Ctx->NameOr("data.achievement." + AchievementId + ".name", Def ? Def->name : AchievementId);
	Toast.Title = Ctx->LocArgsOrStr("ui.achievement.toastUnlock", TEXT("Achievement unlocked: {name}"), { FAbyssUiContext::Arg("name", Name) });
	if (Def != nullptr)
	{
		// FIX Q17: localized description / title / stat label.
		FString Detail = Ctx->NameOr("data.achievement." + AchievementId + ".desc", Def->description);
		if (Def->hasReward)
		{
			Detail += FString::Printf(TEXT("  |  %s+%s"), *Ctx->StatLabel(Def->rewardStat), *FAbyssUiContext::Num(Def->rewardValue));
		}
		if (!Def->title.empty())
		{
			const FString Title = Ctx->NameOr("data.achievement." + AchievementId + ".title", Def->title);
			Detail += TEXT("  ") + Ctx->LocArgsOrStr("ui.achievement.toastTitle", TEXT("Title: {title}"), { FAbyssUiContext::Arg("title", Title) });
		}
		Toast.Detail = Detail;
	}
	Achievements.Add(MoveTemp(Toast));
}

// =====================================================================================================================
// Paint
// =====================================================================================================================

int32 SAbyssNotices::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const double Now = Ctx->Now();
	FAbyssPainter P(AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, Ctx->Style());
	const FVector2D Size(AllottedGeometry.GetLocalSize());
	PaintLoot(P, Size, Now);
	PaintBanners(P, Size, Now);
	PaintToasts(P, Size, Now);
	PaintAchievement(P, Size, Now);
	return P.Layer;
}

void SAbyssNotices::PaintLoot(FAbyssPainter& P, const FVector2D& Size, double Now) const
{
	// expire
	Loot.RemoveAll([Now](const FLootNotice& Notice) { return Now - Notice.Start > 3.6; });
	const bool bTouch = Ctx->IsTouch();
	const double Scale = bTouch ? 1.45 : 1.0;
	const FVector2D CardSize(236.0 * Scale, 34.0 * Scale);
	// desktop anchor (1032, 674) on the right edge; touch (388, 572) on the left
	const FVector2D Anchor = bTouch ? FVector2D(388.0, Size.Y - 148.0) : FVector2D(Size.X - 248.0, Size.Y - 46.0);
	for (int32 Index = 0; Index < Loot.Num(); ++Index)
	{
		const FLootNotice& Notice = Loot[Index];
		const double T = Now - Notice.Start;
		// older notices slide up by 40 px in 150 ms when a new one arrives
		const double Rise = 40.0 * Scale * Index;
		const float In = AbyssEase::CubicOut(AbyssEase::Clamp01(static_cast<float>(T / 0.22)));
		const float Out = T > 3.2 ? 1.f - AbyssEase::Clamp01(static_cast<float>((T - 3.2) / 0.4)) : 1.f;
		const float Alpha = In * Out;
		if (Alpha <= 0.f)
		{
			continue;
		}
		const FVector2D Pos = Anchor + FVector2D(24.0 * (1.0 - In), -Rise);
		FAbyssPainter Faded = P;
		Faded.Tint = P.Tint * FLinearColor(1.f, 1.f, 1.f, Alpha);
		Faded.Frame(Pos, CardSize, 1, Notice.Color, 0.f, 0.95f);
		const float SlotSize = static_cast<float>(26.0 * Scale);
		SAbyssItemSlot::PaintItem(Faded, *Ctx, Pos + FVector2D(5.0 * Scale, (CardSize.Y - SlotSize) * 0.5), SlotSize, &Notice.Item, false);
		const FSlateFontInfo Font = Ctx->Style().Body(static_cast<float>(12.0 * Scale), true, 2);
		const FVector2D LabelSize = FAbyssPainter::Measure(Notice.Label, Font);
		const double MaxW = CardSize.X - SlotSize - 18.0 * Scale;
		const double Squeeze = LabelSize.X > MaxW ? MaxW / LabelSize.X : 1.0;
		Faded.TextCentered(Pos + FVector2D(SlotSize + 10.0 * Scale + FMath::Min(LabelSize.X, MaxW) * 0.5, CardSize.Y * 0.5), Notice.Label, Font,
			Notice.Color, static_cast<float>(Squeeze));
	}
}

void SAbyssNotices::PaintBanners(FAbyssPainter& P, const FVector2D& Size, double Now) const
{
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const double CenterX = Size.X * 0.5;

	// zone banner at 32 % height: name 28 px Cinzel #c0934a, Lv line 16 px #8a7a5a, two 120 px rules
	if (ZoneBanner.IsSet())
	{
		const double T = Now - ZoneBanner->Start;
		const float Alpha = AbyssNotices_Envelope(T, 0.8, 3.0, 0.8);
		if (T > 3.8)
		{
			ZoneBanner.Reset();
		}
		else if (Alpha > 0.f)
		{
			const double Y = Size.Y * 0.32;
			const FLinearColor Gold = FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xc0934a), Alpha);
			P.TextCentered(FVector2D(CenterX, Y), ZoneBanner->Title, Ctx->Style().Title(28.f, true, 3), Gold);
			P.TextCentered(FVector2D(CenterX, Y + 32.0), ZoneBanner->Subtitle, Ctx->Style().Title(16.f, false, 2),
				FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0x8a7a5a), Alpha));
			P.Line(FVector2D(CenterX - 200.0, Y + 16.0), FVector2D(CenterX - 80.0, Y + 16.0), Gold, 1.f);
			P.Line(FVector2D(CenterX + 80.0, Y + 16.0), FVector2D(CenterX + 200.0, Y + 16.0), Gold, 1.f);
		}
	}

	// level-up at y 202: text 32 px #ffd700 scale 0.5 -> 1 (400 ms Back.easeOut), level 20 px; at 2.5 s fade + rise 20 px
	{
		const double T = Now - LevelUpStart;
		if (T >= 0.0 && T < 3.1)
		{
			const float Pop = 0.5f + 0.5f * AbyssEase::BackOut(AbyssEase::Clamp01(static_cast<float>(T / 0.4)));
			const float Fade = T > 2.5 ? 1.f - AbyssEase::Clamp01(static_cast<float>((T - 2.5) / 0.6)) : 1.f;
			const double Rise = T > 2.5 ? 20.0 * AbyssEase::Clamp01(static_cast<float>((T - 2.5) / 0.6)) : 0.0;
			const double Y = Size.Y * (202.0 / 720.0) - Rise;
			P.TextCentered(FVector2D(CenterX, Y), Ctx->LocOrStr("zone.levelUp.text", TEXT("Level Up!")), Ctx->Style().Title(32.f, true, 4),
				FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xffd700), Fade), Pop);
			P.TextCentered(FVector2D(CenterX, Y + 38.0),
				Ctx->LocArgsOrStr("zone.levelUp.level", TEXT("Level {level}"), { FAbyssUiContext::Arg("level", LevelUpLevel) }),
				Ctx->Style().Body(20.f, true, 3), FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xffcc00), Fade));
		}
	}

	// centre banners (quest complete at 22 % height, discovery / coming soon at 30 %), one at a time
	Banners.RemoveAll([Now](const FBanner& Banner) { return Now > Banner.Start + Banner.Duration; });
	if (Banners.Num() > 0)
	{
		const FBanner& Banner = Banners[0];
		const double T = Now - Banner.Start;
		if (T >= 0.0)
		{
			const bool bQuest = Banner.Kind == abyss::BannerKind::QuestComplete;
			const float Alpha = bQuest ? AbyssNotices_Envelope(T, 0.5, 3.5, 0.6) : AbyssNotices_Envelope(T, 0.4, 3.0, 0.6);
			const double Y = Size.Y * (bQuest ? 0.22 : 0.30);
			if (bQuest)
			{
				P.TextCentered(FVector2D(CenterX, Y), Ctx->LocOrStr("zone.questComplete", TEXT("Quest complete!")), Ctx->Style().Title(26.f, true, 3),
					FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xf1c40f), Alpha));
				P.TextCentered(FVector2D(CenterX, Y + 32.0), Banner.Title, Ctx->Style().Body(18.f, true, 3), FAbyssUiStyle::WithAlpha(C.Parchment, Alpha));
				if (!Banner.Subtitle.IsEmpty())
				{
					P.TextCentered(FVector2D(CenterX, Y + 58.0), Banner.Subtitle, Ctx->Style().Body(14.f, false, 2),
						FAbyssUiStyle::WithAlpha(C.TextSoft, Alpha));
				}
			}
			else
			{
				const FLinearColor Color = Banner.Kind == abyss::BannerKind::ComingSoon ? C.Heading : FAbyssUiStyle::Rgb(0x9fe0ff);
				P.Divider(FVector2D(CenterX, Y - 22.0), 260.f, true, FAbyssUiStyle::WithAlpha(C.Gold, Alpha));
				P.TextCentered(FVector2D(CenterX, Y), Banner.Title, Ctx->Style().Title(22.f, true, 3), FAbyssUiStyle::WithAlpha(Color, Alpha));
				if (!Banner.Subtitle.IsEmpty())
				{
					P.TextCentered(FVector2D(CenterX, Y + 28.0), Banner.Subtitle, Ctx->Style().Body(14.f, false, 2), FAbyssUiStyle::WithAlpha(C.TextSoft, Alpha));
				}
				P.Divider(FVector2D(CenterX, Y + 22.0 + (Banner.Subtitle.IsEmpty() ? 0.0 : 22.0)), 260.f, false, FAbyssUiStyle::WithAlpha(C.Gold, Alpha));
			}
		}
	}

	// death text (combat-feel 13.3): big centred text, fade in 250 ms, out 250 ms at the respawn
	if (DeathStart > 0.0)
	{
		const double In = AbyssEase::Clamp01(static_cast<float>((Now - DeathStart) / 0.25));
		double Alpha = In;
		if (DeathEnd >= DeathStart)
		{
			Alpha *= 1.0 - AbyssEase::Clamp01(static_cast<float>((Now - DeathEnd) / 0.25));
			if (Now - DeathEnd > 0.25)
			{
				DeathStart = -100.0;
			}
		}
		if (Alpha > 0.0)
		{
			P.TextCentered(FVector2D(CenterX, Size.Y * 0.42), Ctx->LocOrStr("zone.death.text", TEXT("You died")), Ctx->Style().Title(44.f, true, 4),
				FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xd03a2a), static_cast<float>(Alpha)));
		}
	}
}

void SAbyssNotices::PaintToasts(FAbyssPainter& P, const FVector2D& Size, double Now) const
{
	Toasts.RemoveAll([Now](const FToast& Toast) { return Now - Toast.Start > 2.4; });
	double Y = Size.Y * 0.16;
	for (const FToast& Toast : Toasts)
	{
		const double T = Now - Toast.Start;
		const float Alpha = AbyssNotices_Envelope(T, 0.15, 2.0, 0.4);
		if (Alpha <= 0.f)
		{
			continue;
		}
		const FSlateFontInfo Font = Ctx->Style().Body(15.f, true, 2);
		const FVector2D TextSize = FAbyssPainter::Measure(Toast.Text, Font);
		const FVector2D BoxSize(TextSize.X + 36.0, 30.0);
		const FVector2D Pos(Size.X * 0.5 - BoxSize.X * 0.5, Y);
		FAbyssPainter Faded = P;
		Faded.Tint = P.Tint * FLinearColor(1.f, 1.f, 1.f, Alpha);
		Faded.Frame(Pos, BoxSize, 2, Toast.Color, 0.f, 0.92f);
		Faded.TextCentered(Pos + BoxSize * 0.5, Toast.Text, Font, Toast.Color);
		Y += 36.0;
	}
}

void SAbyssNotices::PaintAchievement(FAbyssPainter& P, const FVector2D& Size, double Now) const
{
	if (Achievements.Num() == 0)
	{
		return;
	}
	FAchievementToast& Toast = Achievements[0];
	if (Toast.Start < 0.0)
	{
		Toast.Start = Now;
	}
	const double T = Now - Toast.Start;
	if (T > 3.8)
	{
		Achievements.RemoveAt(0);
		return;
	}
	// 360x62 at (460, 60 -> 70): in 400 ms Back.easeOut; at 3500 ms out (alpha 0, y -> 40) over 300 ms
	float Alpha = AbyssEase::Clamp01(static_cast<float>(T / 0.4));
	double Y = 60.0 + 10.0 * AbyssEase::BackOut(Alpha);
	if (T > 3.5)
	{
		const float Out = AbyssEase::Clamp01(static_cast<float>((T - 3.5) / 0.3));
		Alpha = 1.f - Out;
		Y = 70.0 - 30.0 * Out;
	}
	const FVector2D BoxSize(360.0, 62.0);
	const FVector2D Pos(Size.X * 0.5 - BoxSize.X * 0.5, Y);
	FAbyssPainter Faded = P;
	Faded.Tint = P.Tint * FLinearColor(1.f, 1.f, 1.f, Alpha);
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	Faded.Frame(Pos, BoxSize, 1, C.GoldBright, 0.f, 0.96f);
	const FVector2D Medal = Pos + FVector2D(31.0, 31.0);
	Faded.Circle(Medal, 20.f, C.GoldDark, C.Gold, 2.f);
	Faded.Star(Medal, 12.f, 5.f, C.GoldBright);
	Faded.Text(Pos + FVector2D(60.0, 10.0), Toast.Title, Ctx->Style().Body(14.f, true, 2), C.GoldBright);
	Faded.Text(Pos + FVector2D(60.0, 34.0), Toast.Detail, Ctx->Style().Body(11.f, false, 1), C.TextSoft);
}
