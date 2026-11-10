// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/HAL/IConsoleManager.h (from UE 5.x source knowledge).
// Console variables and commands. Not part of CoreMinimal.h. TAutoConsoleVariable<T> exists for bool, int32, float and
// FString only; GetValueOnGameThread() / GetValueOnAnyThread() return T; IConsoleVariable::Set takes a value and the
// EConsoleVariableFlags "set by" priority.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Logging/LogMacros.h"
#include "Templates/SharedPointer.h"
#include "Templates/Function.h"

class UWorld;
class FOutputDevice;

enum EConsoleVariableFlags
{
	ECVF_FlagMask = 0x0000ffff,
	ECVF_Default = 0x0,
	ECVF_Cheat = 0x1,
	ECVF_ReadOnly = 0x4,
	ECVF_Unregistered = 0x8,
	ECVF_CreatedFromIni = 0x10,
	ECVF_RenderThreadSafe = 0x20,
	ECVF_Scalability = 0x40,
	ECVF_ScalabilityGroup = 0x80,
	ECVF_Preview = 0x100,
	ECVF_GeneralShaderChange = 0x200,
	ECVF_MobileShaderChange = 0x400,
	ECVF_DesktopShaderChange = 0x800,
	ECVF_ExcludeFromPreview = 0x1000,
	ECVF_SaveForNextBoot = 0x2000,
	ECVF_SetFlagMask = 0x00ff0000,
	ECVF_Set_NoSinkCall_Unsafe = 0x00010000,
	ECVF_SetByMask = 0xff000000,
	ECVF_SetByConstructor = 0x00000000,
	ECVF_SetByScalability = 0x01000000,
	ECVF_SetByGameSetting = 0x02000000,
	ECVF_SetByProjectSetting = 0x03000000,
	ECVF_SetBySystemSettingsIni = 0x04000000,
	ECVF_SetByDeviceProfile = 0x05000000,
	ECVF_SetByGameOverride = 0x06000000,
	ECVF_SetByConsoleVariablesIni = 0x07000000,
	ECVF_SetByCommandline = 0x08000000,
	ECVF_SetByCode = 0x09000000,
	ECVF_SetByConsole = 0x0A000000,
};

class IConsoleVariable;
DECLARE_DELEGATE(FConsoleCommandDelegate);
DECLARE_DELEGATE_OneParam(FConsoleCommandWithArgsDelegate, const TArray<FString>&);
DECLARE_DELEGATE_OneParam(FConsoleCommandWithWorldDelegate, UWorld*);
DECLARE_DELEGATE_TwoParams(FConsoleCommandWithWorldAndArgsDelegate, const TArray<FString>&, UWorld*);
DECLARE_DELEGATE_TwoParams(FConsoleCommandWithArgsAndOutputDeviceDelegate, const TArray<FString>&, FOutputDevice&);
DECLARE_DELEGATE_ThreeParams(FConsoleCommandWithWorldArgsAndOutputDeviceDelegate, const TArray<FString>&, UWorld*, FOutputDevice&);
DECLARE_DELEGATE_OneParam(FConsoleCommandWithOutputDeviceDelegate, FOutputDevice&);
DECLARE_DELEGATE_OneParam(FConsoleVariableDelegate, IConsoleVariable*);
DECLARE_MULTICAST_DELEGATE_OneParam(FConsoleVariableMulticastDelegate, IConsoleVariable*);

template <class T> class TConsoleVariableData;

class IConsoleObject
{
public:
	IConsoleObject() {}
	virtual ~IConsoleObject() {}
	virtual const TCHAR* GetHelp() const = 0;
	virtual void SetHelp(const TCHAR* Value) = 0;
	virtual EConsoleVariableFlags GetFlags() const = 0;
	virtual void SetFlags(const EConsoleVariableFlags Value) = 0;
	void ClearFlags(const EConsoleVariableFlags Value);
	bool TestFlags(const EConsoleVariableFlags Value) const;
	virtual class IConsoleVariable* AsVariable() { return nullptr; }
	virtual bool IsVariableBool() const { return false; }
	virtual bool IsVariableInt() const { return false; }
	virtual bool IsVariableFloat() const { return false; }
	virtual bool IsVariableString() const { return false; }
	virtual struct IConsoleCommand* AsCommand() { return nullptr; }
};

