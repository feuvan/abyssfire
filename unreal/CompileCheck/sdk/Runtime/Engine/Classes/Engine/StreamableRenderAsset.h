// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/StreamableRenderAsset.h (UE 5.x source knowledge;
// reflected class: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

class UStreamableRenderAsset : public UObject
{
	UE_STUB_CLASS_BODY(UStreamableRenderAsset, UObject)
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LevelOfDetail, AdvancedDisplay)
	uint8 NeverStream : 1;
	UPROPERTY(Transient, BlueprintReadWrite, Category = LevelOfDetail)
	uint8 bGlobalForceMipLevelsToBeResident : 1;
	virtual void SetForceMipLevelsToBeResident(float Seconds, int32 CinematicLODGroupMask = 0);
	bool IsStreamable() const;
	bool IsFullyStreamedIn();
	void WaitForStreaming(bool bWaitForLODTransition = false, bool bSendCompletionEvents = false);
};
