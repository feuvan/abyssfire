// Hooks of the in-game autotest's smoothness measurement (performance audit F2 / section 6).
//
// The autotest (UAbyssAutoTestSubsystem, only with -abyssautotest) points GAbyssPerfProbe at the frame record it owns;
// the module's own code counts into it: zone builds, spawns, the compile wait, quality refreshes, asset and texture
// loads, VFX pool creations, combat-log rebuilds, picks, the sim frame. In normal play the pointer is null and a hook is
// one test of a global on the game thread (a scope reads no clock); in Shipping builds, which never run the autotest,
// IsActive() is a constant false and the hooks compile to nothing.
//
// Game thread only. The arithmetic is in Framework/AbyssSmoothnessRules.h.
#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"

#include "Framework/AbyssSmoothnessRules.h"

/** The frame record of the running autotest, null otherwise (set and cleared by FAbyssAutoTestRun). */
inline AbyssSmoothnessRules::FProbeFrame* GAbyssPerfProbe = nullptr;

namespace AbyssPerfProbe
{
	using ECounter = AbyssSmoothnessRules::ECounter;

	inline bool IsActive()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return GAbyssPerfProbe != nullptr;
#endif
	}

	inline void Count(ECounter Counter, uint32 Num = 1)
	{
		if (IsActive())
		{
			GAbyssPerfProbe->Add(Counter, Num, 0.0);
		}
	}

	inline void AddMs(ECounter Counter, double Milliseconds, uint32 Num = 1)
	{
		if (IsActive())
		{
			GAbyssPerfProbe->Add(Counter, Num, Milliseconds);
		}
	}

	/** Counts one occurrence and its wall-clock duration (inclusive: scopes nest, e.g. a load inside a spawn). */
	class FScope
	{
	public:
		explicit FScope(ECounter InCounter)
			: Counter(InCounter)
			, StartSeconds(IsActive() ? FPlatformTime::Seconds() : 0.0)
		{
		}

		~FScope()
		{
			if (StartSeconds > 0.0 && IsActive())
			{
				GAbyssPerfProbe->Add(Counter, 1, (FPlatformTime::Seconds() - StartSeconds) * 1000.0);
			}
		}

		FScope(const FScope&) = delete;
		FScope& operator=(const FScope&) = delete;

		/** The occurrence turned out not to be one (a cache hit after all): nothing is counted. */
		void Cancel()
		{
			StartSeconds = 0.0;
		}

	private:
		ECounter Counter;
		double StartSeconds;
	};
}
