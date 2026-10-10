#include "UI/Core/AbyssUiContext.h"

#include "Brushes/SlateImageBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Framework/Application/SlateApplication.h"

#include <string>
#include <utility>
#include <vector>

#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/items/Inventory.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/Achievements.h"

#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssText.h"
#include "Input/AbyssInputMath.h"
#include "Input/AbyssInputSubsystem.h"
#include "UI/AbyssUiStyle.h"
#include "UI/AbyssUiSubsystem.h"

namespace
{
	/** "quest_elder" -> "QuestElder". */
	FString AbyssUiContext_Pascal(const FString& Id)
	{
		FString Out;
		bool bUpper = true;
		for (const TCHAR Character : Id)
		{
			if (Character == TEXT('_') || Character == TEXT('-') || Character == TEXT(' '))
			{
				bUpper = true;
				continue;
			}
			Out.AppendChar(bUpper ? FChar::ToUpper(Character) : Character);
			bUpper = false;
		}
		return Out;
	}

	std::string AbyssUiContext_Std(const char* Text)
	{
		return std::string(Text);
	}
}

FAbyssUiContext::FAbyssUiContext(UAbyssUiSubsystem& InOwner, const TSharedRef<FAbyssUiStyle>& InStyle)
	: Owner(&InOwner)
	, StylePtr(InStyle)
{
}

// =====================================================================================================================
// Core access
// =====================================================================================================================

UAbyssGameInstance* FAbyssUiContext::GetGameInstance() const
{
	const UAbyssUiSubsystem* Subsystem = Owner.Get();
	return Subsystem ? Subsystem->GetAbyssGameInstance() : nullptr;
}

const abyss::DataStore* FAbyssUiContext::GetData() const
{
	const UAbyssGameInstance* GameInstance = GetGameInstance();
	return GameInstance && GameInstance->IsDataReady() ? GameInstance->GetData() : nullptr;
}

const abyss::I18n* FAbyssUiContext::GetStrings() const
{
	const abyss::DataStore* Data = GetData();
	return Data ? &Data->Strings() : nullptr;
}

const abyss::Snapshot* FAbyssUiContext::GetSnapshot() const
{
	const UAbyssGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSnapshot() : nullptr;
}

bool FAbyssUiContext::IsTouch() const
{
	const UAbyssGameInstance* GameInstance = GetGameInstance();
	return GameInstance != nullptr && GameInstance->IsTouchMode();
}

float FAbyssUiContext::TouchScale() const
{
	if (!IsTouch() || GEngine == nullptr || GEngine->GameViewport == nullptr)
	{
		return 1.f;
	}
	FVector2D ViewportSize = FVector2D::ZeroVector;
	GEngine->GameViewport->GetViewportSize(ViewportSize);
	if (ViewportSize.X <= 0.0 || ViewportSize.Y <= 0.0)
	{
		return 1.f;
	}
	const float DpiScale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(
		FIntPoint(FMath::RoundToInt(ViewportSize.X), FMath::RoundToInt(ViewportSize.Y)));
	float MinK = 1.f;
	float MaxK = 2.f;
	if (const abyss::DataStore* Data = GetData())
	{
		MinK = static_cast<float>(Data->UiTheme().touchScaleMin);
		MaxK = static_cast<float>(Data->UiTheme().touchScaleMax);
	}
	return AbyssInputMath::ComputeTouchUnitScale(AbyssInputMath::EstimateDevicePixelRatio(ViewportSize), DpiScale > 0.f ? DpiScale : 1.f,
		MinK, MaxK);
}

bool FAbyssUiContext::IsHeroDying() const
{
	const abyss::Snapshot* Snap = GetSnapshot();
	return Snap != nullptr && (Snap->hero.life == abyss::HeroLife::Dying || Snap->hero.hp <= 0.0);
}

double FAbyssUiContext::Now() const
{
	return FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetCurrentTime() : FPlatformTime::Seconds();
}

// =====================================================================================================================
// Actions
// =====================================================================================================================

void FAbyssUiContext::Submit(const abyss::Command& Command) const
{
	if (UAbyssGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->Submit(Command);
	}
}

void FAbyssUiContext::PlaySound(abyss::SfxId Cue) const
{
	if (const UAbyssUiSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->PlayUiSound(Cue);
	}
}

