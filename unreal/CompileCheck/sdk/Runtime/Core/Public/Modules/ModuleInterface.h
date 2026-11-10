// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Modules/ModuleInterface.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

class IModuleInterface
{
public:
	virtual ~IModuleInterface() {}
	virtual void StartupModule() {}
	virtual void PreUnloadCallback() {}
	virtual void PostLoadCallback() {}
	virtual void ShutdownModule() {}
	virtual bool SupportsDynamicReloading() { return true; }
	virtual bool SupportsAutomaticShutdown() { return true; }
	virtual bool IsGameModule() const { return false; }
};
