#include "UI/Panels/SAbyssPetsPanel.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include <string>
#include <vector>

#include "abyss/data/DataStore.h"
#include "abyss/items/Inventory.h"
#include "abyss/pets/PetSystem.h"

#include "Framework/AbyssText.h"
#include "UI/AbyssUiStyle.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	/** The key's text, or "" while the key is missing (optional description lines). */
	FString AbyssPets_Optional(const FAbyssUiContext& Ctx, const std::string& Key)
	{
		return Ctx.HasKey(Key) ? Ctx.LocStr(Key) : FString();
	}

	FLinearColor AbyssPets_RarityColor(abyss::PetRarity Rarity)
	{
		switch (Rarity)
		{
		case abyss::PetRarity::Rare: return FAbyssUiStyle::Rgb(0x4f8cff);
		case abyss::PetRarity::Epic: return FAbyssUiStyle::Rgb(0xb36bff);
		case abyss::PetRarity::Common: break;
		}
		return FAbyssUiStyle::Rgb(0xe0d8cc);
	}

	/** Portrait medallion (art R11), fallback: the beast's colour and initial. */
	TSharedRef<SWidget> AbyssPets_Portrait(const TSharedRef<FAbyssUiContext>& Ctx, const std::string& PetId, uint32 Color, const FString& Name,
		float Diameter, bool bDim)
	{
		return SNew(SAbyssCanvas, Ctx, [Ctx, PetId, Color, Name, bDim](FAbyssPainter& P, const FVector2D& Size)
		{
			const FVector2D Center = Size * 0.5;
			const float R = static_cast<float>(FMath::Min(Size.X, Size.Y) * 0.5);
			P.Circle(Center, R, FAbyssUiStyle::Rgb(0x0c0b0e), P.Style.Colors().Gold, 1.5f);
			P.Glow(Center, R - 2.f, FAbyssUiStyle::Rgb(Color, bDim ? 0.15f : 0.4f), 6);
			if (const FSlateBrush* Portrait = Ctx->PetPortrait(PetId))
			{
				P.Brush(Center - FVector2D(R - 3.0, R - 3.0), FVector2D((R - 3.0) * 2.0, (R - 3.0) * 2.0), Portrait,
					bDim ? FLinearColor(0.5f, 0.5f, 0.5f, 1.f) : FLinearColor::White);
			}
			else
			{
				P.TextCentered(Center, Name.Left(1), P.Style.Font(EAbyssFontFace::Title, R * 0.9f, true, 2), FAbyssUiStyle::Rgb(Color));
			}
		})
		.Size(FVector2D(Diameter, Diameter));
	}
}

void SAbyssPetsPanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	SetPanelContent(
		SNew(SAbyssPanelFrame, InContext)
		.Size(GetDesignSize())
		.Title(InContext->LocOr("ui.pet.title", TEXT("Ley-Beasts")))
		.ContentPadding(FMargin(0.f))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SAssignNew(Body, SCanvas)
		]);
}

