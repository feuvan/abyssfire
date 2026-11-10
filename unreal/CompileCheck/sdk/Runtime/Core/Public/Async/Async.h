// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Async/Async.h (from UE 5.x source knowledge).
// AsyncTask(ENamedThreads::GameThread, [...]{}) / Async(EAsyncExecution::..., [...]{}) -> TFuture<Result>.
#pragma once

#include "CoreTypes.h"
#include "Async/Future.h"
#include "Async/TaskGraphInterfaces.h"
#include "Templates/Function.h"

enum class EAsyncExecution
{
	TaskGraph,
	TaskGraphMainThread,
	Thread,
	ThreadIfForkSafe,
	ThreadPool,
	LargeThreadPool,
};

template <typename CallableType>
auto Async(EAsyncExecution Execution, CallableType&& Callable, TUniqueFunction<void()> CompletionCallback = nullptr)
	-> TFuture<decltype(static_cast<CallableType&&>(Callable)())> UE_STUB_BODY
template <typename CallableType>
auto AsyncPool(class FQueuedThreadPool& ThreadPool, CallableType&& Callable, TUniqueFunction<void()> CompletionCallback = nullptr)
	-> TFuture<decltype(static_cast<CallableType&&>(Callable)())> UE_STUB_BODY
void AsyncTask(ENamedThreads::Type Thread, TUniqueFunction<void()> Function);
