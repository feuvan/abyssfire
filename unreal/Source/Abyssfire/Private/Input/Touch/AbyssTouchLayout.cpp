#include "Input/Touch/AbyssTouchLayout.h"

#include <string_view>

#include "abyss/data/UiData.h"

namespace
{
	// Ring angles (degrees, 0 = right, 90 = down) around the corner button (save-ui-input 5.7.2) + port additions.
	constexpr float AbyssTouch_SkillAngles[6] = { 178.0f, 210.0f, 242.0f, 274.0f, 220.0f, 250.0f };
	constexpr float AbyssTouch_DodgeAngle = 190.0f;
	constexpr float AbyssTouch_PotionHpAngle = 196.0f;
	constexpr float AbyssTouch_PotionMpAngle = 214.0f;
	constexpr float AbyssTouch_InteractAngle = 236.0f;
	// Web hit areas (MobileControlsSystem: round buttons r * 1.12, joystick grab zone r * 1.35).
	constexpr float AbyssTouch_RoundHitScale = 1.12f;
	constexpr float AbyssTouch_JoystickGrabScale = 1.35f;

	void AbyssTouch_ReadMetric(const abyss::UiThemeDef& Theme, std::string_view Name, float& InOutValue)
	{
		for (const std::pair<std::string, double>& Entry : Theme.touchCssPx)
		{
			if (Entry.first == Name && Entry.second > 0.0)
			{
				InOutValue = static_cast<float>(Entry.second);
				return;
			}
		}
	}

	FAbyssTouchPlacement AbyssTouch_Circle(EAbyssTouchControl Control, const FVector2D& Center, float Radius, float HitScale)
	{
		FAbyssTouchPlacement Placement;
		Placement.Control = Control;
		Placement.Shape = EAbyssTouchShape::Circle;
		Placement.Center = Center;
		Placement.DrawSize = FVector2D(Radius * 2.0f, Radius * 2.0f);
		Placement.HitScale = HitScale;
		return Placement;
	}

	FAbyssTouchPlacement AbyssTouch_Rect(EAbyssTouchControl Control, const FVector2D& TopLeft, const FVector2D& Size)
	{
		FAbyssTouchPlacement Placement;
		Placement.Control = Control;
		Placement.Shape = EAbyssTouchShape::Rect;
		Placement.Center = TopLeft + Size * 0.5;
		Placement.DrawSize = Size;
		Placement.HitScale = 1.0f;
		return Placement;
	}
}

FAbyssTouchMetrics FAbyssTouchMetrics::FromTheme(const abyss::UiThemeDef& Theme)
{
	FAbyssTouchMetrics Metrics;
	AbyssTouch_ReadMetric(Theme, "margin", Metrics.Margin);
	AbyssTouch_ReadMetric(Theme, "joystickR", Metrics.JoystickR);
	AbyssTouch_ReadMetric(Theme, "cornerR", Metrics.CornerR);
	AbyssTouch_ReadMetric(Theme, "skillR", Metrics.SkillR);
	AbyssTouch_ReadMetric(Theme, "dodgeR", Metrics.DodgeR);
	AbyssTouch_ReadMetric(Theme, "ring1", Metrics.Ring1);
	AbyssTouch_ReadMetric(Theme, "ring2", Metrics.Ring2);
	AbyssTouch_ReadMetric(Theme, "panelBtn", Metrics.PanelButton);
	AbyssTouch_ReadMetric(Theme, "panelGap", Metrics.PanelGap);
	AbyssTouch_ReadMetric(Theme, "toggleW", Metrics.ToggleW);
	AbyssTouch_ReadMetric(Theme, "toggleH", Metrics.ToggleH);
	AbyssTouch_ReadMetric(Theme, "toggleGap", Metrics.ToggleGap);
	if (Theme.touchScaleMin > 0.0 && Theme.touchScaleMax >= Theme.touchScaleMin)
	{
		Metrics.ScaleMin = static_cast<float>(Theme.touchScaleMin);
		Metrics.ScaleMax = static_cast<float>(Theme.touchScaleMax);
	}
	return Metrics;
}

namespace AbyssTouchLayout
{
	float Px(float CssPx, float UnitScale)
	{
		return FMath::RoundToFloat(CssPx * UnitScale);
	}

	bool RingAngle(EAbyssTouchControl Control, float& OutDegrees)
	{
		switch (Control)
		{
		case EAbyssTouchControl::Skill1: OutDegrees = AbyssTouch_SkillAngles[0]; return true;
		case EAbyssTouchControl::Skill2: OutDegrees = AbyssTouch_SkillAngles[1]; return true;
		case EAbyssTouchControl::Skill3: OutDegrees = AbyssTouch_SkillAngles[2]; return true;
		case EAbyssTouchControl::Skill4: OutDegrees = AbyssTouch_SkillAngles[3]; return true;
		case EAbyssTouchControl::Skill5: OutDegrees = AbyssTouch_SkillAngles[4]; return true;
		case EAbyssTouchControl::Skill6: OutDegrees = AbyssTouch_SkillAngles[5]; return true;
		case EAbyssTouchControl::Dodge: OutDegrees = AbyssTouch_DodgeAngle; return true;
		case EAbyssTouchControl::PotionHp: OutDegrees = AbyssTouch_PotionHpAngle; return true;
		case EAbyssTouchControl::PotionMp: OutDegrees = AbyssTouch_PotionMpAngle; return true;
		case EAbyssTouchControl::Interact: OutDegrees = AbyssTouch_InteractAngle; return true;
		default: return false;
		}
	}

