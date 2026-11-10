// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Algo/Sort.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/Less.h"

namespace UE::Stub
{
	template <typename RangeType> using TRangeElement = std::remove_reference_t<decltype(*GetData(std::declval<RangeType&>()))>;
	template <typename RangeType> concept CContiguousRange = requires(RangeType& Range) { GetData(Range); GetNum(Range); };
}
namespace Algo
{
	template <typename RangeType> requires UE::Stub::CContiguousRange<RangeType>
	void Sort(RangeType&& Range) UE_STUB_BODY
	template <typename RangeType, typename PredicateType>
		requires UE::Stub::CContiguousRange<RangeType> && std::is_invocable_r_v<bool, PredicateType&, const UE::Stub::TRangeElement<RangeType>&, const UE::Stub::TRangeElement<RangeType>&>
	void Sort(RangeType&& Range, PredicateType Pred) UE_STUB_BODY
	template <typename RangeType, typename ProjectionType>
		requires UE::Stub::CContiguousRange<RangeType> && std::is_invocable_v<ProjectionType&, const UE::Stub::TRangeElement<RangeType>&>
	void SortBy(RangeType&& Range, ProjectionType Proj) UE_STUB_BODY
	template <typename RangeType, typename ProjectionType, typename PredicateType>
		requires UE::Stub::CContiguousRange<RangeType> && std::is_invocable_v<ProjectionType&, const UE::Stub::TRangeElement<RangeType>&>
	void SortBy(RangeType&& Range, ProjectionType Proj, PredicateType Pred) UE_STUB_BODY
}
