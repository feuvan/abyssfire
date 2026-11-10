// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Tasks/Task.h (from UE 5.x source knowledge).
// UE::Tasks: Launch(DebugName, Body[, Prerequisites(...)][, Priority]) -> TTask<Result>; FTask = TTask<void>.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Misc/Timespan.h"
#include "Templates/Function.h"

namespace UE::Tasks
{
	enum class ETaskPriority : int8 { High, Normal, BackgroundHigh, BackgroundNormal, BackgroundLow, Count, Default = Normal, ForegroundCount = BackgroundHigh, Inherit };
	enum class EExtendedTaskPriority : int8 { None, Inline, TaskEvent, GameThreadNormalPri, GameThreadHiPri, RenderThreadNormalPri, RenderThreadHiPri };
	enum class ETaskFlags : uint32 { None, DoNotRunInsideBusyWait = 1 << 0 };

	namespace Private
	{
		class FTaskHandle
		{
		public:
			FTaskHandle() = default;
			bool IsValid() const;
			bool IsCompleted() const;
			bool Wait(FTimespan Timeout) const;
			bool Wait() const;
			bool TryRetractAndExecute();
			ETaskPriority GetPriority() const;
			bool operator==(const FTaskHandle& Other) const;
			bool operator!=(const FTaskHandle& Other) const;
		};
	}

	template <typename ResultType>
	class TTask : public Private::FTaskHandle
	{
	public:
		TTask() = default;
		ResultType& GetResult() UE_STUB_BODY
	};
	template <>
	class TTask<void> : public Private::FTaskHandle
	{
	public:
		TTask() = default;
		void GetResult();
	};
	using FTask = Private::FTaskHandle;

	class FTaskEvent : public Private::FTaskHandle
	{
	public:
		explicit FTaskEvent(const TCHAR* DebugName);
		// AddPrerequisites (one task or a collection of tasks) is not declared: unused by the module.
		void Trigger();
	};

	template <typename... TaskTypes> requires (std::is_base_of_v<Private::FTaskHandle, std::decay_t<TaskTypes>> && ...)
	TArray<Private::FTaskHandle, TInlineAllocator<sizeof...(TaskTypes)>> Prerequisites(TaskTypes&... Tasks) UE_STUB_BODY
	/** A collection of tasks is its own prerequisites list (the engine's body). */
	template <typename TaskCollectionType>
	const TaskCollectionType& Prerequisites(const TaskCollectionType& Tasks) { return Tasks; }

	template <typename TaskBody> requires std::is_invocable_v<std::decay_t<TaskBody>&>
	TTask<std::invoke_result_t<std::decay_t<TaskBody>&>> Launch(const TCHAR* DebugName, TaskBody&& Body, ETaskPriority Priority = ETaskPriority::Normal,
		EExtendedTaskPriority ExtendedPriority = EExtendedTaskPriority::None, ETaskFlags Flags = ETaskFlags::None) UE_STUB_BODY
	template <typename TaskBody, typename PrerequisitesCollectionType> requires std::is_invocable_v<std::decay_t<TaskBody>&> && requires(PrerequisitesCollectionType&& P) { P.Num(); }
	TTask<std::invoke_result_t<std::decay_t<TaskBody>&>> Launch(const TCHAR* DebugName, TaskBody&& Body, PrerequisitesCollectionType&& Prerequisites,
		ETaskPriority Priority = ETaskPriority::Normal, EExtendedTaskPriority ExtendedPriority = EExtendedTaskPriority::None, ETaskFlags Flags = ETaskFlags::None) UE_STUB_BODY

	// The free Wait(Tasks, Timeout) over a collection of tasks is not declared: unused by the module (FTaskHandle::Wait is).
	void AddNested(const FTask& Nested);
}
