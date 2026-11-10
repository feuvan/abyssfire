// Stub SDK. Real header: Engine/Source/Runtime/RHI/Public/DynamicRHI.h (UE 5.x source knowledge).
// GDynamicRHI and the little a game normally asks it (name, interface type).
#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

enum class ERHIInterfaceType { Hidden, Null, D3D11, D3D12, Vulkan, Metal, Agility, OpenGL };

class FDynamicRHI
{
public:
	virtual ~FDynamicRHI() {}
	virtual void Init() = 0;
	virtual void Shutdown() = 0;
	virtual const TCHAR* GetName() = 0;
	virtual ERHIInterfaceType GetInterfaceType() const { return ERHIInterfaceType::Hidden; }
};
extern FDynamicRHI* GDynamicRHI;
ERHIInterfaceType RHIGetInterfaceType();
