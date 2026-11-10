// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/TopLevelAssetPath.h (UE 5.1+; from UE 5.x
// source knowledge; reflected struct "TopLevelAssetPath": checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

struct FTopLevelAssetPath
{
	UE_STUB_STRUCT_BODY(FTopLevelAssetPath)
	FTopLevelAssetPath() {}
	FTopLevelAssetPath(TYPE_OF_NULLPTR) {}
	explicit FTopLevelAssetPath(FName InPackageName, FName InAssetName);
	explicit FTopLevelAssetPath(const TCHAR* InPackageName, const TCHAR* InAssetName);
	explicit FTopLevelAssetPath(const TCHAR* InPath);
	explicit FTopLevelAssetPath(const FString& InPath);
	explicit FTopLevelAssetPath(FStringView InPath);
	explicit FTopLevelAssetPath(const UObject* InObject);
	bool TrySetPath(FName InPackageName, FName InAssetName);
	bool TrySetPath(const FString& Path);
	FName GetPackageName() const;
	FName GetAssetName() const;
	FString ToString() const;
	void ToString(FString& OutString) const;
	bool IsValid() const;
	bool IsNull() const;
	void Reset();
	bool operator==(FTopLevelAssetPath const& Other) const;
	bool operator!=(FTopLevelAssetPath const& Other) const;
};
uint32 GetTypeHash(FTopLevelAssetPath const& This);
