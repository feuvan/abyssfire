// Stub SDK. Real header: Engine/Source/Runtime/DeveloperSettings/Public/Engine/DeveloperSettings.h (UE 5.x source
// knowledge; reflected class: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Class.h"

class UDeveloperSettings : public UObject
{
	UE_STUB_CLASS_BODY(UDeveloperSettings, UObject)
public:
	virtual FName GetContainerName() const;
	virtual FName GetCategoryName() const;
	virtual FName GetSectionName() const;
#if WITH_EDITOR
	virtual FText GetSectionText() const;
	virtual FText GetSectionDescription() const;
#endif
protected:
	FName CategoryName;
	FName SectionName;
};
