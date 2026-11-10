// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/ConfigCacheIni.h (from UE 5.x source knowledge).
// GConfig (declared in CoreGlobals.h) points at this type; the header is NOT part of CoreMinimal.h.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "Math/Color.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"

class FConfigCacheIni
{
public:
	bool GetString(const TCHAR* Section, const TCHAR* Key, FString& Value, const FString& Filename);
	bool GetText(const TCHAR* Section, const TCHAR* Key, FText& Value, const FString& Filename);
	bool GetSection(const TCHAR* Section, TArray<FString>& Result, const FString& Filename);
	bool DoesSectionExist(const TCHAR* Section, const FString& Filename);
	void SetString(const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const FString& Filename);
	void SetText(const TCHAR* Section, const TCHAR* Key, const FText& Value, const FString& Filename);
	bool RemoveKey(const TCHAR* Section, const TCHAR* Key, const FString& Filename);
	bool EmptySection(const TCHAR* Section, const FString& Filename);
	void Flush(bool bRemoveFromCache, const FString& Filename = TEXT(""));
	FString GetStr(const TCHAR* Section, const TCHAR* Key, const FString& Filename);
	bool GetInt(const TCHAR* Section, const TCHAR* Key, int32& Value, const FString& Filename);
	bool GetInt64(const TCHAR* Section, const TCHAR* Key, int64& Value, const FString& Filename);
	bool GetFloat(const TCHAR* Section, const TCHAR* Key, float& Value, const FString& Filename);
	bool GetDouble(const TCHAR* Section, const TCHAR* Key, double& Value, const FString& Filename);
	bool GetBool(const TCHAR* Section, const TCHAR* Key, bool& Value, const FString& Filename);
	int32 GetArray(const TCHAR* Section, const TCHAR* Key, TArray<FString>& out_Arr, const FString& Filename);
	bool GetColor(const TCHAR* Section, const TCHAR* Key, FColor& Value, const FString& Filename);
	bool GetVector2D(const TCHAR* Section, const TCHAR* Key, FVector2D& Value, const FString& Filename);
	bool GetVector(const TCHAR* Section, const TCHAR* Key, FVector& Value, const FString& Filename);
	bool GetVector4(const TCHAR* Section, const TCHAR* Key, FVector4& Value, const FString& Filename);
	bool GetRotator(const TCHAR* Section, const TCHAR* Key, FRotator& Value, const FString& Filename);
	void SetInt(const TCHAR* Section, const TCHAR* Key, int32 Value, const FString& Filename);
	void SetFloat(const TCHAR* Section, const TCHAR* Key, float Value, const FString& Filename);
	void SetDouble(const TCHAR* Section, const TCHAR* Key, double Value, const FString& Filename);
	void SetBool(const TCHAR* Section, const TCHAR* Key, bool Value, const FString& Filename);
	void SetArray(const TCHAR* Section, const TCHAR* Key, const TArray<FString>& Value, const FString& Filename);
	void SetColor(const TCHAR* Section, const TCHAR* Key, FColor Value, const FString& Filename);
	void SetVector2D(const TCHAR* Section, const TCHAR* Key, FVector2D Value, const FString& Filename);
	void SetVector(const TCHAR* Section, const TCHAR* Key, FVector Value, const FString& Filename);
	void SetRotator(const TCHAR* Section, const TCHAR* Key, FRotator Value, const FString& Filename);
};
