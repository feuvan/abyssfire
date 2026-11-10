// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/SoftObjectPath.h (from UE 5.x source knowledge;
// reflected struct "SoftObjectPath": checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/TopLevelAssetPath.h"

struct FSoftObjectPath
{
	UE_STUB_STRUCT_BODY(FSoftObjectPath)
	FSoftObjectPath() = default;
	FSoftObjectPath(const FSoftObjectPath& Other) = default;
	FSoftObjectPath(FSoftObjectPath&& Other) = default;
	// Since 5.1 only the FString constructor is implicit ("non-explicit for backwards compatibility"): a TEXT("...")
	// literal or nullptr is not passed where an FSoftObjectPath is expected. Assignment from them (operator=) exists.
	FSoftObjectPath(const FString& Path);
	explicit FSoftObjectPath(FStringView Path);
	explicit FSoftObjectPath(const TCHAR* Path);
	explicit FSoftObjectPath(TYPE_OF_NULLPTR) {}
	FSoftObjectPath(FTopLevelAssetPath InAssetPath, FString InSubPathString = FString());
	FSoftObjectPath(const UObject* InObject);
	template <typename T> FSoftObjectPath(const TObjectPtr<T>& InObject) UE_STUB_BODY
	FSoftObjectPath& operator=(const FSoftObjectPath& Path) = default;
	FSoftObjectPath& operator=(FSoftObjectPath&& Path) = default;
	FSoftObjectPath& operator=(const FString& Path);
	FSoftObjectPath& operator=(const TCHAR* Path);
	FString ToString() const;
	void ToString(FStringBuilderBase& Builder) const;
	FTopLevelAssetPath GetAssetPath() const;
	FSoftObjectPath GetWithoutSubPath() const;
	FString GetAssetPathString() const;
	const FString& GetSubPathString() const;
	FString GetLongPackageName() const;
	FName GetLongPackageFName() const;
	FString GetAssetName() const;
	FName GetAssetFName() const;
	void SetPath(const FTopLevelAssetPath& InAssetPath, FString InSubPathString = FString());
	void SetPath(FStringView Path);
	void SetPath(const TCHAR* Path);
	void SetPath(const FString& Path);
	UObject* TryLoad(struct FUObjectSerializeContext* InLoadContext = nullptr) const;
	UObject* ResolveObject() const;
	void Reset();
	bool IsValid() const;
	bool IsNull() const;
	bool IsAsset() const;
	bool IsSubobject() const;
	bool operator==(FSoftObjectPath const& Other) const;
	bool operator!=(FSoftObjectPath const& Other) const;
	bool LexicalLess(const FSoftObjectPath& Other) const;
	bool FastLess(const FSoftObjectPath& Other) const;
	static FSoftObjectPath GetOrCreateIDForObject(const UObject* Object);
};
uint32 GetTypeHash(FSoftObjectPath const& This);

struct FSoftClassPath : public FSoftObjectPath
{
	UE_STUB_STRUCT_BODY(FSoftClassPath)
	FSoftClassPath() = default;
	FSoftClassPath(const FString& PathString);
	FSoftClassPath(const UClass* InClass);
	/** The engine: `return LoadClass<T>(nullptr, *ToString(), nullptr, LOAD_None, nullptr);` */
	template <typename T> requires UE::Stub::CUObjectClass<T>
	UClass* TryLoadClass() const UE_STUB_BODY
	UClass* ResolveClass() const;
};