void FAbyssUiContext::PressAction(EAbyssInputAction Action) const
{
	const UAbyssGameInstance* GameInstance = GetGameInstance();
	UAbyssInputSubsystem* Input = GameInstance ? GameInstance->GetSubsystem<UAbyssInputSubsystem>() : nullptr;
	if (Input != nullptr)
	{
		// HUD / touch presses never aim with the cursor (the controller treats injected presses accordingly).
		Input->PressAction(Action, IsTouch() ? EAbyssInputDevice::Touch : EAbyssInputDevice::KeyboardMouse);
	}
}

FString FAbyssUiContext::KeyHint(EAbyssInputAction Action) const
{
	const UAbyssGameInstance* GameInstance = GetGameInstance();
	const UAbyssInputSubsystem* Input = GameInstance ? GameInstance->GetSubsystem<UAbyssInputSubsystem>() : nullptr;
	return Input ? Input->GetKeyHint(Action) : FString();
}

// =====================================================================================================================
// Host
// =====================================================================================================================

void FAbyssUiContext::ShowTooltip(const TSharedRef<SWidget>& Content, const FVector2D& AbsoluteAnchor, const void* InOwner) const
{
	if (Host)
	{
		Host->ShowTooltip(Content, AbsoluteAnchor, InOwner);
	}
}

void FAbyssUiContext::HideTooltip(const void* InOwner) const
{
	if (Host)
	{
		Host->HideTooltip(InOwner);
	}
}

void FAbyssUiContext::ShowPopup(const TSharedRef<SWidget>& Content, const FVector2D& AbsoluteAnchor) const
{
	if (Host)
	{
		Host->ShowPopup(Content, AbsoluteAnchor);
	}
}

void FAbyssUiContext::ClosePopup() const
{
	if (Host)
	{
		Host->ClosePopup();
	}
}

void FAbyssUiContext::Confirm(FAbyssConfirmRequest Request) const
{
	if (Host)
	{
		Host->ShowConfirm(MoveTemp(Request));
	}
}

void FAbyssUiContext::Toast(const FText& Text, const FLinearColor& Color) const
{
	if (Host)
	{
		Host->ShowToast(Text, Color);
	}
}

// =====================================================================================================================
// Text
// =====================================================================================================================

bool FAbyssUiContext::HasKey(std::string_view Key) const
{
	const abyss::I18n* Strings = GetStrings();
	return Strings != nullptr && Strings->Has(Key);
}

FText FAbyssUiContext::Loc(std::string_view Key) const
{
	return FText::AsCultureInvariant(LocStr(Key));
}

FString FAbyssUiContext::LocStr(std::string_view Key) const
{
	const abyss::I18n* Strings = GetStrings();
	return Strings ? AbyssText::LocalizeString(*Strings, Key) : AbyssText::ToFString(Key);
}

FText FAbyssUiContext::Loc(const abyss::LocText& Text) const
{
	return FText::AsCultureInvariant(LocStr(Text));
}

FString FAbyssUiContext::LocStr(const abyss::LocText& Text) const
{
	const abyss::I18n* Strings = GetStrings();
	return Strings ? AbyssText::LocalizeString(*Strings, Text) : AbyssText::ToFString(Text.key);
}

FText FAbyssUiContext::LocOr(std::string_view Key, const TCHAR* Fallback) const
{
	return FText::AsCultureInvariant(LocOrStr(Key, Fallback));
}

FString FAbyssUiContext::LocOrStr(std::string_view Key, const TCHAR* Fallback) const
{
	return HasKey(Key) ? LocStr(Key) : FString(Fallback);
}

FText FAbyssUiContext::LocArgs(std::string_view Key, std::initializer_list<abyss::I18nArg> Args) const
{
	abyss::LocText Text;
	Text.key = std::string(Key);
	Text.args.assign(Args.begin(), Args.end());
	return Loc(Text);
}

FText FAbyssUiContext::LocArgsOr(std::string_view Key, const TCHAR* Fallback, std::initializer_list<abyss::I18nArg> Args) const
{
	return FText::AsCultureInvariant(LocArgsOrStr(Key, Fallback, Args));
}

