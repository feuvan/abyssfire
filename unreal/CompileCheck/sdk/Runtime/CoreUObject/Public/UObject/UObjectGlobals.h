// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h (from UE 5.x source knowledge).
// Object construction / lookup / loading globals, FObjectInitializer, FCoreUObjectDelegates.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

class UObject;
class UClass;
class UPackage;
class UPackageMap;
class UWorld;
struct FObjectInstancingGraph;
class FLinkerInstancingContext;
struct FTopLevelAssetPath;
class FReferenceCollector;
template <typename T> struct TObjectPtr;

DECLARE_LOG_CATEGORY_EXTERN(LogUObjectGlobals, Log, All);

enum class EFindFirstObjectOptions { None = 0, ExactClass = 1 << 1, NativeFirst = 1 << 2, EnsureIfAmbiguous = 1 << 3 };
ENUM_CLASS_FLAGS(EFindFirstObjectOptions);
namespace EDuplicateMode { enum Type { Normal, World, PIE }; }

UPackage* GetTransientPackage();
UObject* StaticFindObject(UClass* Class, UObject* InOuter, const TCHAR* Name, bool ExactClass = false);
UObject* StaticFindObjectFast(UClass* Class, UObject* InOuter, FName InName, bool ExactClass = false, EObjectFlags ExclusiveFlags = RF_NoFlags, EInternalObjectFlags ExclusiveInternalFlags = EInternalObjectFlags::None);
UObject* StaticFindFirstObject(UClass* Class, const TCHAR* Name, EFindFirstObjectOptions Options = EFindFirstObjectOptions::None, ELogVerbosity::Type AmbiguousMessageVerbosity = ELogVerbosity::NoLogging, const TCHAR* InCurrentOperation = nullptr);
UObject* StaticLoadObject(UClass* Class, UObject* InOuter, const TCHAR* Name, const TCHAR* Filename = nullptr, uint32 LoadFlags = LOAD_None, UPackageMap* Sandbox = nullptr, bool bAllowObjectReconciliation = true, const FLinkerInstancingContext* InstancingContext = nullptr);
UClass* StaticLoadClass(UClass* BaseClass, UObject* InOuter, const TCHAR* Name, const TCHAR* Filename = nullptr, uint32 LoadFlags = LOAD_None, UPackageMap* Sandbox = nullptr);
UObject* StaticDuplicateObject(UObject const* SourceObject, UObject* DestOuter, const FName DestName = NAME_None, EObjectFlags FlagMask = RF_AllFlags, UClass* DestClass = nullptr, EDuplicateMode::Type DuplicateMode = EDuplicateMode::Normal, EInternalObjectFlags InternalFlagsMask = EInternalObjectFlags::None);
UPackage* LoadPackage(UPackage* InOuter, const TCHAR* InLongPackageName, uint32 LoadFlags, FArchive* InReaderOverride = nullptr, const FLinkerInstancingContext* InstancingContext = nullptr);
UPackage* CreatePackage(const TCHAR* PackageName);
UPackage* FindPackage(UObject* InOuter, const TCHAR* PackageName);
void CollectGarbage(EObjectFlags KeepFlags, bool bPerformFullPurge = true);
bool TryCollectGarbage(EObjectFlags KeepFlags, bool bPerformFullPurge = true);
bool IsGarbageCollecting();
bool IsLoading();
void FlushAsyncLoading(int32 PackageID = INDEX_NONE);
bool IsAsyncLoading();
FName MakeUniqueObjectName(UObject* Outer, const UClass* Class, FName BaseName = NAME_None);
FString GetPathNameSafe(const UObject* Object);
FString GetNameSafe(const UObject* Object);
FString GetFullNameSafe(const UObject* Object);
bool UObjectInitialized();

class FVTableHelper
{
public:
	FVTableHelper();
};

namespace UE::Stub
{
	/** NewObject / FindObject / LoadObject need the complete class (they call T::StaticClass()). */
	template <typename T> concept CUObjectClass = requires { sizeof(T); T::StaticClass(); } && std::is_base_of_v<UObject, T>;
}

class FObjectInitializer
{
public:
	FObjectInitializer();
	~FObjectInitializer();
	static FObjectInitializer& Get();
	UObject* GetArchetype() const;
	UObject* GetObj() const;
	UClass* GetClass() const;
	// The engine's bodies call TReturnType::StaticClass() (and TClassToConstructByDefault::StaticClass()) and
	// static_cast the created UObject* to TReturnType*: complete UObject classes.
	template <class TReturnType> requires UE::Stub::CUObjectClass<TReturnType>
	TReturnType* CreateDefaultSubobject(UObject* Outer, FName SubobjectName, bool bTransient = false) const UE_STUB_BODY
	template <class TReturnType> requires UE::Stub::CUObjectClass<TReturnType>
	TReturnType* CreateOptionalDefaultSubobject(UObject* Outer, FName SubobjectName, bool bTransient = false) const UE_STUB_BODY
	template <class TReturnType, class TClassToConstructByDefault> requires UE::Stub::CUObjectClass<TReturnType> && UE::Stub::CUObjectClass<TClassToConstructByDefault>
	TReturnType* CreateDefaultSubobject(UObject* Outer, FName SubobjectName, bool bTransient = false) const UE_STUB_BODY
	template <class T> requires UE::Stub::CUObjectClass<T>
	const FObjectInitializer& SetDefaultSubobjectClass(FName SubobjectName) const UE_STUB_BODY
	template <class T> requires UE::Stub::CUObjectClass<T>
	const FObjectInitializer& SetDefaultSubobjectClass(const TCHAR* SubobjectName) const UE_STUB_BODY
	const FObjectInitializer& DoNotCreateDefaultSubobject(FName SubobjectName) const;
	const FObjectInitializer& DoNotCreateDefaultSubobject(const TCHAR* SubobjectName) const;
};

