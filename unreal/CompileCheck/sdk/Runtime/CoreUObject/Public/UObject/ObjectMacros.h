// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/ObjectMacros.h (from UE 5.x source knowledge).
//
// The reflection markup macros are the engine's: UPROPERTY / UFUNCTION / USTRUCT / UENUM / UMETA / UPARAM expand to
// nothing, UCLASS / UINTERFACE expand to <CURRENT_FILE_ID>_<line>_PROLOG and GENERATED_BODY() to
// <CURRENT_FILE_ID>_<line>_GENERATED_BODY - macros that only the header's .generated.h defines (CompileCheck/uht writes
// it). So a header that forgets its .generated.h include, includes it before another include that defines
// CURRENT_FILE_ID, or moves GENERATED_BODY() off the line UHT saw, fails here the way it fails with the real UHT.
//
// SDK engine classes cannot have generated headers; they use UE_STUB_CLASS_BODY / UE_STUB_STRUCT_BODY (below), which
// declare what GENERATED_BODY gives an engine class (Super, ThisClass, StaticClass, constructors). CompileCheck/tools/
// check_reflected.py treats exactly those classes / structs as "reflected" and checks them against the Python stub.
#pragma once

#include "CoreTypes.h"
#include "Misc/EnumClassFlags.h"

class UObject;
class UClass;
class UScriptStruct;
class FObjectInitializer;
class FVTableHelper;

enum EObjectFlags
{
	RF_NoFlags = 0x00000000,
	RF_Public = 0x00000001,
	RF_Standalone = 0x00000002,
	RF_MarkAsNative = 0x00000004,
	RF_Transactional = 0x00000008,
	RF_ClassDefaultObject = 0x00000010,
	RF_ArchetypeObject = 0x00000020,
	RF_Transient = 0x00000040,
	RF_MarkAsRootSet = 0x00000080,
	RF_TagGarbageTemp = 0x00000100,
	RF_NeedInitialization = 0x00000200,
	RF_NeedLoad = 0x00000400,
	RF_KeepForCooker = 0x00000800,
	RF_NeedPostLoad = 0x00001000,
	RF_NeedPostLoadSubobjects = 0x00002000,
	RF_NewerVersionExists = 0x00004000,
	RF_BeginDestroyed = 0x00008000,
	RF_FinishDestroyed = 0x00010000,
	RF_BeingRegenerated = 0x00020000,
	RF_DefaultSubObject = 0x00040000,
	RF_WasLoaded = 0x00080000,
	RF_TextExportTransient = 0x00100000,
	RF_LoadCompleted = 0x00200000,
	RF_InheritableComponentTemplate = 0x00400000,
	RF_DuplicateTransient = 0x00800000,
	RF_StrongRefOnFrame = 0x01000000,
	RF_NonPIEDuplicateTransient = 0x02000000,
	RF_WillBeLoaded = 0x08000000,
	RF_HasExternalPackage = 0x10000000,
	RF_AllocatedInSharedPage = 0x80000000,
};
ENUM_CLASS_FLAGS(EObjectFlags);
#define RF_AllFlags (EObjectFlags)0xffffffff

enum class EInternalObjectFlags : int32 { None = 0, Garbage = 1 << 21, RootSet = 1 << 30 };
ENUM_CLASS_FLAGS(EInternalObjectFlags);

enum ELoadFlags
{
	LOAD_None = 0x00000000,
	LOAD_Async = 0x00000001,
	LOAD_NoWarn = 0x00000002,
	LOAD_EditorOnly = 0x00000004,
	LOAD_ResolvingDeferredExports = 0x00000008,
	LOAD_Verify = 0x00000010,
	LOAD_NoVerify = 0x00000080,
	LOAD_Quiet = 0x00002000,
	LOAD_MemoryReader = 0x00020000,
};
enum EClassFlags
{
	CLASS_None = 0x00000000u,
	CLASS_Abstract = 0x00000001u,
	CLASS_DefaultConfig = 0x00000002u,
	CLASS_Config = 0x00000004u,
	CLASS_Transient = 0x00000008u,
	CLASS_Optional = 0x00000010u,
	CLASS_Native = 0x00000080u,
	CLASS_NotPlaceable = 0x00000200u,
	CLASS_Interface = 0x00004000u,
	CLASS_Const = 0x00010000u,
	CLASS_CompiledFromBlueprint = 0x00040000u,
	CLASS_MinimalAPI = 0x00080000u,
	CLASS_RequiredAPI = 0x00100000u,
	CLASS_Deprecated = 0x02000000u,
	CLASS_Hidden = 0x01000000u,
};
ENUM_CLASS_FLAGS(EClassFlags);
enum EInternal { EC_InternalUseOnlyConstructor };
enum EStaticConstructor { EC_StaticConstructor };

