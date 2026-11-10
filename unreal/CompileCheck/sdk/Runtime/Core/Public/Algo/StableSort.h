// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Algo/StableSort.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Algo/Sort.h"

namespace Algo
{
	template <typename RangeType> requires UE::Stub::CContiguousRange<RangeType>
	void StableSort(RangeType&& Range) UE_STUB_BODY
	template <typename RangeType, typename PredicateType>
		requires UE::Stub::CContiguousRange<RangeType> && std::is_invocable_r_v<bool, PredicateType&, const UE::Stub::TRangeElement<RangeType>&, const UE::Stub::TRangeElement<RangeType>&>
	void StableSort(RangeType&& Range, PredicateType Pred) UE_STUB_BODY
	template <typename RangeType, typename ProjectionType>
		requires UE::Stub::CContiguousRange<RangeType> && std::is_invocable_v<ProjectionType&, const UE::Stub::TRangeElement<RangeType>&>
	void StableSortBy(RangeType&& Range, ProjectionType Proj) UE_STUB_BODY
	template <typename RangeType, typename ProjectionType, typename PredicateType>
		requires UE::Stub::CContiguousRange<RangeType> && std::is_invocable_v<ProjectionType&, const UE::Stub::TRangeElement<RangeType>&>
	void StableSortBy(RangeType&& Range, ProjectionType Proj, PredicateType Pred) UE_STUB_BODY
}
