// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/CoreDelegates.h (from UE 5.x source knowledge).
// Application lifecycle delegates. The lifecycle ones are thread-safe multicast delegates (FTSSimpleMulticastDelegate,
// 5.1+): AddUObject / AddLambda / Remove work as on any multicast delegate.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"

class FCoreDelegates
{
public:
	DECLARE_TS_MULTICAST_DELEGATE(FApplicationLifetimeDelegate);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUserActivityStringChanged, const FString&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FApplicationReceivedOnScreenOrientationChangedNotificationDelegate, int32);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSystemResolutionChanged, uint32);

	static FSimpleMulticastDelegate OnPreExit;
	static FSimpleMulticastDelegate OnExit;
	static FSimpleMulticastDelegate OnEnginePreExit;
	static FSimpleMulticastDelegate OnPostEngineInit;
	static FSimpleMulticastDelegate OnAllModuleLoadingPhasesComplete;
	static FSimpleMulticastDelegate OnFEngineLoopInitComplete;
	static FSimpleMulticastDelegate OnBeginFrame;
	static FSimpleMulticastDelegate OnEndFrame;
	static FSimpleMulticastDelegate OnBeginFrameRT;
	static FSimpleMulticastDelegate OnEndFrameRT;
	static FSimpleMulticastDelegate OnHandleSystemError;
	static FSimpleMulticastDelegate OnHandleSystemEnsure;
	static FSimpleMulticastDelegate OnShutdownAfterError;
	static FSimpleMulticastDelegate OnSafeFrameChangedEvent;
	static FTSSimpleMulticastDelegate& GetMemoryTrimDelegate();
	static FTSSimpleMulticastDelegate& GetLowLevelAllocatorMemoryTrimDelegate();
	static FTSSimpleMulticastDelegate& GetOutOfMemoryDelegate();

	static FApplicationLifetimeDelegate ApplicationWillDeactivateDelegate;
	static FApplicationLifetimeDelegate ApplicationHasReactivatedDelegate;
	static FApplicationLifetimeDelegate ApplicationWillEnterBackgroundDelegate;
	static FApplicationLifetimeDelegate ApplicationHasEnteredForegroundDelegate;
	static FApplicationLifetimeDelegate ApplicationWillTerminateDelegate;
	// UNVERIFIED (API_UNCERTAIN.md #3): the accessor that replaced direct use of ApplicationWillTerminateDelegate in 5.x.
	// Believed real, not confirmed for 5.8, not reflected. Declared (on by default) because
	// Private/Framework/AbyssGameInstance.cpp:65,82 calls it; the static member above is the older form.
	static FApplicationLifetimeDelegate& GetApplicationWillTerminateDelegate();
	// end UNVERIFIED
	static FApplicationLifetimeDelegate ApplicationShouldUnloadResourcesDelegate;
	static FOnUserActivityStringChanged UserActivityStringChanged;
	static FApplicationReceivedOnScreenOrientationChangedNotificationDelegate ApplicationReceivedScreenOrientationChangedNotificationDelegate;
	static FOnSystemResolutionChanged OnSystemResolutionChanged;
};
