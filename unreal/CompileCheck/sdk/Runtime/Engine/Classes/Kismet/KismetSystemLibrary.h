// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Kismet/KismetSystemLibrary.h (UE 5.x source knowledge;
// UFUNCTION-marked members: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/EngineTypes.h"

class APlayerController;

UENUM()
namespace EQuitPreference
{
	enum Type : int
	{
		Quit,
		Background,
	};
}

class UKismetSystemLibrary : public UBlueprintFunctionLibrary
{
	UE_STUB_CLASS_BODY(UKismetSystemLibrary, UBlueprintFunctionLibrary)
public:
	UFUNCTION(BlueprintCallable, Category = "Utilities", meta = (WorldContext = "WorldContextObject", CallableWithoutWorldContext))
	static void QuitGame(const UObject* WorldContextObject, class APlayerController* SpecificPlayer, TEnumAsByte<EQuitPreference::Type> QuitPreference, bool bIgnorePlatformRestrictions);
	UFUNCTION(BlueprintCallable, Category = "Development", meta = (WorldContext = "WorldContextObject"))
	static void ExecuteConsoleCommand(const UObject* WorldContextObject, const FString& Command, class APlayerController* SpecificPlayer = nullptr);
	UFUNCTION(BlueprintPure, Category = "Utilities")
	static bool IsValid(const UObject* Object);
	UFUNCTION(BlueprintPure, Category = "Utilities")
	static FString GetObjectName(const UObject* Object);
	UFUNCTION(BlueprintPure, Category = "Utilities")
	static FString GetDisplayName(const UObject* Object);
	UFUNCTION(BlueprintPure, Category = "Utilities|Time", meta = (WorldContext = "WorldContextObject"))
	static double GetGameTimeInSeconds(const UObject* WorldContextObject);
	UFUNCTION(BlueprintPure, Category = "Utilities")
	static FString GetProjectDirectory();
	UFUNCTION(BlueprintPure, Category = "Utilities")
	static FString GetPlatformUserName();
	UFUNCTION(BlueprintPure, Category = "Development")
	static bool IsPackagedForDistribution();
	UFUNCTION(BlueprintPure, Category = "Utilities|Platform")
	static FString GetEngineVersion();
	UFUNCTION(BlueprintCallable, Category = "Rendering|Debug", meta = (WorldContext = "WorldContextObject", DevelopmentOnly))
	static void DrawDebugLine(const UObject* WorldContextObject, const FVector LineStart, const FVector LineEnd, FLinearColor LineColor, float Duration = 0.f, float Thickness = 0.f);
	UFUNCTION(BlueprintCallable, Category = "Development", meta = (WorldContext = "WorldContextObject", CallableWithoutWorldContext, Keywords = "log print", AdvancedDisplay = "2", DevelopmentOnly))
	static void PrintString(const UObject* WorldContextObject, const FString& InString = FString(TEXT("Hello")), bool bPrintToScreen = true, bool bPrintToLog = true, FLinearColor TextColor = FLinearColor(0.0f, 0.66f, 1.0f), float Duration = 2.f, const FName Key = NAME_None);
};
