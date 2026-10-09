// Touch control layout (save-ui-input.md 5.7.2, world-map-nav.md 7.4 section 3, DECISIONS I4 / Q6 / U9, ue58-platform.md
// 9.5). Pure maths: positions in Slate units (the web's logical px) relative to the touch layer's top-left corner, for a
// layer of any size (the layer sits inside the HUD's SSafeZone).
//
// Units: every size is a "CSS px" constant c converted with px(c) = round(c * UnitScale), UnitScale = k * the user's
// touch-control scale (U9), k = clamp(DPR / DpiScale, 1, 2) (AbyssInputMath::ComputeTouchUnitScale). The web constants
// come from Data/ui_theme.json (touch.cssPx); the port additions below have no web counterpart.
//
// Web layout (k = 1 at 1280x720): joystick (76,650) r58; corner LOCK (1232,672) r36; skills 1-4 on ring 1 (r100) at
// 178/210/242/274 deg around the corner, r28; dodge on ring 2 (r162) at 190 deg, r25; skills 5-6 on ring 2 at 220/250
// deg; top-left toggles auto-combat 66x44, auto-loot 66x44, log 44x44 (+ the portal square of world 7.4, left edge at
// 206); top-right panel squares 44 (gap 4) right-aligned at the margin.
// Port additions: HP / MP potion buttons (I4) on ring 3 (r224) at 196 / 214 deg, r24; the context Talk / Use button (Q6)
// on ring 3 at 236 deg, r30; a system "menu" square at the right end of the panel row (Q30 / U9: settings and
// achievements are reached through the system menu; iOS has no back button). Homestead (U6) is never shown in
// milestone 1; the pets square only once a beast is owned.
#pragma once

#include "CoreMinimal.h"

namespace abyss
{
	struct UiThemeDef;
}

/** Every touch control. */
enum class EAbyssTouchControl : uint8
{
	Joystick,
	Lock,
	Skill1,
	Skill2,
	Skill3,
	Skill4,
	Skill5,
	Skill6,
	Dodge,
	PotionHp,
	PotionMp,
	Interact,
	AutoCombat,
	AutoLoot,
	Log,
	Portal,
	PanelInventory,
	PanelCharacter,
	PanelSkills,
	PanelWorldMap,
	PanelPets,
	PanelQuestLog,
	Menu,

	Count
};

inline constexpr int32 AbyssTouchControlCount = static_cast<int32>(EAbyssTouchControl::Count);

enum class EAbyssTouchShape : uint8
{
	Circle,
	Rect,
};

/** One placed control. */
struct FAbyssTouchPlacement
{
	EAbyssTouchControl Control = EAbyssTouchControl::Joystick;
	EAbyssTouchShape Shape = EAbyssTouchShape::Circle;
	/** Centre, Slate units from the layer's top-left. */
	FVector2D Center = FVector2D::ZeroVector;
	/** Drawn size (circle: diameter). */
	FVector2D DrawSize = FVector2D::ZeroVector;
	/** Hit area = DrawSize * HitScale (web: round buttons 1.12, joystick grab zone 1.35, squares 1). */
	float HitScale = 1.0f;

	FVector2D WidgetSize() const { return DrawSize * HitScale; }
};

/** The layout constants in CSS px. */
struct ABYSSFIRE_API FAbyssTouchMetrics
{
	// web (ui_theme.json touch.cssPx, MobileControlsSystem.ts CSS)
	float Margin = 12.0f;
	float JoystickR = 58.0f;
	float CornerR = 36.0f;
	float SkillR = 28.0f;
	float DodgeR = 25.0f;
	float Ring1 = 100.0f;
	float Ring2 = 162.0f;
	float PanelButton = 44.0f;
	float PanelGap = 4.0f;
	float ToggleW = 66.0f;
	float ToggleH = 44.0f;
	float ToggleGap = 6.0f;
	float ScaleMin = 1.0f;
	float ScaleMax = 2.0f;
	// port additions
	float PotionR = 24.0f;
	float Ring3 = 224.0f;
	float InteractR = 30.0f;

	/** The web values from the exported theme (missing entries keep the defaults above). */
	static FAbyssTouchMetrics FromTheme(const abyss::UiThemeDef& Theme);
};

struct FAbyssTouchLayoutInput
{
	/** Layer size in Slate units. */
	FVector2D LayerSize = FVector2D(1280.0, 720.0);
	/** k * user touch scale. */
	float UnitScale = 1.0f;
	/** Bit i set = hotbar slot i holds a skill (save-ui-input 5.7.3: buttons only for bound slots). */
	uint8 SkillSlotMask = 0;
	/** U6: the pets square once a beast is owned. */
	bool bShowPets = false;
};

namespace AbyssTouchLayout
{
	/** px(c) = round(c * UnitScale) (MobileControlsSystem px()). */
	ABYSSFIRE_API float Px(float CssPx, float UnitScale);

	/** All placements for the input (interact / potions are always placed; their visibility is dynamic). */
	ABYSSFIRE_API void Compute(const FAbyssTouchMetrics& Metrics, const FAbyssTouchLayoutInput& Input,
		TArray<FAbyssTouchPlacement>& OutPlacements);

	/** Angle (degrees, 0 = right, 90 = down) of a ring control around the corner button. */
	ABYSSFIRE_API bool RingAngle(EAbyssTouchControl Control, float& OutDegrees);
}
