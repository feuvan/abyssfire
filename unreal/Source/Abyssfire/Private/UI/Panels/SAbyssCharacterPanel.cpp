#include "UI/Panels/SAbyssCharacterPanel.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Text/STextBlock.h"

#include <string>

#include "abyss/combat/Damage.h"
#include "abyss/data/DataStore.h"
#include "abyss/items/Inventory.h"

#include "UI/AbyssUiStyle.h"
#include "UI/Widgets/AbyssUiWidgets.h"

namespace
{
	// 7.3 row order
	constexpr abyss::PrimaryStat GAbyssCharacterStats[6] = {
		abyss::PrimaryStat::Str, abyss::PrimaryStat::Dex, abyss::PrimaryStat::Int,
		abyss::PrimaryStat::Vit, abyss::PrimaryStat::Spi, abyss::PrimaryStat::Lck,
	};
}

void SAbyssCharacterPanel::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	SAbyssPanelBase::Construct(SAbyssPanelBase::FArguments(), InContext);
	const TSharedRef<FAbyssUiContext> Context = InContext;
	SetPanelContent(
		SNew(SAbyssPanelFrame, Context)
		.Size(GetDesignSize())
		.Title_Lambda([Context]()
		{
			const abyss::Snapshot* Snap = Context->GetSnapshot();
			const FString ClassName = Snap != nullptr ? Context->ClassName(Snap->hero.cls) : FString();
			return Context->LocArgsOr("ui.character.title", TEXT("Character - {className}"), { FAbyssUiContext::Arg("className", ClassName) });
		})
		.ContentPadding(FMargin(0.f))
		.OnClose_Lambda([this]() { RequestClose(); })
		[
			SAssignNew(Body, SCanvas)
		]);
}

