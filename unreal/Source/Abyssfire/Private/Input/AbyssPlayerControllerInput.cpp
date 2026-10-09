// INPUT AGENT FILE. Input half of AAbyssPlayerController (see Framework/AbyssPlayerController.h "input agent region").
// Scaffold stubs written by the backbone so the module links; the input agent replaces their bodies:
//   * runtime Enhanced Input objects (ue58-platform.md 8.1, save-ui-input.md 5.6): IA_* actions + IMC_Gameplay_KBM,
//     IMC_Gameplay_Gamepad, IMC_Touch, IMC_UI, kept alive through AbyssInputConfig (UPROPERTY);
//   * keyboard / mouse / gamepad / touch -> abyss::Command via SubmitCommand (CmdPointerPress / CmdPointerHold /
//     CmdSetMoveInput / CmdCastSkill / CmdDodge / CmdCycleTarget / CmdToggleAutoCombat / CmdTownPortal / CmdInteract /
//     CmdOpenPanel ...), picking with DeprojectToTile + UAbyssActorRegistry projected bounds (no traces);
//   * IA_Back -> UAbyssGameInstance::GetUiRoot()->HandleBack() (U4).
#include "Framework/AbyssPlayerController.h"

#include "Components/InputComponent.h"

void AAbyssPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
}

void AAbyssPlayerController::InitAbyssInput()
{
}

void AAbyssPlayerController::ShutdownAbyssInput()
{
	AbyssInputConfig = nullptr;
}

void AAbyssPlayerController::TickAbyssInput(float DeltaTime)
{
}

void AAbyssPlayerController::OnAbyssInputContextChanged(EAbyssAppState NewState, bool bTouch)
{
}