class IConsoleVariable : public IConsoleObject
{
public:
	virtual void Set(const TCHAR* InValue, EConsoleVariableFlags SetBy = ECVF_SetByCode) = 0;
	virtual bool GetBool() const = 0;
	virtual int32 GetInt() const = 0;
	virtual float GetFloat() const = 0;
	virtual FString GetString() const = 0;
	void GetValue(int32& OutIntValue);
	void GetValue(bool& OutBoolValue);
	void GetValue(float& OutFloatValue);
	void GetValue(FString& OutStringValue);
	virtual void SetOnChangedCallback(const FConsoleVariableDelegate& Callback) = 0;
	virtual FConsoleVariableMulticastDelegate& OnChangedDelegate() = 0;
	void Set(bool InValue, EConsoleVariableFlags SetBy = ECVF_SetByCode);
	void Set(int32 InValue, EConsoleVariableFlags SetBy = ECVF_SetByCode);
	void Set(float InValue, EConsoleVariableFlags SetBy = ECVF_SetByCode);
	void SetWithCurrentPriority(bool InValue);
	void SetWithCurrentPriority(int32 InValue);
	void SetWithCurrentPriority(float InValue);
	void SetWithCurrentPriority(const TCHAR* InValue);
	virtual FString GetDefaultValue() = 0;
};

struct IConsoleCommand : public IConsoleObject
{
	virtual bool Execute(const TArray<FString>& Args, UWorld* InWorld, class FOutputDevice& OutputDevice) = 0;
};

template <class T>
class TConsoleVariableData
{
public:
	TConsoleVariableData(const T DefaultValue) UE_STUB_BODY
	T GetValueOnGameThread() const UE_STUB_BODY
	T GetValueOnRenderThread() const UE_STUB_BODY
	T GetValueOnAnyThread(bool bForceGameThread = false) const UE_STUB_BODY
};