void SAbyssCharacterPanel::Refresh(const abyss::Snapshot& Snap)
{
	Body->ClearChildren();
	const abyss::DataStore* Data = Ctx->GetData();
	if (Data == nullptr || Snap.inventory == nullptr)
	{
		return;
	}
	const TSharedRef<FAbyssUiContext> Context = Ctx.ToSharedRef();
	const FAbyssUiPalette& C = Ctx->Style().Colors();
	const abyss::HeroView& Hero = Snap.hero;
	const int32 Points = Hero.freeStatPoints;
	const bool bDying = Ctx->IsHeroDying();
	const abyss::EquipStats Gear = Snap.inventory->GearStats();
	const abyss::EquipStats Merged = FAbyssUiContext::ApproxEquipStats(Snap);

	// subtitle: Lv + free points (gold when points are available)
	Body->AddSlot().Position(FVector2D(0.0, 42.0)).Size(FVector2D(380.0, 20.0))
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		[
			AbyssUi::Label(*Ctx, Ctx->LocArgsOr("ui.character.subtitle", TEXT("Lv.{level}  Stat Points: {points}"),
				{ FAbyssUiContext::Arg("level", Hero.level), FAbyssUiContext::Arg("points", Points) }), 13.f,
				Points > 0 ? FAbyssUiStyle::Rgb(0xffd98a) : C.TextSoft, Points > 0, 1)
		]
	];

	// six stat rows: pitch 42 from y 70, card 344 x 37
	for (int32 Row = 0; Row < 6; ++Row)
	{
		const abyss::PrimaryStat Stat = GAbyssCharacterStats[Row];
		const std::string Id(abyss::EnumName(Stat));
		const int32 Base = Hero.baseStats.Get(Stat);
		const double Bonus = Gear.Get(abyss::ToStat(Stat));
		FString Value = FAbyssUiContext::Int(Base);
		if (Bonus > 0.0)
		{
			Value += FString::Printf(TEXT(" (+%s)"), *FAbyssUiContext::Num(Bonus));
		}
		const double Y = 70.0 + Row * 42.0;
		Body->AddSlot().Position(FVector2D(18.0, Y)).Size(FVector2D(344.0, 37.0))
		[
			SNew(SAbyssCanvas, Context, [](FAbyssPainter& P, const FVector2D& Size)
			{
				P.Card(FVector2D::ZeroVector, Size, P.Style.Colors().Card, FAbyssUiStyle::Rgb(0x3a343f), P.Style.Colors().Gold);
			})
		];
		Body->AddSlot().Position(FVector2D(30.0, Y + 3.0)).Size(FVector2D(220.0, 32.0))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Ctx->NameOr("ui.character.stat." + Id, Id)), 13.f, C.Text, true, 1)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Ctx->HasKey("ui.character.stat." + Id + ".desc")
					? Ctx->LocStr("ui.character.stat." + Id + ".desc") : FString()), 10.f, C.Muted, false, 1)
			]
		];
		Body->AddSlot().Position(FVector2D(236.0, Y + 8.0)).Size(FVector2D(88.0, 22.0))
		[
			SNew(SBox)
			.HAlign(HAlign_Right)
			[
				AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Value), 15.f, Bonus > 0.0 ? FAbyssUiStyle::Rgb(0x8be9fd) : C.Parchment, true, 1)
			]
		];
		if (Points > 0)
		{
			Body->AddSlot().Position(FVector2D(331.0, Y + 5.5)).Size(FVector2D(26.0, 26.0))
			[
				SNew(SAbyssButton, Context)
				.Text(FText::AsCultureInvariant(TEXT("+")))
				.Kind(EAbyssButtonKind::Success)
				.FontPx(15.f)
				.Width(26.f)
				.Height(26.f)
				.IsEnabled(!bDying)
				.OnClicked_Lambda([Context, Stat]() { Context->Submit(abyss::CmdAllocStat{ Stat, 1 }); })
			];
		}
	}

	// combat stats well (real formula numbers, Q19)
	Body->AddSlot().Position(FVector2D(18.0, 330.0)).Size(FVector2D(344.0, 18.0))
	[
		AbyssUi::SectionHeader(Context, Ctx->LocOr("ui.character.derivedHeader", TEXT("Combat Stats")), 344.f)
	];
	abyss::Combatant HeroCombatant;
	HeroCombatant.stats = Hero.baseStats;
	HeroCombatant.baseDamage = Hero.derived.baseDamage;
	HeroCombatant.defense = Hero.derived.defense;
	HeroCombatant.mana = Hero.mana;
	HeroCombatant.maxHp = Hero.maxHp;
	HeroCombatant.buffs = Snap.heroBuffs;
	HeroCombatant.eq = &Merged;
	HeroCombatant.outgoingMultiplier = Hero.resonating ? 1.0 + Data->Classes().spirit.For(Hero.cls).resonanceDamageBonus : 1.0;
	abyss::DamageRules Rules;
	Rules.formulas = &Data->Classes().formulas;
	Rules.caps = &Data->Classes().buffCaps;
	Rules.skillRules = &Data->Classes().skillRules;
	const abyss::CombatSummary Summary = abyss::SummarizeCombatant(Rules, HeroCombatant);

	FString Attack = FAbyssUiContext::Int(static_cast<int64>(FMath::FloorToDouble(Hero.derived.baseDamage)));
	if (Gear.Get(abyss::Stat::Damage) > 0.0)
	{
		Attack += FString::Printf(TEXT(" (+%s)"), *FAbyssUiContext::Num(Gear.Get(abyss::Stat::Damage)));
	}
	if (Gear.Get(abyss::Stat::DamagePercent) > 0.0)
	{
		Attack += FString::Printf(TEXT(" +%s%%"), *FAbyssUiContext::Num(Gear.Get(abyss::Stat::DamagePercent)));
	}
	FString Defense = FAbyssUiContext::Int(static_cast<int64>(FMath::FloorToDouble(Hero.derived.defense)));
	if (Gear.Get(abyss::Stat::Defense) > 0.0)
	{
		Defense += FString::Printf(TEXT(" (+%s)"), *FAbyssUiContext::Num(Gear.Get(abyss::Stat::Defense)));
	}

	struct FDerivedRow
	{
		FText Label;
		FString Value;
		FLinearColor Color;
	};
	const TArray<FDerivedRow> Rows = {
		{ FText::AsCultureInvariant(Ctx->StatLabel(abyss::Stat::MaxHp)),
			FString::Printf(TEXT("%lld/%lld"), static_cast<long long>(FMath::CeilToDouble(Hero.hp)), static_cast<long long>(FMath::RoundToDouble(Hero.maxHp))),
			FAbyssUiStyle::Rgb(0xff8a72) },
		{ FText::AsCultureInvariant(Ctx->StatLabel(abyss::Stat::MaxMana)),
			FString::Printf(TEXT("%lld/%lld"), static_cast<long long>(FMath::CeilToDouble(Hero.mana)), static_cast<long long>(FMath::RoundToDouble(Hero.maxMana))),
			FAbyssUiStyle::Rgb(0x7fb6ff) },
		{ Ctx->LocOr("ui.character.computed.attack", TEXT("Attack")), Attack, C.Text },
		{ Ctx->LocOr("ui.character.computed.hit", TEXT("Basic hit")),
			FString::Printf(TEXT("%d - %d"), Summary.damageMin, Summary.damageMax), C.Text },
		{ Ctx->LocOr("ui.character.computed.defense", TEXT("Defense")), Defense, C.Text },
		{ Ctx->LocOr("ui.character.computed.critRate", TEXT("Crit Rate")), FAbyssUiContext::Fixed(Summary.critChancePercent, 1) + TEXT("%"), C.Text },
		{ Ctx->LocOr("ui.character.computed.critDamage", TEXT("Crit Damage")),
			FAbyssUiContext::Int(FMath::RoundToInt(Summary.critMultiplier * 100.0)) + TEXT("%"), C.Text },
		{ Ctx->LocOr("ui.character.computed.dodge", TEXT("Dodge")), FAbyssUiContext::Fixed(Summary.dodgeChancePercent, 1) + TEXT("%"), C.Text },
		{ Ctx->LocOr("ui.character.computed.gold", TEXT("Gold")), FAbyssUiContext::Int(Hero.gold) + TEXT("G"), FAbyssUiStyle::Rgb(0xffd35a) },
	};
	const double WellTop = 354.0;
	const double RowH = 19.0;
	Body->AddSlot().Position(FVector2D(18.0, WellTop)).Size(FVector2D(344.0, Rows.Num() * RowH + 12.0))
	[
		SNew(SAbyssCanvas, Context, [](FAbyssPainter& P, const FVector2D& Size)
		{
			P.Well(FVector2D::ZeroVector, Size, 4.f);
		})
	];
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const double Y = WellTop + 6.0 + Index * RowH;
		Body->AddSlot().Position(FVector2D(30.0, Y)).Size(FVector2D(160.0, RowH))
		[
			AbyssUi::Label(*Ctx, Rows[Index].Label, 12.f, C.TextSoft, false, 1)
		];
		Body->AddSlot().Position(FVector2D(190.0, Y)).Size(FVector2D(160.0, RowH))
		[
			SNew(SBox)
			.HAlign(HAlign_Right)
			[
				AbyssUi::Label(*Ctx, FText::AsCultureInvariant(Rows[Index].Value), 12.f, Rows[Index].Color, true, 1)
			]
		];
	}
}
