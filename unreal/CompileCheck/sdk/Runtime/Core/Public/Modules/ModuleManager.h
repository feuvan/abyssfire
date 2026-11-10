// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Modules/ModuleManager.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FModuleManager
{
public:
	static FModuleManager& Get();
	bool IsModuleLoaded(const FName InModuleName) const;
	IModuleInterface* LoadModule(const FName InModuleName);
	IModuleInterface& LoadModuleChecked(const FName InModuleName);
	bool UnloadModule(const FName InModuleName, bool bIsShutdown = false);
	IModuleInterface* GetModule(const FName InModuleName);
	bool ModuleExists(const TCHAR* ModuleName, FString* OutModuleFilePath = nullptr) const;
	template <typename TModuleInterface> static TModuleInterface& GetModuleChecked(const FName ModuleName) requires std::is_base_of_v<IModuleInterface, TModuleInterface> UE_STUB_BODY
	template <typename TModuleInterface> static TModuleInterface* GetModulePtr(const FName ModuleName) requires std::is_base_of_v<IModuleInterface, TModuleInterface> UE_STUB_BODY
	template <typename TModuleInterface> static TModuleInterface& LoadModuleChecked(const FName InModuleName) requires std::is_base_of_v<IModuleInterface, TModuleInterface> UE_STUB_BODY
	template <typename TModuleInterface> static TModuleInterface* LoadModulePtr(const FName InModuleName) requires std::is_base_of_v<IModuleInterface, TModuleInterface> UE_STUB_BODY
};

class FDefaultModuleImpl : public IModuleInterface
{
};
class FDefaultGameModuleImpl : public FDefaultModuleImpl
{
	virtual bool IsGameModule() const override { return true; }
};

namespace UE::Stub
{
	/** The module class must be default-constructible and derive from IModuleInterface (what the registrant needs). */
	template <typename ModuleClass>
	struct TStaticModuleRegistrant
	{
		explicit TStaticModuleRegistrant(const TCHAR* InModuleName)
		{
			static_assert(std::is_base_of_v<IModuleInterface, ModuleClass>, "IMPLEMENT_MODULE: the module class must derive from IModuleInterface");
			static_assert(std::is_default_constructible_v<ModuleClass>, "IMPLEMENT_MODULE: the module class must be default-constructible");
		}
	};
}
#define IMPLEMENT_MODULE(ModuleImplClass, ModuleName) \
	static UE::Stub::TStaticModuleRegistrant<ModuleImplClass> ModuleRegistrant##ModuleName(TEXT(#ModuleName));
#define IMPLEMENT_GAME_MODULE(ModuleImplClass, ModuleName) IMPLEMENT_MODULE(ModuleImplClass, ModuleName)
#define IMPLEMENT_PRIMARY_GAME_MODULE(ModuleImplClass, ModuleName, GameName) \
	static_assert(sizeof(GameName) > 1, "IMPLEMENT_PRIMARY_GAME_MODULE: GameName must be a (narrow) string literal"); \
	IMPLEMENT_GAME_MODULE(ModuleImplClass, ModuleName)