#define BODY_MACRO_COMBINE_INNER(A, B, C, D) A##B##C##D
#define BODY_MACRO_COMBINE(A, B, C, D) BODY_MACRO_COMBINE_INNER(A, B, C, D)
#define GENERATED_BODY_LEGACY(...) BODY_MACRO_COMBINE(CURRENT_FILE_ID, _, __LINE__, _GENERATED_BODY_LEGACY);
#define GENERATED_BODY(...) BODY_MACRO_COMBINE(CURRENT_FILE_ID, _, __LINE__, _GENERATED_BODY);
#define GENERATED_USTRUCT_BODY(...) GENERATED_BODY()
#define GENERATED_UCLASS_BODY(...) GENERATED_BODY_LEGACY()
#define GENERATED_UINTERFACE_BODY(...) GENERATED_BODY_LEGACY()
#define GENERATED_IINTERFACE_BODY(...) GENERATED_BODY_LEGACY()
#define UCLASS(...) BODY_MACRO_COMBINE(CURRENT_FILE_ID, _, __LINE__, _PROLOG)
#define UINTERFACE(...) UCLASS()
#define UPROPERTY(...)
#define UFUNCTION(...)
#define USTRUCT(...)
#define UMETA(...)
#define UPARAM(...)
#define UENUM(...)
#define UDELEGATE(...)
#define RIGVM_METHOD(...)

#define NO_API
#define UE_STUB_DECLARE_CLASS_COMMON(TClass, TSuperClass) \
	private: \
		TClass& operator=(TClass&&); \
		TClass& operator=(const TClass&); \
	public: \
		typedef TSuperClass Super; \
		typedef TClass ThisClass; \
		static UClass* StaticClass(); \
		static const TCHAR* StaticPackage(); \
		void* operator new(const size_t InSize, EInternal InInternalOnly, UObject* InOuter, class FName InName, EObjectFlags InSetFlags); \
		void* operator new(const size_t InSize, EInternal* InMem) { return (void*)InMem; } \
		void operator delete(void* InMem) { ::operator delete(InMem); }

/** Module classes: what <FileId>_<Line>_GENERATED_BODY declares (written by CompileCheck/uht per class). */
#define UE_STUB_DECLARE_CLASS(TClass, TSuperClass) \
	UE_STUB_DECLARE_CLASS_COMMON(TClass, TSuperClass) \
		TClass(TClass&&) = delete; \
		TClass(const TClass&) = delete; \
		TClass(FVTableHelper& Helper);

/** SDK engine classes (see the header comment). Both constructor forms derived classes may call are declared. */
#define UE_STUB_CLASS_BODY(TClass, TSuperClass) \
	UE_STUB_DECLARE_CLASS_COMMON(TClass, TSuperClass) \
		TClass(TClass&&) = delete; \
		TClass(const TClass&) = delete; \
		TClass(FVTableHelper& Helper); \
		TClass(const FObjectInitializer& ObjectInitializer); \
		TClass(); \
		virtual ~TClass(); \
	private:

#define UE_STUB_STRUCT_BODY(TStruct) \
	public: \
		static UScriptStruct* StaticStruct();

/** SDK engine interfaces: the I-class half (IFoo) of a UINTERFACE pair. */
#define UE_STUB_IINTERFACE_BODY(IClass, UClassName) \
	protected: \
		virtual ~IClass() {} \
	public: \
		typedef UClassName UClassType; \
		typedef IClass ThisClass; \
		virtual UObject* _getUObject() const { return nullptr; } \
	private:
