// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/ShaderCompiler.h (UE 5.x source knowledge). Only the
// global manager and its blocking calls are declared.
#pragma once

#include "CoreMinimal.h"

class FShaderCompilingManager
{
public:
	/** Returns whether async compiling is happening. */
	bool IsCompiling() const;
	int32 GetNumRemainingJobs() const;
	/** Blocks until completion of all async shader compiling, and assigns shader maps to relevant materials. */
	void FinishAllCompilation();
	void ProcessAsyncResults(bool bLimitExecutionTime, bool bBlockOnGlobalShaderCompletion);
};

/** The global shader compiling thread manager. */
extern FShaderCompilingManager* GShaderCompilingManager;
