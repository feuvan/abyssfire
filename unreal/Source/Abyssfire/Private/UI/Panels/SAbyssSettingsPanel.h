// The one Settings panel (DECISIONS U9; save-ui-input 7.13 / 9): master / music / SFX volume, language, graphics quality
// (auto / low / balanced / high), control layout, touch control size and opacity, camera shake, damage numbers. Reached
// from the title screen, the system menu and the O key. Every change goes through UAbyssGameInstance::ApplyUserSettings
// (sanitised, applied, persisted in settings.json); sliders apply when the drag ends.
#pragma once

#include "CoreMinimal.h"

#include "Platform/AbyssSettings.h"
#include "UI/Panels/SAbyssPanelBase.h"

class SScrollBox;

class SAbyssSettingsPanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssSettingsPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::Settings; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(560.0, 600.0); }
	virtual FVector2D GetDesignPosition() const override { return FVector2D(-1.0, -1.0); }

private:
	FAbyssUserSettings Current() const;
	void Apply(const FAbyssUserSettings& NewSettings) const;

	TSharedRef<SWidget> MakeSliderRow(const FText& Label, TFunction<float(const FAbyssUserSettings&)> Get,
		TFunction<void(FAbyssUserSettings&, float)> Set, float Min, float Max);
	TSharedRef<SWidget> MakeChoiceRow(const FText& Label, const TArray<FText>& Options, TFunction<int32(const FAbyssUserSettings&)> Get,
		TFunction<void(FAbyssUserSettings&, int32)> Set);
	TSharedRef<SWidget> MakeToggleRow(const FText& Label, TFunction<bool(const FAbyssUserSettings&)> Get,
		TFunction<void(FAbyssUserSettings&, bool)> Set);

	TSharedPtr<SScrollBox> Scroll;
	bool bBuilt = false;
};
