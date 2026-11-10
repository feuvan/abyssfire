// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Sound/SlateSound.h (UE 5.x source knowledge; reflected
// struct: checked against the 5.6 stub). A sound a widget style plays (the resource is a USoundBase, held as UObject).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"

struct FSlateSound
{
	UE_STUB_STRUCT_BODY(FSlateSound)
	FSlateSound();
	UObject* GetResourceObject() const;
	void SetResourceObject(UObject* InResourceObject);
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound, meta = (AllowedClasses = "/Script/Engine.SoundBase", DisplayName = "Sound"))
	TObjectPtr<UObject> ResourceObject;
};