	void Compute(const FAbyssTouchMetrics& Metrics, const FAbyssTouchLayoutInput& Input,
		TArray<FAbyssTouchPlacement>& OutPlacements)
	{
		OutPlacements.Reset();
		const float K = FMath::Max(Input.UnitScale, 0.1f);
		const double LayerW = Input.LayerSize.X;
		const double LayerH = Input.LayerSize.Y;
		const float M = Px(Metrics.Margin, K);

		// ---- joystick, bottom-left: centre (m + r + px(6), H - m - r) ----
		{
			const float Radius = Px(Metrics.JoystickR, K);
			const FVector2D Center(M + Radius + Px(6.0f, K), LayerH - M - Radius);
			OutPlacements.Add(AbyssTouch_Circle(EAbyssTouchControl::Joystick, Center, Radius, AbyssTouch_JoystickGrabScale));
		}

		// ---- corner LOCK and the rings around it: corner centre (W - px(48), H - px(48)) ----
		const FVector2D Corner(LayerW - Px(48.0f, K), LayerH - Px(48.0f, K));
		OutPlacements.Add(AbyssTouch_Circle(EAbyssTouchControl::Lock, Corner, Px(Metrics.CornerR, K), AbyssTouch_RoundHitScale));

		auto AddRing = [&](EAbyssTouchControl Control, float RingCss, float RadiusCss)
		{
			float Degrees = 0.0f;
			RingAngle(Control, Degrees);
			const float Ring = Px(RingCss, K);
			const double Radians = FMath::DegreesToRadians(static_cast<double>(Degrees));
			const FVector2D Center = Corner + FVector2D(FMath::Cos(Radians), FMath::Sin(Radians)) * Ring;
			OutPlacements.Add(AbyssTouch_Circle(Control, Center, Px(RadiusCss, K), AbyssTouch_RoundHitScale));
		};

		static constexpr EAbyssTouchControl SkillControls[6] = {
			EAbyssTouchControl::Skill1, EAbyssTouchControl::Skill2, EAbyssTouchControl::Skill3,
			EAbyssTouchControl::Skill4, EAbyssTouchControl::Skill5, EAbyssTouchControl::Skill6,
		};
		for (int32 Slot = 0; Slot < 6; ++Slot)
		{
			if ((Input.SkillSlotMask & (1u << Slot)) != 0)
			{
				AddRing(SkillControls[Slot], Slot < 4 ? Metrics.Ring1 : Metrics.Ring2, Metrics.SkillR);
			}
		}
		AddRing(EAbyssTouchControl::Dodge, Metrics.Ring2, Metrics.DodgeR);
		AddRing(EAbyssTouchControl::PotionHp, Metrics.Ring3, Metrics.PotionR);
		AddRing(EAbyssTouchControl::PotionMp, Metrics.Ring3, Metrics.PotionR);
		AddRing(EAbyssTouchControl::Interact, Metrics.Ring3, Metrics.InteractR);

		// ---- top-left toggles: auto-combat, auto-loot, log, town portal (y centre = m + px(22)) ----
		{
			const float Top = M;
			const float ToggleW = Px(Metrics.ToggleW, K);
			const float ToggleH = Px(Metrics.ToggleH, K);
			const float Gap = Px(Metrics.ToggleGap, K);
			const float Square = Px(Metrics.PanelButton, K);
			float Left = M;
			OutPlacements.Add(AbyssTouch_Rect(EAbyssTouchControl::AutoCombat, FVector2D(Left, Top), FVector2D(ToggleW, ToggleH)));
			Left += ToggleW + Gap;
			OutPlacements.Add(AbyssTouch_Rect(EAbyssTouchControl::AutoLoot, FVector2D(Left, Top), FVector2D(ToggleW, ToggleH)));
			Left += ToggleW + Gap;
			OutPlacements.Add(AbyssTouch_Rect(EAbyssTouchControl::Log, FVector2D(Left, Top), FVector2D(Square, Square)));
			Left += Square + Gap;
			OutPlacements.Add(AbyssTouch_Rect(EAbyssTouchControl::Portal, FVector2D(Left, Top), FVector2D(Square, Square)));
		}

		// ---- top-right panel row (right-aligned at the margin): bag, character, skills, map, [pets], quest, menu ----
		{
			TArray<EAbyssTouchControl, TInlineAllocator<8>> Row;
			Row.Add(EAbyssTouchControl::PanelInventory);
			Row.Add(EAbyssTouchControl::PanelCharacter);
			Row.Add(EAbyssTouchControl::PanelSkills);
			Row.Add(EAbyssTouchControl::PanelWorldMap);
			if (Input.bShowPets)
			{
				Row.Add(EAbyssTouchControl::PanelPets);
			}
			Row.Add(EAbyssTouchControl::PanelQuestLog);
			Row.Add(EAbyssTouchControl::Menu);
			const float Square = Px(Metrics.PanelButton, K);
			const float Gap = Px(Metrics.PanelGap, K);
			const float RowWidth = Row.Num() * Square + (Row.Num() - 1) * Gap;
			float Left = static_cast<float>(LayerW) - M - RowWidth;
			for (const EAbyssTouchControl Control : Row)
			{
				OutPlacements.Add(AbyssTouch_Rect(Control, FVector2D(Left, M), FVector2D(Square, Square)));
				Left += Square + Gap;
			}
		}
	}
}