template <class T> requires UE::Stub::CUObjectClass<T>
T* NewObject(UObject* Outer, const UClass* Class, FName Name = NAME_None, EObjectFlags Flags = RF_NoFlags, UObject* Template = nullptr,
	bool bCopyTransientsFromClassDefaults = false, FObjectInstancingGraph* InInstanceGraph = nullptr, UPackage* ExternalPackage = nullptr) UE_STUB_BODY
template <class T> requires UE::Stub::CUObjectClass<T>
T* NewObject(UObject* Outer = (UObject*)GetTransientPackage()) UE_STUB_BODY
template <class T> requires UE::Stub::CUObjectClass<T>
T* NewObject(UObject* Outer, FName Name, EObjectFlags Flags = RF_NoFlags, UObject* Template = nullptr,
	bool bCopyTransientsFromClassDefaults = false, FObjectInstancingGraph* InInstanceGraph = nullptr) UE_STUB_BODY
template <class T> requires UE::Stub::CUObjectClass<T>
T* FindObject(UObject* Outer, const TCHAR* Name, bool ExactClass = false) UE_STUB_BODY
template <class T> requires UE::Stub::CUObjectClass<T>
T* FindObjectFast(UObject* Outer, FName Name, bool ExactClass = false, EObjectFlags ExclusiveFlags = RF_NoFlags) UE_STUB_BODY
template <class T> requires UE::Stub::CUObjectClass<T>
T* FindObjectChecked(UObject* Outer, const TCHAR* Name, bool ExactClass = false) UE_STUB_BODY
template <class T> requires UE::Stub::CUObjectClass<T>
T* FindFirstObject(const TCHAR* Name, EFindFirstObjectOptions Options = EFindFirstObjectOptions::None, ELogVerbosity::Type AmbiguousMessageVerbosity = ELogVerbosity::NoLogging, const TCHAR* CurrentOperation = nullptr) UE_STUB_BODY
template <class T> requires UE::Stub::CUObjectClass<T>
T* LoadObject(UObject* Outer, const TCHAR* Name, const TCHAR* Filename = nullptr, uint32 LoadFlags = LOAD_None, UPackageMap* Sandbox = nullptr, const FLinkerInstancingContext* InstancingContext = nullptr) UE_STUB_BODY
template <class T> requires UE::Stub::CUObjectClass<T>
UClass* LoadClass(UObject* Outer, const TCHAR* Name, const TCHAR* Filename = nullptr, uint32 LoadFlags = LOAD_None, UPackageMap* Sandbox = nullptr) UE_STUB_BODY
template <class T> requires UE::Stub::CUObjectClass<T>
const T* GetDefault() UE_STUB_BODY
template <class T> requires UE::Stub::CUObjectClass<T>
const T* GetDefault(UClass* Class) UE_STUB_BODY
template <class T> requires UE::Stub::CUObjectClass<T>
T* GetMutableDefault() UE_STUB_BODY
template <class T> requires UE::Stub::CUObjectClass<T>
T* GetMutableDefault(UClass* Class) UE_STUB_BODY
template <class T> requires UE::Stub::CUObjectClass<T>
T* DuplicateObject(const T* SourceObject, UObject* Outer, const FName Name = NAME_None) UE_STUB_BODY
template <class T> requires UE::Stub::CUObjectClass<T>
T* DuplicateObject(const TObjectPtr<T>& SourceObject, UObject* Outer, const FName Name = NAME_None) UE_STUB_BODY

struct FCoreUObjectDelegates
{
	DECLARE_MULTICAST_DELEGATE_OneParam(FPreLoadMapDelegate, const FString&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FPostLoadMapDelegate, UWorld*);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnObjectModified, UObject*);
	static FPreLoadMapDelegate PreLoadMap;
	static FPostLoadMapDelegate PostLoadMapWithWorld;
	static FSimpleMulticastDelegate PostDemoPlay;
	static FOnObjectModified OnObjectModified;
	static FSimpleMulticastDelegate& GetPreGarbageCollectDelegate();
	static FSimpleMulticastDelegate& GetPostGarbageCollect();
	static FSimpleMulticastDelegate PreGarbageCollectConditionalBeginDestroy;
	static FSimpleMulticastDelegate PostGarbageCollectConditionalBeginDestroy;
};
