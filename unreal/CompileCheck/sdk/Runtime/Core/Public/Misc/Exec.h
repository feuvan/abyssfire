// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/Exec.h (UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

class UWorld;
class FOutputDevice;

class FExec
{
public:
	virtual ~FExec();
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);
};
