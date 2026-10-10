#include "UI/Panels/SAbyssWorldMapPanel.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Text/STextBlock.h"

#include <algorithm>
#include <string>
#include <vector>

#include "abyss/data/DataStore.h"
#include "abyss/quests/Achievements.h"

#include "Framework/AbyssText.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Hud/AbyssMinimapTexture.h"
#include "UI/Hud/SAbyssMinimap.h"
#include "UI/Widgets/AbyssUiWidgets.h"

void SAbyssWorldMapPanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext,
	const TSharedRef<FAbyssHudState>& InHudState, const TSharedRef<FAbyssMinimapTexture>& InMinimap)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	HudState = InHudState;
	Minimap = InMinimap;
	SetPanelContent(
		SNew(SAbyssPanelFrame, InContext)
		.Size(GetDesignSize())
		.Title(InContext->LocOr("ui.worldMap.title", TEXT("Abyssfire")))
		.ContentPadding(FMargin(0.f))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SAssignNew(Body, SCanvas)
		]);
}

void SAbyssWorldMapPanel::Refresh(const abyss::Snapshot& Snap)
{
	Body->ClearChildren();
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr)
	{
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const std::vector<std::string>& Order = Data->World().mapOrder;
	std::vector<std::string> Visited;
	if (Snap.achievements != nullptr)
	{
		Visited = Snap.achievements->ExploredZones();
	}
	if (!Snap.zone.mapId.empty() && std::find(Visited.begin(), Visited.end(), Snap.zone.mapId) == Visited.end())
	{
		Visited.push_back(Snap.zone.mapId);
	}

	// zone strip: 98 x 70 cards linked by gold arrows; the current zone carries a bobbing pin
	constexpr double CardW = 98.0;
	constexpr double CardH = 70.0;
	constexpr double Gap = 22.0;
	const int32 Count = static_cast<int32>(Order.size());
	const double StripW = Count * CardW + FMath::Max(0, Count - 1) * Gap;
	const double Left = (680.0 - StripW) * 0.5;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const std::string& MapId = Order[static_cast<size_t>(Index)];
		const abyss::MapDef* Map = Data->FindMap(MapId);
		const bool bVisited = std::find(Visited.begin(), Visited.end(), MapId) != Visited.end();
		const bool bCurrent = MapId == Snap.zone.mapId;
		FString Name = Ctx->ZoneName(MapId);
		FString Sub = Map != nullptr ? FString::Printf(TEXT("Lv.%d-%d"), Map->levelMin, Map->levelMax) : FString();
		if (!bVisited)
		{
			// U8: locked zones greyed with their chapter name
			if (const abyss::ChapterCard* Chapter = Data->Story().ChapterFor(MapId))
			{
				Name = Ctx->LocStr(Chapter->number);
				Sub = Ctx->LocStr(Chapter->title);
			}
		}
		const double X = Left + Index * (CardW + Gap);
		Body->AddSlot().Position(FVector2D(X, 64.0)).Size(FVector2D(CardW, CardH))
		[
			SNew(SAbyssCanvas, Context, [Context, Name, Sub, bVisited, bCurrent](FAbyssPainter& P, const FVector2D& Size)
			{
				const FAbyssUiPalette& Palette = P.Style.Colors();
				if (bCurrent)
				{
					P.Card(FVector2D::ZeroVector, Size, FAbyssUiStyle::Rgb(0x2a2114), Palette.GoldBright, FLinearColor::Transparent, FAbyssUiStyle::Rgb(0xffc860));
				}
				else
				{
					P.Card(FVector2D::ZeroVector, Size, bVisited ? Palette.Card : FAbyssUiStyle::Rgb(0x0e0c10),
						bVisited ? Palette.Gold : FAbyssUiStyle::Rgb(0x2e2a32));
				}
				const FSlateFontInfo NameFont = P.Style.Body(Name.Len() > 6 ? 11.f : 12.f, true, 1);
				P.TextCentered(FVector2D(Size.X * 0.5, Size.Y * 0.4), Name, NameFont, bVisited ? Palette.Parchment : Palette.Dim);
				P.TextCentered(FVector2D(Size.X * 0.5, Size.Y * 0.72), Sub, P.Style.Body(10.f, false, 1), bVisited ? Palette.TextSoft : Palette.Faint);
				if (!bVisited)
				{
					P.Diamond(FVector2D(Size.X - 10.0, 10.0), 8.f, FAbyssUiStyle::Rgb(0x3f3845));
				}
				if (bCurrent)
				{
					// bobbing pin (+-3 px, 600 ms yoyo)
					const double Bob = 3.0 * FMath::Sin(Context->Now() * UE_PI / 0.6);
					const FVector2D Tip(Size.X * 0.5, -4.0 + Bob);
					TArray<FVector2D> Pin;
					Pin.Add(Tip);
					Pin.Add(Tip + FVector2D(-6.0, -10.0));
					Pin.Add(Tip + FVector2D(6.0, -10.0));
					P.ConvexPolygon(Pin, FAbyssUiStyle::Rgb(0xe74c3c));
					P.Circle(Tip + FVector2D(0.0, -13.0), 6.f, FAbyssUiStyle::Rgb(0xe74c3c), FAbyssUiStyle::Rgb(0x5a0f0a), 1.f);
					P.Circle(Tip + FVector2D(0.0, -13.0), 2.2f, FAbyssUiStyle::Rgb(0xffe0d6));
				}
			})
		];
		if (Index + 1 < Count)
		{
			Body->AddSlot().Position(FVector2D(X + CardW, 64.0 + CardH * 0.5 - 6.0)).Size(FVector2D(Gap, 12.0))
			[
				SNew(SAbyssCanvas, Context, [](FAbyssPainter& P, const FVector2D& Size)
				{
					const FLinearColor Gold = P.Style.Colors().Gold;
					P.Line(FVector2D(3.0, Size.Y * 0.5), FVector2D(Size.X - 6.0, Size.Y * 0.5), Gold, 1.5f);
					TArray<FVector2D> Head;
					Head.Add(FVector2D(Size.X - 2.0, Size.Y * 0.5));
					Head.Add(FVector2D(Size.X - 8.0, Size.Y * 0.5 - 4.0));
					Head.Add(FVector2D(Size.X - 8.0, Size.Y * 0.5 + 4.0));
					P.ConvexPolygon(Head, Gold);
				})
			];
		}
	}

	// the current zone: whole map, explored fog, north up (W2 / U8)
	constexpr double MapSize = 380.0;
	Body->AddSlot().Position(FVector2D(24.0, 154.0)).Size(FVector2D(MapSize, MapSize))
	[
		SNew(SAbyssMinimap, Context, HudState.ToSharedRef(), Minimap.ToSharedRef())
		.Size(static_cast<float>(MapSize))
		.Rotate(false)
		.WholeZone(true)
		.Interactive(false)
	];

	// legend column
	int32 ExploredTiles = 0;
	const int32 Cols = Minimap->GetCols();
	const int32 Rows = Minimap->GetRows();
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Col = 0; Col < Cols; ++Col)
		{
			ExploredTiles += Minimap->IsExplored(Col, Row) ? 1 : 0;
		}
	}
	const int32 Percent = Cols * Rows > 0 ? FMath::RoundToInt(100.0 * ExploredTiles / (static_cast<double>(Cols) * Rows)) : 0;
	TSharedRef<SVerticalBox> Legend = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Snap.zone.nameKey.empty() ? Ctx->ZoneName(Snap.zone.mapId)
				: Ctx->NameOr(Snap.zone.nameKey, Snap.zone.mapId)), 17.f, C.Parchment, true, 2, 240.f)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))
		[
			AbyssUi::Label(*Ctx, FText::AsCultureInvariant(FString::Printf(TEXT("Lv.%d-%d"), Snap.zone.levelMin, Snap.zone.levelMax)), 12.f, C.TextSoft)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 10.f))
		[
			AbyssUi::Label(*Ctx, Ctx->LocArgsOr("ui.worldMap.explored", TEXT("Explored {percent}%"), { FAbyssUiContext::Arg("percent", Percent) }),
				12.f, C.Heading, true)
		];
	struct FLegendRow
	{
		const char* Key;
		const TCHAR* Fallback;
		uint32 Color;
		int32 Shape;  // 0 circle, 1 square, 2 star
	};
	static const FLegendRow LegendRows[] = {
		{ "ui.worldMap.legend.hero", TEXT("You"), 0x7fd4ff, 0 },
		{ "ui.worldMap.legend.questGiver", TEXT("Quest giver"), 0xffd23a, 0 },
		{ "ui.worldMap.legend.guide", TEXT("Quest target"), 0xffb347, 2 },
		{ "ui.worldMap.legend.exit", TEXT("Exit"), 0x00e676, 1 },
		{ "ui.worldMap.legend.monster", TEXT("Monster"), 0xff4444, 0 },
		{ "ui.worldMap.legend.questArea", TEXT("Quest area"), 0xf1c40f, 0 },
	};
	for (const FLegendRow& Row : LegendRows)
	{
		const uint32 Color = Row.Color;
		const int32 Shape = Row.Shape;
		Legend->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 3.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
				[
					SNew(SAbyssCanvas, Context, [Color, Shape](FAbyssPainter& P, const FVector2D& Size)
					{
						const FVector2D Center = Size * 0.5;
						if (Shape == 1)
						{
							P.Box(Center - FVector2D(4.0, 4.0), FVector2D(8.0, 8.0), FAbyssUiStyle::Rgb(Color));
						}
						else if (Shape == 2)
						{
							P.Star(Center, 6.f, 2.7f, FAbyssUiStyle::Rgb(Color));
						}
						else
						{
							P.Circle(Center, 4.f, FAbyssUiStyle::Rgb(Color), FLinearColor(0.f, 0.f, 0.f, 0.7f), 1.f);
						}
					})
					.Size(FVector2D(14.0, 14.0))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					AbyssUi::Label(*Ctx, Ctx->LocOr(Row.Key, Row.Fallback), 12.f, C.Text)
				]
			];
	}
	Body->AddSlot().Position(FVector2D(424.0, 154.0)).Size(FVector2D(236.0, MapSize))
	[
		Legend
	];
	if (!Ctx->IsTouch())
	{
		Body->AddSlot().Position(FVector2D(0.0, 536.0)).Size(FVector2D(680.0, 18.0))
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			[
				AbyssUi::Label(*Ctx, Ctx->LocOr("ui.worldMap.closeHint", TEXT("Press M to close")), 11.f, C.Muted)
			]
		];
	}
}