FString FAbyssUiContext::LocArgsOrStr(std::string_view Key, const TCHAR* Fallback, std::initializer_list<abyss::I18nArg> Args) const
{
	if (HasKey(Key))
	{
		abyss::LocText Text;
		Text.key = std::string(Key);
		Text.args.assign(Args.begin(), Args.end());
		return LocStr(Text);
	}
	// Same literal {name} substitution as the core (key args resolved in the current locale first).
	std::vector<abyss::I18nArg> Resolved(Args.begin(), Args.end());
	const abyss::I18n* Strings = GetStrings();
	for (abyss::I18nArg& Value : Resolved)
	{
		if (Value.isKey && Strings != nullptr)
		{
			Value.value = Strings->T(Value.value);
			Value.isKey = false;
		}
	}
	return AbyssText::ToFString(abyss::I18n::Substitute(AbyssText::ToStd(FString(Fallback)), Resolved));
}

FString FAbyssUiContext::NameOr(std::string_view Key, const std::string& RawFallback) const
{
	if (HasKey(Key))
	{
		return LocStr(Key);
	}
	return RawFallback.empty() ? AbyssText::ToFString(Key) : AbyssText::ToFString(RawFallback);
}

abyss::I18nArg FAbyssUiContext::Arg(const char* Name, const FString& Value)
{
	return abyss::I18nArg{ AbyssUiContext_Std(Name), AbyssText::ToStd(Value), false };
}

abyss::I18nArg FAbyssUiContext::Arg(const char* Name, int64 Value)
{
	return abyss::I18nArg{ AbyssUiContext_Std(Name), std::to_string(Value), false };
}

abyss::I18nArg FAbyssUiContext::ArgKey(const char* Name, std::string Key)
{
	return abyss::KeyArg(AbyssUiContext_Std(Name), std::move(Key));
}

// =====================================================================================================================
// Names
// =====================================================================================================================

FString FAbyssUiContext::ItemName(const abyss::ItemInstance& Item) const
{
	const abyss::DataStore* Data = GetData();
	if (Data == nullptr)
	{
		return AbyssText::ToFString(Item.name);
	}
	return AbyssText::ToFString(abyss::ItemDisplayName(Item, *Data, Data->Strings()));
}

FString FAbyssUiContext::ItemBaseName(const std::string& BaseId) const
{
	const abyss::DataStore* Data = GetData();
	const abyss::ItemBaseDef* Base = Data ? Data->FindItemBase(BaseId) : nullptr;
	std::string Fallback = BaseId;
	if (Base != nullptr)
	{
		const bool bEnglish = Data->Strings().Current() == abyss::LocaleId::En;
		Fallback = bEnglish && !Base->nameEn.empty() ? Base->nameEn : Base->name;
	}
	return NameOr("data.item." + BaseId + ".name", Fallback);
}

FString FAbyssUiContext::StatLabel(abyss::Stat Stat) const
{
	const std::string Name(abyss::EnumName(Stat));
	if (HasKey("ui.stat." + Name))
	{
		return LocStr("ui.stat." + Name);
	}
	if (HasKey("data.stat." + Name))
	{
		return LocStr("data.stat." + Name);
	}
	if (const abyss::DataStore* Data = GetData())
	{
		for (const abyss::StatDisplayDef& Display : Data->Items().statDisplay)
		{
			if (Display.stat == Stat && !Display.label.empty())
			{
				return AbyssText::ToFString(Display.label);
			}
		}
	}
	return AbyssText::ToFString(Name);
}

bool FAbyssUiContext::IsPercentStat(abyss::Stat Stat) const
{
	if (const abyss::DataStore* Data = GetData())
	{
		for (const abyss::StatDisplayDef& Display : Data->Items().statDisplay)
		{
			if (Display.stat == Stat)
			{
				return Display.isPercent;
			}
		}
	}
	return false;
}

FString FAbyssUiContext::StatValue(abyss::Stat Stat, double Value, bool bSigned) const
{
	FString Text = Num(Value);
	if (bSigned && Value >= 0.0)
	{
		Text = TEXT("+") + Text;
	}
	if (IsPercentStat(Stat))
	{
		Text += TEXT("%");
	}
	return Text;
}

