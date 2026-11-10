// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/AssetCompilingManager.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"

class UObject;

class FAssetCompilingManager
{
public:
	static FAssetCompilingManager& Get();
	/** Blocks until completion of all assets. */
	void FinishAllCompilation();
	/** Blocks until completion of the requested objects. */
	void FinishCompilationForObjects(TArrayView<UObject* const> InObjects);
	/** Returns the number of remaining compilations. */
	int32 GetNumRemainingAssets() const;
	/** Called once per frame, fetches completed tasks and applies them to the scene. */
	void ProcessAsyncTasks(bool bLimitExecutionTime = false);
	void Shutdown();
private:
	FAssetCompilingManager();
	~FAssetCompilingManager();
};