void SAbyssPetsPanel::Refresh(const abyss::Snapshot& Snap)
{
	Body->ClearChildren();
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr || Snap.pets == nullptr)
	{
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const abyss::PetSystem& Pets = *Snap.pets;
	const abyss::PetTables& Tables = Data->Pets();
	const bool bDying = Ctx->IsHeroDying();
	const bool bTouch = Ctx->IsTouch();

	// order: the active beast, then the other owned ones in acquisition order (Q3: owned beasts only)
	std::vector<const abyss::PetInstance*> Owned;
	if (const abyss::PetInstance* Active = Pets.Active())
	{
		Owned.push_back(Active);
	}
	for (const abyss::PetInstance& Pet : Pets.Owned())
	{
		if (Pet.petId != Pets.ActiveId())
		{
			Owned.push_back(&Pet);
		}
	}
	const int32 Fruit = Snap.inventory != nullptr ? Snap.inventory->CountOf(Tables.leyFruitId) : 0;

	// header: owned count + fruit counter
	Body->AddSlot().Position(FVector2D(16.0, 44.0)).Size(FVector2D(628.0, 22.0))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			AbyssUi::Label(*Ctx, Ctx->LocArgsOr("ui.pet.owned", TEXT("Met {count}/{total}"),
				{ FAbyssUiContext::Arg("count", static_cast<int64>(Owned.size())),
					FAbyssUiContext::Arg("total", static_cast<int64>(Tables.chapter1Slice.empty() ? Tables.pets.size() : Tables.chapter1Slice.size())) }),
				13.f, C.Heading, true, 1)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			AbyssUi::Label(*Ctx, FText::AsCultureInvariant(FString::Printf(TEXT("%s \x00D7 %d"), *Ctx->ItemBaseName(Tables.leyFruitId), Fruit)), 12.f,
				Fruit > 0 ? C.Good : C.Dim, true, 1)
		]
	];

	if (Owned.empty())
	{
		Body->AddSlot().Position(FVector2D(60.0, 160.0)).Size(FVector2D(540.0, 200.0))
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			[
				AbyssUi::Label(*Ctx, Ctx->LocOr("ui.pet.none", TEXT("No ley-beast follows you yet.")), 14.f, C.TextSoft, false, 1, 520.f)
			]
		];
		return;
	}
	bool bSelectedOwned = false;
	for (const abyss::PetInstance* Pet : Owned)
	{
		bSelectedOwned = bSelectedOwned || Pet->petId == SelectedPet;
	}
	if (!bSelectedOwned)
	{
		SelectedPet = Owned.front()->petId;
	}

	// list (left, 208 px)
	double Y = 74.0;
	for (const abyss::PetInstance* Pet : Owned)
	{
		const abyss::PetDef* Def = Data->FindPet(Pet->petId);
		if (Def == nullptr)
		{
			continue;
		}
		const std::string PetId = Pet->petId;
		const bool bSelected = PetId == SelectedPet;
		const bool bActive = PetId == Pets.ActiveId();
		const bool bAway = Pets.IsAway(PetId);
		const FString Name = Ctx->PetName(PetId, Pet->evolved);
		const FString Sub = FString::Printf(TEXT("Lv.%d%s"), Pet->level,
			bActive ? *(TEXT("  \x00B7 ") + Ctx->LocOrStr("ui.pet.active", TEXT("Active"))) : (bAway ? *(TEXT("  \x00B7 ") + Ctx->LocOrStr("ui.pet.away", TEXT("Away"))) : TEXT("")));
		const FLinearColor RarityColor = AbyssPets_RarityColor(Def->rarity);
		const uint32 PetColor = Def->color;
		Body->AddSlot().Position(FVector2D(16.0, Y)).Size(FVector2D(208.0, 58.0))
		[
			SNew(SAbyssHitArea, Context)
			.OnPressed_Lambda([this, PetId]()
			{
				SelectedPet = PetId;
				MarkDirty();
			})
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SAbyssCanvas, Context, [bSelected, bActive](FAbyssPainter& P, const FVector2D& Size)
					{
						const FAbyssUiPalette& Palette = P.Style.Colors();
						P.Card(FVector2D::ZeroVector, Size, bSelected ? Palette.CardHover : Palette.Card, bSelected ? Palette.Gold : FAbyssUiStyle::Rgb(0x3a343f),
							bActive ? FAbyssUiStyle::Rgb(0x6fd35a) : FLinearColor::Transparent);
					})
				]
				+ SOverlay::Slot()
				.Padding(FMargin(8.f, 6.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
					[
						AbyssPets_Portrait(Context, PetId, PetColor, Name, 44.f, bAway)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Name), 13.f, RarityColor, true, 1, 140.f)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Sub), 11.f, bActive ? C.Good : C.TextSoft, false, 1)
						]
					]
				]
			]
		];
		Y += 64.0;
	}

	// detail (right)
	const abyss::PetInstance* Pet = Pets.Find(SelectedPet);
	const abyss::PetDef* Def = Pet != nullptr ? Data->FindPet(Pet->petId) : nullptr;
	if (Pet == nullptr || Def == nullptr)
	{
		return;
	}
	const std::string PetId = Pet->petId;
	const FString Name = Ctx->PetName(PetId, Pet->evolved);
	const bool bActive = PetId == Pets.ActiveId();
	const bool bAway = Pets.IsAway(PetId);
	const std::string RoleId(abyss::EnumName(Def->role));
	const FString Role = Ctx->NameOr("data.pet.role." + RoleId, RoleId);
	const FString Stage = Ctx->LocOrStr("ui.pet.evo." + std::to_string(FMath::Clamp(Pet->evolved, 0, 2)), TEXT(""));
	const FString Description = AbyssPets_Optional(*Ctx, Def->descKey.empty() ? "data.pet." + PetId + ".desc" : Def->descKey);
	const bool bMaxLevel = Pet->level >= Tables.maxLevel;
	const int64 Need = abyss::PetExpToNext(Tables.system, Pet->level);
	const float ExpFraction = bMaxLevel ? 1.f : (Need > 0 ? static_cast<float>(static_cast<double>(Pet->exp) / static_cast<double>(Need)) : 0.f);
	int32 NextEvoLevel = 0;
	for (const int32 Level : Tables.evolutionLevels)
	{
		if (Level > Pet->level)
		{
			NextEvoLevel = Level;
			break;
		}
	}
	const int32 BondCap = Pets.BondCap();
	const int32 Bond = Pet->bond;
	const double Passive = abyss::PetPassiveValue(Tables, *Def, *Pet);
	const std::string StatId(abyss::EnumName(Def->passiveStat));
	const FString PassiveText = Ctx->LocArgsOrStr("data.pet.stat." + StatId, TEXT("{value}"), { FAbyssUiContext::Arg("value", FAbyssUiContext::Num(Passive)) });

	TSharedRef<SVerticalBox> Detail = SNew(SVerticalBox);
	Detail->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 14.f, 0.f))
			[
				AbyssPets_Portrait(Context, PetId, Def->color, Name, 96.f, bAway)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Name), 18.f, AbyssPets_RarityColor(Def->rarity), true, 2, 280.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))
				[
					AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Stage.IsEmpty() ? Role : Role + TEXT(" \x00B7 ") + Stage), 12.f, C.TextSoft, false, 1)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))
				[
					AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Description), 11.f, C.Muted, false, 1, 280.f)
				]
			]
		];
	// level / exp
	Detail->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 12.f, 0.f, 0.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				AbyssUi::Label(*Ctx, bMaxLevel ? Ctx->LocArgsOr("ui.pet.levelMax", TEXT("Lv.{level} \x00B7 Max"), { FAbyssUiContext::Arg("level", Pet->level) })
					: Ctx->LocArgsOr("ui.pet.level", TEXT("Lv.{level}"), { FAbyssUiContext::Arg("level", Pet->level) }), 13.f, C.Parchment, true, 1)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f, 0.f))
			[
				SNew(SAbyssCanvas, Context, [ExpFraction](FAbyssPainter& P, const FVector2D& Size)
				{
					P.Bar(FVector2D::ZeroVector, Size, ExpFraction, FAbyssUiStyle::Rgb(0x8be9fd), true);
				})
				.Size(FVector2D(170.0, 10.0))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				AbyssUi::Label(*Ctx, bMaxLevel ? FText::GetEmpty() : Ctx->LocArgsOr("ui.pet.exp", TEXT("{exp}/{need}"),
					{ FAbyssUiContext::Arg("exp", Pet->exp), FAbyssUiContext::Arg("need", Need) }), 11.f, C.TextSoft, false, 1)
			]
		];
	if (NextEvoLevel > 0)
	{
		Detail->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 2.f, 0.f, 0.f))
			[
				AbyssUi::Label(*Ctx, Ctx->LocArgsOr("ui.pet.nextEvo", TEXT("Evolves at Lv.{level}"), { FAbyssUiContext::Arg("level", NextEvoLevel) }), 11.f,
					FAbyssUiStyle::Rgb(0xffd98a), false, 1)
			];
	}
	// bond: five pips, those above the cap dimmed (the Moon Well note is part of the hidden tower UI, Q3)
	Detail->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 10.f, 0.f, 0.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
			[
				AbyssUi::Label(*Ctx, Ctx->LocOr("ui.pet.bond", TEXT("Bond")), 12.f, C.Heading, true, 1)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SAbyssCanvas, Context, [Bond, BondCap, MaxBond = Tables.maxBond](FAbyssPainter& P, const FVector2D& Size)
				{
					for (int32 Pip = 0; Pip < MaxBond; ++Pip)
					{
						const FVector2D Center(9.0 + Pip * 20.0, Size.Y * 0.5);
						const bool bFilled = Pip < Bond;
						const bool bCapped = Pip >= BondCap;
						const FLinearColor Color = bFilled ? FAbyssUiStyle::Rgb(0xff6fa8) : (bCapped ? FAbyssUiStyle::Rgb(0x1a171d) : FAbyssUiStyle::Rgb(0x3a343f));
						P.Diamond(Center, 13.f, Color);
						P.Diamond(Center, 13.f * 0.55f, bFilled ? FAbyssUiStyle::Rgb(0xffd0e4) : FLinearColor::Transparent);
					}
				})
				.Size(FVector2D(Tables.maxBond * 20.0, 18.0))
			]
		];
	// passive
	Detail->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 10.f, 0.f, 0.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
			[
				AbyssUi::Label(*Ctx, Ctx->LocOr("ui.pet.passive", TEXT("Passive")), 12.f, C.Heading, true, 1)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				AbyssUi::Label(*Ctx, FText::AsCultureInvariant(PassiveText), 12.f, C.Good, true, 1)
			]
		];
	// abilities
	Detail->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 10.f, 0.f, 4.f))
		[
			AbyssUi::SectionHeader(Context, Ctx->LocOr("ui.pet.abilities", TEXT("Abilities")), 396.f)
		];
	TSharedRef<SVerticalBox> Abilities = SNew(SVerticalBox);
	for (const abyss::PetAbilityDef& Ability : Def->abilities)
	{
		const bool bUnlocked = Ability.unlock <= Pet->evolved;
		FString Header = Ctx->NameOr("data.pet.ability." + Ability.id + ".name", Ability.id);
		if (Ability.cooldownMs > 0.0)
		{
			Header += TEXT("  ") + Ctx->LocArgsOrStr("ui.pet.cooldown", TEXT("{sec}s cooldown"),
				{ FAbyssUiContext::Arg("sec", FAbyssUiContext::Num(FMath::RoundToDouble(Ability.cooldownMs / 100.0) / 10.0)) });
		}
		const FString Desc = AbyssPets_Optional(*Ctx, "data.pet.ability." + Ability.id + ".desc");
		Abilities->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 0.f, 6.f, 6.f))
			[
				SNew(SAbyssCanvas, Context, [bUnlocked](FAbyssPainter& P, const FVector2D& Size)
				{
					const FAbyssUiPalette& Palette = P.Style.Colors();
					P.Card(FVector2D::ZeroVector, Size, bUnlocked ? Palette.Card : FAbyssUiStyle::Rgb(0x121015),
						bUnlocked ? FAbyssUiStyle::Rgb(0x3a343f) : FAbyssUiStyle::Rgb(0x2e2a32), bUnlocked ? Palette.Info : FLinearColor::Transparent);
				})
				.Padding(FMargin(10.f, 6.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Header), 12.f, bUnlocked ? C.Parchment : C.Dim, true, 1, 370.f)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						AbyssUi::Label(*Ctx, bUnlocked ? FText::AsCultureInvariant(Desc) : Ctx->LocOr("ui.pet.locked", TEXT("Unlocks when Awakened")), 11.f,
							bUnlocked ? C.TextSoft : FAbyssUiStyle::Rgb(0xc07a6a), false, 1, 370.f)
					]
				]
			];
	}
	TSharedRef<SScrollBox> AbilityScroll = AbyssUi::ScrollBox(*Ctx);
	AbilityScroll->AddSlot()[Abilities];
	Detail->AddSlot()
		.FillHeight(1.f)
		[
			AbilityScroll
		];

	// buttons: summon / rest, feed (fruit count; disabled without fruit or when it cannot eat)
	const bool bCanFeed = Fruit > 0 && Pets.CanFeed(PetId);
	Detail->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
			[
				SNew(SAbyssButton, Context)
				.Text(bActive ? Ctx->LocOr("ui.pet.setRest", TEXT("Rest")) : Ctx->LocOr("ui.pet.setActive", TEXT("Summon")))
				.Kind(bActive ? EAbyssButtonKind::Secondary : EAbyssButtonKind::Primary)
				.FontPx(13.f)
				.Width(bTouch ? 150.f : 130.f)
				.Height(bTouch ? 46.f : 32.f)
				.IsEnabled(!bDying && (bActive || !bAway))
				.OnClicked_Lambda([Context, PetId, bActive]()
				{
					Context->Submit(abyss::CmdSetActivePet{ bActive ? std::string() : PetId });
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SAbyssButton, Context)
				.Text(Ctx->LocArgsOr("ui.pet.feed", TEXT("Feed ({count})"), { FAbyssUiContext::Arg("count", Fruit) }))
				.Kind(EAbyssButtonKind::Success)
				.FontPx(13.f)
				.Width(bTouch ? 150.f : 130.f)
				.Height(bTouch ? 46.f : 32.f)
				.IsEnabled(!bDying && bCanFeed)
				.OnClicked_Lambda([Context, PetId]() { Context->Submit(abyss::CmdFeedPet{ PetId }); })
			]
		];
	Body->AddSlot().Position(FVector2D(240.0, 74.0)).Size(FVector2D(404.0, 452.0))
	[
		Detail
	];
}