FString FAbyssUiContext::MonsterName(const std::string& DefId) const
{
	const abyss::DataStore* Data = GetData();
	const abyss::MonsterDef* Def = Data ? Data->FindMonster(DefId) : nullptr;
	return NameOr("data.monster." + DefId, Def ? Def->name : DefId);
}

FString FAbyssUiContext::NpcName(const std::string& NpcId) const
{
	const abyss::DataStore* Data = GetData();
	const abyss::NpcDef* Def = Data ? Data->FindNpc(NpcId) : nullptr;
	if (Def != nullptr && !Def->nameKey.empty())
	{
		return NameOr(Def->nameKey, Def->name);
	}
	return NameOr("data.npc." + NpcId + ".name", Def ? Def->name : NpcId);
}

FString FAbyssUiContext::QuestName(const abyss::QuestDef& Quest) const
{
	return NameOr(Quest.nameKey.empty() ? "data.quest." + Quest.id + ".name" : Quest.nameKey, Quest.name);
}

FString FAbyssUiContext::ZoneName(const std::string& MapId) const
{
	const abyss::DataStore* Data = GetData();
	const abyss::MapDef* Map = Data ? Data->FindMap(MapId) : nullptr;
	if (Map != nullptr && !Map->nameKey.empty())
	{
		return NameOr(Map->nameKey, Map->name);
	}
	return NameOr("data.zone." + MapId, Map ? Map->name : MapId);
}

FString FAbyssUiContext::ClassName(abyss::ClassId Class) const
{
	const std::string Id(abyss::EnumName(Class));
	const abyss::DataStore* Data = GetData();
	const abyss::ClassDef* Def = Data ? Data->Classes().Find(Class) : nullptr;
	return NameOr("data.class." + Id + ".name", Def ? Def->name : Id);
}

FString FAbyssUiContext::SkillName(const abyss::SkillDef& Skill) const
{
	const abyss::DataStore* Data = GetData();
	const bool bEnglish = Data != nullptr && Data->Strings().Current() == abyss::LocaleId::En;
	return NameOr("data.skill." + Skill.id + ".name", bEnglish && !Skill.nameEn.empty() ? Skill.nameEn : Skill.name);
}

FString FAbyssUiContext::SkillName(const std::string& SkillId) const
{
	const abyss::DataStore* Data = GetData();
	const abyss::SkillDef* Skill = Data ? Data->FindSkill(SkillId) : nullptr;
	return Skill ? SkillName(*Skill) : NameOr("data.skill." + SkillId + ".name", SkillId);
}

FString FAbyssUiContext::DamageTypeName(abyss::DamageType Type) const
{
	const std::string Id(abyss::EnumName(Type));
	return NameOr("data.damageType." + Id, Id);
}

FString FAbyssUiContext::QualityName(abyss::ItemQuality Quality) const
{
	const std::string Id(abyss::EnumName(Quality));
	return NameOr("ui.tooltip.quality." + Id, Id);
}

FString FAbyssUiContext::DifficultyName(abyss::Difficulty Difficulty) const
{
	const std::string Id(abyss::EnumName(Difficulty));
	if (HasKey("menu.difficulty." + Id))
	{
		return LocStr("menu.difficulty." + Id);
	}
	return NameOr("sys.difficulty.name." + Id, Id);
}

FString FAbyssUiContext::PetName(const std::string& PetId, int32 Evolved) const
{
	const abyss::DataStore* Data = GetData();
	if (Data == nullptr)
	{
		return AbyssText::ToFString(PetId);
	}
	return LocStr(abyss::PetDisplayName(*Data, PetId, Evolved));
}

FString FAbyssUiContext::ObjectiveTypeLabel(const abyss::QuestObjectiveDef& Objective, abyss::QuestType QuestType) const
{
	using abyss::ObjectiveType;
	switch (Objective.type)
	{
	case ObjectiveType::Kill:
	case ObjectiveType::Collect:
	case ObjectiveType::Explore:
	case ObjectiveType::Talk:
	case ObjectiveType::Escort:
	{
		const std::string Id(abyss::EnumName(Objective.type));
		return NameOr("sys.quest.type." + Id, Id);
	}
	default:
	{
		const std::string Id(abyss::EnumName(Objective.type));
		if (HasKey("sys.quest.objType." + Id))
		{
			return LocStr("sys.quest.objType." + Id);
		}
		const std::string QuestTypeId(abyss::EnumName(QuestType));
		return NameOr("sys.quest.type." + QuestTypeId, Id);
	}
	}
}

