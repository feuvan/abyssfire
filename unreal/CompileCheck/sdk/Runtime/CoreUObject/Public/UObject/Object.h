// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/Object.h (from UE 5.x source knowledge).
// UObject (non-reflected core API) and the global IsValid(). UClass is only forward-declared here: its members need
// UObject/Class.h (which every Engine header in the module's PCH pulls in, as in the engine).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/ObjectPtr.h"

class FOutputDevice;
class FReferenceCollector;
class UWorld;
class ITargetPlatform;
struct FPropertyChangedEvent;
struct FPropertyChangedChainEvent;
class FProperty;
class FObjectPreSaveContext;
struct FAssetData;
struct FFrame;
typedef uint32 ERenameFlags;
#define REN_None (0x0000)
#define REN_DontCreateRedirectors (0x0010)
#define REN_NonTransactional (0x0020)
#define REN_DoNotDirty (0x0080)

class UObject : public UObjectBaseUtility
{
	UE_STUB_DECLARE_CLASS_COMMON(UObject, UObject)
public:
	UObject();
	UObject(const FObjectInitializer& ObjectInitializer);
	UObject(EStaticConstructor, EObjectFlags InFlags);
	UObject(FVTableHelper& Helper);
	UObject(const UObject&) = delete;

	UObject* CreateDefaultSubobject(FName SubobjectFName, UClass* ReturnType, UClass* ClassToCreateByDefault, bool bIsRequired, bool bIsTransient);
	template <class TReturnType> requires UE::Stub::CUObjectClass<TReturnType>
	TReturnType* CreateDefaultSubobject(FName SubobjectName, bool bTransient = false) UE_STUB_BODY
	template <class TReturnType, class TClassToConstructByDefault> requires UE::Stub::CUObjectClass<TReturnType> && std::is_base_of_v<TReturnType, TClassToConstructByDefault>
	TReturnType* CreateDefaultSubobject(FName SubobjectName, bool bTransient = false) UE_STUB_BODY
	template <class TReturnType> requires UE::Stub::CUObjectClass<TReturnType>
	TReturnType* CreateEditorOnlyDefaultSubobject(FName SubobjectName, bool bTransient = false) UE_STUB_BODY
	template <class TReturnType> requires UE::Stub::CUObjectClass<TReturnType>
	TReturnType* CreateOptionalDefaultSubobject(FName SubobjectName, bool bTransient = false) UE_STUB_BODY
	void GetDefaultSubobjects(TArray<UObject*>& OutDefaultSubobjects);
	UObject* GetDefaultSubobjectByName(FName ToFind);

	virtual void PostInitProperties();
	virtual void PostReinitProperties();
	virtual void PostCDOContruct();
	virtual void PreSave(FObjectPreSaveContext SaveContext);
	virtual bool Modify(bool bAlwaysMarkDirty = true);
	virtual void PostLoad();
	virtual void PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph);
	virtual void BeginDestroy();
	virtual bool IsReadyForFinishDestroy();
	virtual void FinishDestroy();
	virtual void Serialize(FArchive& Ar);
	virtual void ShutdownAfterError();
	virtual void PostInterpChange(FProperty* PropertyThatChanged);
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent);
	virtual bool CanEditChange(const FProperty* InProperty) const;
	virtual void PostEditUndo();
	virtual void PostEditImport();
	void PostEditChange();
#endif
	virtual void PostRename(UObject* OldOuter, const FName OldName);
	virtual void PostDuplicate(bool bDuplicateForPIE);
	virtual bool NeedsLoadForClient() const;
	virtual bool NeedsLoadForServer() const;
	virtual bool NeedsLoadForEditorGame() const;
	virtual bool IsEditorOnly() const;
	virtual bool IsPostLoadThreadSafe() const;
	virtual bool IsDestructionThreadSafe() const;
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps);
	virtual bool Rename(const TCHAR* NewName = nullptr, UObject* NewOuter = nullptr, ERenameFlags Flags = REN_None);
	virtual FString GetDesc();
	virtual UWorld* GetWorld() const;
	virtual bool IsAsset() const;
	virtual bool IsNameStableForNetworking() const;
	virtual bool IsFullNameStableForNetworking() const;
	virtual bool IsSupportedForNetworking() const;
	virtual void GetSubobjectsWithStableNamesForNetworking(TArray<UObject*>& ObjList);
	virtual bool IsLocalizedResource() const;
	virtual bool IsSafeForRootSet() const;
	virtual void MarkAsEditorOnlySubobject();
	virtual bool CallRemoteFunction(UFunction* Function, void* Parms, struct FOutParmRec* OutParms, FFrame* Stack);
	virtual bool ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor);
	virtual void ProcessEvent(UFunction* Function, void* Parms);
	virtual UFunction* FindFunction(FName InName) const;
	UFunction* FindFunctionChecked(FName InName) const;

	bool ConditionalBeginDestroy();
	bool ConditionalFinishDestroy();
	void ConditionalPostLoad();
	bool IsBasedOnArchetype(const UObject* const SomeObject) const;
	UObject* GetArchetype() const;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	bool IsSelected() const;
	void SaveConfig(uint64 Flags = 0x00004000, const TCHAR* Filename = nullptr, FConfigCacheIni* Config = GConfig, bool bAllowCopyToDefaultObject = true);
	void LoadConfig(UClass* ConfigClass = nullptr, const TCHAR* Filename = nullptr, uint32 PropagationFlags = 0, FProperty* PropertyToLoad = nullptr);
	void ReloadConfig(UClass* ConfigClass = nullptr, const TCHAR* Filename = nullptr, uint32 PropagationFlags = 0, FProperty* PropertyToLoad = nullptr);
	bool CallFunctionByNameWithArguments(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor, bool bForceCallWithNonExec = false);
	template <class T> bool Implements() const;
	FString GetDetailedInfo() const;
	bool CheckDefaultSubobjects(bool bForceCheck = false) const;
};

bool IsValid(const UObject* Test);
bool IsValidChecked(const UObject* Test);
template <typename T> T* GetValid(T* Test) requires std::is_base_of_v<UObject, T> UE_STUB_BODY
