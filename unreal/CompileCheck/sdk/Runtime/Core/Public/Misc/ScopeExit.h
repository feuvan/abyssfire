// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/ScopeExit.h (from UE 5.x source knowledge).
#pragma once

#include "Templates/UnrealTemplate.h"

namespace ScopeExitSupport
{
	template <typename FuncType>
	class TScopeGuard : public FNoncopyable
	{
	public:
		explicit TScopeGuard(FuncType&& InFunc) : Func(static_cast<FuncType&&>(InFunc)) {}
		~TScopeGuard() { Func(); }
	private:
		FuncType Func;
	};
	struct FScopeGuardSyntaxSupport
	{
		template <typename FuncType>
		TScopeGuard<FuncType> operator+(FuncType&& InFunc) { return TScopeGuard<FuncType>(static_cast<FuncType&&>(InFunc)); }
	};
}
#define UE_PRIVATE_SCOPE_EXIT_JOIN(A, B) UE_PRIVATE_SCOPE_EXIT_JOIN_INNER(A, B)
#define UE_PRIVATE_SCOPE_EXIT_JOIN_INNER(A, B) A##B
#define ON_SCOPE_EXIT const auto UE_PRIVATE_SCOPE_EXIT_JOIN(ScopeGuard_, __LINE__) = ::ScopeExitSupport::FScopeGuardSyntaxSupport() + [&]()