FString FAbyssUiContext::ObjectiveTargetLabel(const abyss::QuestDef& Quest, int32 ObjectiveIndex) const
{
	if (ObjectiveIndex < 0 || static_cast<size_t>(ObjectiveIndex) >= Quest.objectives.size())
	{
		return FString();
	}
	const abyss::QuestObjectiveDef& Objective = Quest.objectives[static_cast<size_t>(ObjectiveIndex)];
	if (!Objective.labelKey.empty())
	{
		return NameOr(Objective.labelKey, Objective.targetName);
	}
	return NameOr("data.questTarget." + Objective.targetId, Objective.targetName.empty() ? Objective.targetId : Objective.targetName);
}

// =====================================================================================================================
// Hero numbers
// =====================================================================================================================

abyss::EquipStats FAbyssUiContext::ApproxEquipStats(const abyss::Snapshot& Snap)
{
	abyss::EquipStats Out;
	if (Snap.inventory != nullptr)
	{
		Out.AddAll(Snap.inventory->GearStats());
	}
	if (Snap.achievements != nullptr)
	{
		Out.AddAll(Snap.achievements->Bonuses());
	}
	if (Snap.pets != nullptr)
	{
		Snap.pets->Bonuses().AddTo(Out);
	}
	return Out;
}

// =====================================================================================================================
// Numbers
// =====================================================================================================================

FString FAbyssUiContext::Num(double Value)
{
	return AbyssText::ToFString(abyss::FormatI18nNumber(Value));
}

FString FAbyssUiContext::Fixed(double Value, int32 Decimals)
{
	return AbyssText::ToFString(abyss::FormatFixed(Value, FMath::Clamp(Decimals, 0, 6)));
}

FString FAbyssUiContext::Int(int64 Value)
{
	return FString::Printf(TEXT("%lld"), static_cast<long long>(Value));
}

// =====================================================================================================================
// Icons
// =====================================================================================================================

const FSlateBrush* FAbyssUiContext::TextureBrush(FName AssetName) const
{
	if (AssetName.IsNone())
	{
		return nullptr;
	}
	if (const TUniquePtr<FSlateBrush>* Found = Brushes.Find(AssetName))
	{
		return Found->Get();
	}
	if (MissingBrushes.Contains(AssetName))
	{
		return nullptr;
	}
	UAbyssUiSubsystem* Subsystem = Owner.Get();
	UTexture2D* Texture = Subsystem ? Subsystem->FindUiTexture(AssetName) : nullptr;
	if (Texture == nullptr)
	{
		MissingBrushes.Add(AssetName);
		return nullptr;
	}
	TUniquePtr<FSlateBrush> Brush = MakeUnique<FSlateBrush>(
		FSlateImageBrush(Texture, FVector2D(FMath::Max(1, Texture->GetSizeX()), FMath::Max(1, Texture->GetSizeY()))));
	const FSlateBrush* Result = Brush.Get();
	Brushes.Add(AssetName, MoveTemp(Brush));
	return Result;
}

const FSlateBrush* FAbyssUiContext::RuntimeTextureBrush(UTexture2D* Texture, FName Key) const
{
	if (Texture == nullptr)
	{
		return nullptr;
	}
	if (TUniquePtr<FSlateBrush>* Found = Brushes.Find(Key))
	{
		if ((*Found)->GetResourceObject() == Texture)
		{
			return Found->Get();
		}
		(*Found)->SetResourceObject(Texture);
		return Found->Get();
	}
	TUniquePtr<FSlateBrush> Brush = MakeUnique<FSlateBrush>(
		FSlateImageBrush(Texture, FVector2D(FMath::Max(1, Texture->GetSizeX()), FMath::Max(1, Texture->GetSizeY()))));
	const FSlateBrush* Result = Brush.Get();
	Brushes.Add(Key, MoveTemp(Brush));
	return Result;
}

