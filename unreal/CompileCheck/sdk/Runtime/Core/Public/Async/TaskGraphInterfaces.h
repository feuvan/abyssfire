// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Async/TaskGraphInterfaces.h (+ TaskGraphFwd.h).
// From UE 5.x source knowledge.
#pragma once

#include "CoreTypes.h"
#include "Templates/Function.h"
#include "Stats/Stats.h"

namespace ENamedThreads
{
	enum Type : int32
	{
		UnusedAnchor = -1,
		RHIThread,
		GameThread,
		ActualRenderingThread = GameThread + 1,
		AnyThread = 0xff,
		MainQueue = 0x000,
		LocalQueue = 0x100,
		NumQueues = 2,
		ThreadIndexMask = 0xff,
		QueueIndexMask = 0x100,
		QueueIndexShift = 8,
		NormalTaskPriority = 0x000,
		HighTaskPriority = 0x200,
		NormalThreadPriority = 0x000,
		HighThreadPriority = 0x400,
		BackgroundThreadPriority = 0x800,
		GameThread_Local = GameThread | LocalQueue,
		ActualRenderingThread_Local = ActualRenderingThread | LocalQueue,
		AnyHiPriThreadNormalTask = AnyThread | HighThreadPriority | NormalTaskPriority,
		AnyHiPriThreadHiPriTask = AnyThread | HighThreadPriority | HighTaskPriority,
		AnyNormalThreadNormalTask = AnyThread | NormalThreadPriority | NormalTaskPriority,
		AnyNormalThreadHiPriTask = AnyThread | NormalThreadPriority | HighTaskPriority,
		AnyBackgroundThreadNormalTask = AnyThread | BackgroundThreadPriority | NormalTaskPriority,
		AnyBackgroundHiPriTask = AnyThread | BackgroundThreadPriority | HighTaskPriority,
	};
}
namespace ESubsequentsMode { enum Type { TrackSubsequents, FireAndForget }; }
