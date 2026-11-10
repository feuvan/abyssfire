// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Stats/Stats.h (+ Stats2.h, StatsCommon.h).
// From UE 5.x source knowledge. Stat macros compile to declarations of TStatId; with STATS 0 (Shipping / Test) the
// declaring macros expand to nothing and RETURN_QUICK_DECLARE_CYCLE_STAT returns an empty TStatId.
#pragma once

#include "CoreTypes.h"
#include "UObject/NameTypes.h"

struct TStatId
{
	TStatId() = default;
	bool IsValidStat() const;
	bool IsNone() const;
	FName GetName() const;
	bool operator==(TStatId Other) const;
	bool operator!=(TStatId Other) const;
};
uint32 GetTypeHash(TStatId StatId);

class FScopeCycleCounter
{
public:
	FScopeCycleCounter(TStatId StatId, bool bAlways = false);
	~FScopeCycleCounter();
};

#if STATS
	struct FStatGroup_STATGROUP_Tickables {};
	struct FStatGroup_STATGROUP_Game {};
	struct FStatGroup_STATGROUP_Quick {};
	struct FStatGroup_STATGROUP_Slate {};
	struct FStatGroup_STATGROUP_Audio {};
	struct FStatGroup_STATGROUP_Engine {};
	namespace UE::Stub { TStatId MakeStatId(const TCHAR* Description, const ANSICHAR* StatName); }
	#define DECLARE_STATS_GROUP(GroupDesc, GroupId, GroupCat) struct FStatGroup_##GroupId {};
	#define DECLARE_STATS_GROUP_VERBOSE(GroupDesc, GroupId, GroupCat) struct FStatGroup_##GroupId {};
	#define STATCAT_Advanced 0
	#define DECLARE_CYCLE_STAT(CounterName, StatId, GroupId) \
		static struct FStat_##StatId { using TGroup = FStatGroup_##GroupId; static TStatId GetStatId() { return UE::Stub::MakeStatId(CounterName, #StatId); } } StatPtr_##StatId;
	#define DECLARE_CYCLE_STAT_EXTERN(CounterName, StatId, GroupId, API) \
		struct FStat_##StatId { using TGroup = FStatGroup_##GroupId; static TStatId GetStatId() { return UE::Stub::MakeStatId(CounterName, #StatId); } }; extern API FStat_##StatId StatPtr_##StatId;
	#define DEFINE_STAT(Stat) FStat_##Stat StatPtr_##Stat;
	#define GET_STATID(Stat) (FStat_##Stat::GetStatId())
	#define SCOPE_CYCLE_COUNTER(Stat) FScopeCycleCounter CycleCount_##Stat(GET_STATID(Stat));
	#define QUICK_SCOPE_CYCLE_COUNTER(Stat) \
		struct FStat_##Stat { using TGroup = FStatGroup_STATGROUP_Quick; static TStatId GetStatId() { return UE::Stub::MakeStatId(TEXT(#Stat), #Stat); } }; \
		FScopeCycleCounter CycleCount_##Stat(FStat_##Stat::GetStatId());
	#define RETURN_QUICK_DECLARE_CYCLE_STAT(StatId, GroupId) \
		struct FStat_##StatId { using TGroup = FStatGroup_##GroupId; static TStatId GetStatId() { return UE::Stub::MakeStatId(TEXT(#StatId), #StatId); } }; \
		return FStat_##StatId::GetStatId();
	#define DECLARE_DWORD_COUNTER_STAT(CounterName, StatId, GroupId) DECLARE_CYCLE_STAT(CounterName, StatId, GroupId)
	#define DECLARE_FLOAT_COUNTER_STAT(CounterName, StatId, GroupId) DECLARE_CYCLE_STAT(CounterName, StatId, GroupId)
	#define DECLARE_DWORD_ACCUMULATOR_STAT(CounterName, StatId, GroupId) DECLARE_CYCLE_STAT(CounterName, StatId, GroupId)
	#define DECLARE_MEMORY_STAT(CounterName, StatId, GroupId) DECLARE_CYCLE_STAT(CounterName, StatId, GroupId)
	#define INC_DWORD_STAT(Stat) { (void)GET_STATID(Stat); }
	#define INC_DWORD_STAT_BY(Stat, Amount) { (void)GET_STATID(Stat); (void)(Amount); }
	#define DEC_DWORD_STAT(Stat) { (void)GET_STATID(Stat); }
	#define SET_DWORD_STAT(Stat, Value) { (void)GET_STATID(Stat); (void)(Value); }
	#define SET_FLOAT_STAT(Stat, Value) { (void)GET_STATID(Stat); (void)(Value); }
	#define SET_MEMORY_STAT(Stat, Value) { (void)GET_STATID(Stat); (void)(Value); }
#else
	#define DECLARE_STATS_GROUP(GroupDesc, GroupId, GroupCat)
	#define DECLARE_STATS_GROUP_VERBOSE(GroupDesc, GroupId, GroupCat)
	#define DECLARE_CYCLE_STAT(CounterName, StatId, GroupId)
	#define DECLARE_CYCLE_STAT_EXTERN(CounterName, StatId, GroupId, API)
	#define DEFINE_STAT(Stat)
	#define GET_STATID(Stat) (TStatId())
	#define SCOPE_CYCLE_COUNTER(Stat)
	#define QUICK_SCOPE_CYCLE_COUNTER(Stat)
	#define RETURN_QUICK_DECLARE_CYCLE_STAT(StatId, GroupId) return TStatId();
	#define DECLARE_DWORD_COUNTER_STAT(CounterName, StatId, GroupId)
	#define DECLARE_FLOAT_COUNTER_STAT(CounterName, StatId, GroupId)
	#define DECLARE_DWORD_ACCUMULATOR_STAT(CounterName, StatId, GroupId)
	#define DECLARE_MEMORY_STAT(CounterName, StatId, GroupId)
	#define INC_DWORD_STAT(Stat)
	#define INC_DWORD_STAT_BY(Stat, Amount)
	#define DEC_DWORD_STAT(Stat)
	#define SET_DWORD_STAT(Stat, Value)
	#define SET_FLOAT_STAT(Stat, Value)
	#define SET_MEMORY_STAT(Stat, Value)
#endif
#define TRACE_CPUPROFILER_EVENT_SCOPE(Name)
#define TRACE_CPUPROFILER_EVENT_SCOPE_STR(NameStr)
#define TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(Name)
