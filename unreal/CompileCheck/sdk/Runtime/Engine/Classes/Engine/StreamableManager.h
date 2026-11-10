// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/StreamableManager.h (UE 5.x source knowledge; not
// reflected). The RequestAsyncLoad / RequestSyncLoad overloads are the long-standing ones (path array or single path,
// delegate or TFunction callback); 5.5+ fold them into templates that accept the same calls.
#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"

/** Defines FStreamableDelegate delegate interface. */
DECLARE_DELEGATE(FStreamableDelegate);
DECLARE_DELEGATE_OneParam(FStreamableUpdateDelegate, TSharedRef<struct FStreamableHandle>);

typedef int32 TAsyncLoadPriority;

/** A handle to a synchronous or async load. As long as the handle is Active, loaded assets will stay in memory. */
struct FStreamableHandle : public TSharedFromThis<FStreamableHandle>
{
	/** If this request has finished loading, meaning all available assets were loaded. */
	bool HasLoadCompleted() const;
	/** If this handle is still active, meaning it wasn't canceled or released. */
	bool IsActive() const;
	/** Release this handle: when all handles to an asset are released it can be garbage collected. The complete delegate still fires. */
	void ReleaseHandle();
	/** Cancel a request, callable from within the manager or externally. Also releases the handle; the complete delegate does not fire. */
	void CancelHandle();
	~FStreamableHandle();
};

/** A native class for managing streaming assets in and keeping them in memory. */
struct FStreamableManager : public FGCObject
{
	// Default priority for all async loads
	static constexpr TAsyncLoadPriority DefaultAsyncLoadPriority = 0;
	// Priority to try and load immediately
	static constexpr TAsyncLoadPriority AsyncLoadHighPriority = 100;

	/** The primary streamable operation: requests streaming of one or more target objects; the delegate is called when all are loaded. */
	TSharedPtr<FStreamableHandle> RequestAsyncLoad(TArray<FSoftObjectPath> TargetsToStream, FStreamableDelegate DelegateToCall = FStreamableDelegate(),
		TAsyncLoadPriority Priority = DefaultAsyncLoadPriority, bool bManageActiveHandle = false, bool bStartStalled = false, FString DebugName = TEXT("RequestAsyncLoad ArrayDelegate"));
	TSharedPtr<FStreamableHandle> RequestAsyncLoad(const FSoftObjectPath& TargetToStream, FStreamableDelegate DelegateToCall = FStreamableDelegate(),
		TAsyncLoadPriority Priority = DefaultAsyncLoadPriority, bool bManageActiveHandle = false, bool bStartStalled = false, FString DebugName = TEXT("RequestAsyncLoad SingleDelegate"));
	/** Lambda wrappers. Be aware that Callback may go off multiple seconds in the future. */
	TSharedPtr<FStreamableHandle> RequestAsyncLoad(TArray<FSoftObjectPath> TargetsToStream, TFunction<void()>&& Callback,
		TAsyncLoadPriority Priority = DefaultAsyncLoadPriority, bool bManageActiveHandle = false, bool bStartStalled = false, FString DebugName = TEXT("RequestAsyncLoad ArrayLambda"));
	TSharedPtr<FStreamableHandle> RequestAsyncLoad(const FSoftObjectPath& TargetToStream, TFunction<void()>&& Callback,
		TAsyncLoadPriority Priority = DefaultAsyncLoadPriority, bool bManageActiveHandle = false, bool bStartStalled = false, FString DebugName = TEXT("RequestAsyncLoad SingleLambda"));
	/** Synchronously load a set of assets, and return a handle. This can be very slow and may stall the game thread for several seconds. */
	TSharedPtr<FStreamableHandle> RequestSyncLoad(TArray<FSoftObjectPath> TargetsToStream, bool bManageActiveHandle = false, FString DebugName = TEXT("RequestSyncLoad Array"));
	TSharedPtr<FStreamableHandle> RequestSyncLoad(const FSoftObjectPath& TargetToStream, bool bManageActiveHandle = false, FString DebugName = TEXT("RequestSyncLoad Single"));
	/** Synchronously load the referred asset and return the loaded object, or nullptr if it can't be found. */
	UObject* LoadSynchronous(const FSoftObjectPath& Target, bool bManageActiveHandle = false, TSharedPtr<FStreamableHandle>* RequestHandlePointer = nullptr);
	template <typename T>
	T* LoadSynchronous(const FSoftObjectPath& Target, bool bManageActiveHandle = false, TSharedPtr<FStreamableHandle>* RequestHandlePointer = nullptr)
	{
		return Cast<T>(LoadSynchronous(Target, bManageActiveHandle, RequestHandlePointer));
	}
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	FStreamableManager();
	~FStreamableManager();
};
