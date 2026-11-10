// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/HAL/Runnable.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

class FRunnable
{
public:
	virtual bool Init() { return true; }
	virtual uint32 Run() = 0;
	virtual void Stop() {}
	virtual void Exit() {}
	virtual class FSingleThreadRunnable* GetSingleThreadInterface() { return nullptr; }
	virtual ~FRunnable() {}
};