const FSlateBrush* FAbyssUiContext::ItemIcon(const std::string& BaseId) const
{
	const abyss::DataStore* Data = GetData();
	const abyss::ItemBaseDef* Base = Data ? Data->FindItemBase(BaseId) : nullptr;
	const FString Id = AbyssText::ToFString(BaseId);
	if (Base != nullptr && !Base->icon.empty())
	{
		const FString Icon = AbyssText::ToFString(Base->icon);
		// art-inventory 9: T_UI_ItemIcon_<iconId>__<baseId> for a base-specific variant, else the icon id.
		if (const FSlateBrush* Variant = TextureBrush(FName(*FString::Printf(TEXT("T_UI_ItemIcon_%s__%s"), *Icon, *Id))))
		{
			return Variant;
		}
		if (const FSlateBrush* Generic = TextureBrush(FName(*FString::Printf(TEXT("T_UI_ItemIcon_%s"), *Icon))))
		{
			return Generic;
		}
	}
	return TextureBrush(FName(*FString::Printf(TEXT("T_UI_ItemIcon_%s"), *Id)));
}

const FSlateBrush* FAbyssUiContext::SkillIcon(const std::string& SkillId) const
{
	return SkillId.empty() ? nullptr : TextureBrush(FName(*(TEXT("T_UI_SkillIcon_") + AbyssText::ToFString(SkillId))));
}

const FSlateBrush* FAbyssUiContext::QuestItemIcon(const std::string& Kind) const
{
	return Kind.empty() ? nullptr : TextureBrush(FName(*(TEXT("T_UI_QuestItem_") + AbyssText::ToFString(Kind))));
}

const FSlateBrush* FAbyssUiContext::Portrait(const std::string& ArtId) const
{
	const FString Id = AbyssText::ToFString(ArtId);
	if (Id.IsEmpty())
	{
		return TextureBrush(FName(TEXT("T_UI_Emblem_Generic")));
	}
	abyss::ClassId Class = abyss::ClassId::Warrior;
	if (abyss::ParseEnum(ArtId, Class))
	{
		return HeroPortrait(Class);
	}
	if (Id.StartsWith(TEXT("emblem_")))
	{
		if (const FSlateBrush* Emblem = TextureBrush(FName(*(TEXT("T_UI_Emblem_") + AbyssUiContext_Pascal(Id.Mid(7))))))
		{
			return Emblem;
		}
		return TextureBrush(FName(TEXT("T_UI_Emblem_Generic")));
	}
	FString Stripped = Id;
	for (const TCHAR* Prefix : { TEXT("player_"), TEXT("npc_"), TEXT("monster_"), TEXT("beast_") })
	{
		if (Stripped.StartsWith(Prefix))
		{
			Stripped = Stripped.Mid(FCString::Strlen(Prefix));
			break;
		}
	}
	if (const FSlateBrush* Found = TextureBrush(FName(*(TEXT("T_UI_Portrait_") + AbyssUiContext_Pascal(Stripped)))))
	{
		return Found;
	}
	if (Stripped != Id)
	{
		if (const FSlateBrush* Found = TextureBrush(FName(*(TEXT("T_UI_Portrait_") + AbyssUiContext_Pascal(Id)))))
		{
			return Found;
		}
	}
	return TextureBrush(FName(TEXT("T_UI_Emblem_Generic")));
}

const FSlateBrush* FAbyssUiContext::HeroPortrait(abyss::ClassId Class) const
{
	const FString Id = AbyssUiContext_Pascal(AbyssText::ToFString(abyss::EnumName(Class)));
	return TextureBrush(FName(*(TEXT("T_UI_Portrait_Hero_") + Id)));
}

const FSlateBrush* FAbyssUiContext::PetPortrait(const std::string& PetId) const
{
	FString Id = AbyssText::ToFString(PetId);
	const FString Full = AbyssUiContext_Pascal(Id);
	Id.RemoveFromStart(TEXT("pet_"));
	if (const FSlateBrush* Found = TextureBrush(FName(*(TEXT("T_UI_Portrait_Pet_") + AbyssUiContext_Pascal(Id)))))
	{
		return Found;
	}
	return TextureBrush(FName(*(TEXT("T_UI_Portrait_") + Full)));
}

const FSlateBrush* FAbyssUiContext::Glyph(const TCHAR* GlyphId) const
{
	return GlyphId ? TextureBrush(FName(*(FString(TEXT("T_UI_Glyph_")) + GlyphId))) : nullptr;
}