struct IConsoleManager
{
	virtual IConsoleVariable* RegisterConsoleVariable(const TCHAR* Name, bool DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	virtual IConsoleVariable* RegisterConsoleVariable(const TCHAR* Name, int32 DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	virtual IConsoleVariable* RegisterConsoleVariable(const TCHAR* Name, float DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	virtual IConsoleVariable* RegisterConsoleVariable(const TCHAR* Name, const TCHAR* DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	virtual IConsoleVariable* RegisterConsoleVariable(const TCHAR* Name, const FString& DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	virtual IConsoleVariable* RegisterConsoleVariableRef(const TCHAR* Name, bool& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	virtual IConsoleVariable* RegisterConsoleVariableRef(const TCHAR* Name, int32& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	virtual IConsoleVariable* RegisterConsoleVariableRef(const TCHAR* Name, float& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	virtual IConsoleVariable* RegisterConsoleVariableRef(const TCHAR* Name, FString& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default) = 0;
	virtual void CallAllConsoleVariableSinks() = 0;
	virtual IConsoleCommand* RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandDelegate& Command, uint32 Flags = ECVF_Default) = 0;
	virtual IConsoleCommand* RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithArgsDelegate& Command, uint32 Flags = ECVF_Default) = 0;
	virtual IConsoleCommand* RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldDelegate& Command, uint32 Flags = ECVF_Default) = 0;
	virtual IConsoleCommand* RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldAndArgsDelegate& Command, uint32 Flags = ECVF_Default) = 0;
	virtual IConsoleCommand* RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithOutputDeviceDelegate& Command, uint32 Flags = ECVF_Default) = 0;
	virtual IConsoleObject* FindConsoleObject(const TCHAR* Name, bool bTrackFrequentCalls = true) const = 0;
	virtual IConsoleVariable* FindConsoleVariable(const TCHAR* Name, bool bTrackFrequentCalls = true) const = 0;
	TConsoleVariableData<int32>* FindTConsoleVariableDataInt(const TCHAR* Name) const;
	TConsoleVariableData<float>* FindTConsoleVariableDataFloat(const TCHAR* Name) const;
	virtual bool ProcessUserConsoleInput(const TCHAR* Input, FOutputDevice& Ar, UWorld* InWorld) = 0;
	virtual void UnregisterConsoleObject(IConsoleObject* ConsoleObject, bool bKeepState = true) = 0;
	virtual void UnregisterConsoleObject(const TCHAR* Name, bool bKeepState = true) = 0;
	static IConsoleManager& Get();
protected:
	virtual ~IConsoleManager() {}
};

class FAutoConsoleObject
{
protected:
	FAutoConsoleObject(IConsoleObject* InTarget);
public:
	virtual ~FAutoConsoleObject();
	IConsoleVariable* AsVariable();
	const IConsoleVariable* AsVariable() const;
};
class FAutoConsoleVariable : private FAutoConsoleObject
{
public:
	FAutoConsoleVariable(const TCHAR* Name, bool DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default);
	FAutoConsoleVariable(const TCHAR* Name, int32 DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default);
	FAutoConsoleVariable(const TCHAR* Name, float DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default);
	FAutoConsoleVariable(const TCHAR* Name, const TCHAR* DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default);
	IConsoleVariable& operator*();
	IConsoleVariable* operator->();
};
class FAutoConsoleVariableRef : private FAutoConsoleObject
{
public:
	FAutoConsoleVariableRef(const TCHAR* Name, int32& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default);
	FAutoConsoleVariableRef(const TCHAR* Name, float& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default);
	FAutoConsoleVariableRef(const TCHAR* Name, bool& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default);
	FAutoConsoleVariableRef(const TCHAR* Name, FString& RefValue, const TCHAR* Help, uint32 Flags = ECVF_Default);
	FAutoConsoleVariableRef(const TCHAR* Name, int32& RefValue, const TCHAR* Help, const FConsoleVariableDelegate& Callback, uint32 Flags = ECVF_Default);
	FAutoConsoleVariableRef(const TCHAR* Name, float& RefValue, const TCHAR* Help, const FConsoleVariableDelegate& Callback, uint32 Flags = ECVF_Default);
	FAutoConsoleVariableRef(const TCHAR* Name, bool& RefValue, const TCHAR* Help, const FConsoleVariableDelegate& Callback, uint32 Flags = ECVF_Default);
	IConsoleVariable& operator*();
	IConsoleVariable* operator->();
};

namespace UE::Stub
{
	template <typename T> concept CConsoleVariableType = std::is_same_v<T, bool> || std::is_same_v<T, int32> || std::is_same_v<T, float> || std::is_same_v<T, FString>;
}
template <class T>
class TAutoConsoleVariable : public FAutoConsoleObject
{
	static_assert(UE::Stub::CConsoleVariableType<T>, "TAutoConsoleVariable<T>: T must be bool, int32, float or FString");
public:
	TAutoConsoleVariable(const TCHAR* Name, const T& DefaultValue, const TCHAR* Help, uint32 Flags = ECVF_Default) : FAutoConsoleObject(nullptr) UE_STUB_BODY
	TAutoConsoleVariable(const TCHAR* Name, const T& DefaultValue, const TCHAR* Help, const FConsoleVariableDelegate& Callback, uint32 Flags = ECVF_Default) : FAutoConsoleObject(nullptr) UE_STUB_BODY
	T GetValueOnGameThread() const UE_STUB_BODY
	T GetValueOnAnyThread(bool bForceGameThread = false) const UE_STUB_BODY
	T GetValueOnRenderThread() const UE_STUB_BODY
	IConsoleVariable& operator*() UE_STUB_BODY
	const IConsoleVariable& operator*() const UE_STUB_BODY
	IConsoleVariable* operator->() UE_STUB_BODY
	const IConsoleVariable* operator->() const UE_STUB_BODY
};

class FAutoConsoleCommand : private FAutoConsoleObject
{
public:
	FAutoConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandDelegate& Command, uint32 Flags = ECVF_Default);
	FAutoConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithArgsDelegate& Command, uint32 Flags = ECVF_Default);
	FAutoConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldArgsAndOutputDeviceDelegate& Command, uint32 Flags = ECVF_Default);
};
class FAutoConsoleCommandWithWorld : private FAutoConsoleObject
{
public:
	FAutoConsoleCommandWithWorld(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldDelegate& Command, uint32 Flags = ECVF_Default);
};
class FAutoConsoleCommandWithWorldAndArgs : private FAutoConsoleObject
{
public:
	FAutoConsoleCommandWithWorldAndArgs(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldAndArgsDelegate& Command, uint32 Flags = ECVF_Default);
};
class FAutoConsoleCommandWithArgsAndOutputDevice : private FAutoConsoleObject
{
public:
	FAutoConsoleCommandWithArgsAndOutputDevice(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithArgsAndOutputDeviceDelegate& Command, uint32 Flags = ECVF_Default);
};
class FAutoConsoleCommandWithOutputDevice : private FAutoConsoleObject
{
public:
	FAutoConsoleCommandWithOutputDevice(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithOutputDeviceDelegate& Command, uint32 Flags = ECVF_Default);
};
class FAutoConsoleCommandWithWorldArgsAndOutputDevice : private FAutoConsoleObject
{
public:
	FAutoConsoleCommandWithWorldArgsAndOutputDevice(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldArgsAndOutputDeviceDelegate& Command, uint32 Flags = ECVF_Default);
};
