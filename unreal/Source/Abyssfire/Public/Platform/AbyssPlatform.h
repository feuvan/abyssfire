// Platform helpers: touch / desktop layout, quality tier selection, screen sleep (ue58-platform.md 6.8, 8.3, 13).
#pragma once

#include "CoreMinimal.h"

#include "Platform/AbyssSettings.h"

namespace AbyssPlatform
{
	/** iOS / Android build. */
	ABYSSFIRE_API bool IsMobilePlatform();

	/**
	 * Touch layout? Setting Desktop / Touch forces it; Auto = mobile platform, or a desktop run with -faketouches
	 * (mouse produces touch events) or -abysstouch (force the touch layout for testing).
	 */
	ABYSSFIRE_API bool ResolveTouchMode(EAbyssControlLayout Layout);

	/**
	 * The render tier (ue58-platform.md 6.8). Explicit setting wins; else the console variable abyss.Quality (0/1/2,
	 * set by device profiles on mobile or -abyssquality=<0|1|2|low|balanced|high>); else on mobile Balanced; else the
	 * web rule (RenderQuality.ts:50-62) on the default window: <= 4 cores or <= 4 GB -> Low; physical pixel budget
	 * W*H > 5 000 000 -> Low; DPI scale >= 2 and budget <= 3 700 000 -> High; else Balanced.
	 */
	ABYSSFIRE_API EAbyssQualityTier ResolveQualityTier(EAbyssQualitySetting Setting);

	/** abyss.RenderScale override (0 = automatic per tier: 3D resolution height cap 720 / 1080 / 1440 px). */
	ABYSSFIRE_API float GetRenderScaleOverride();

	/** Keeps the screen awake during gameplay (ue58-platform.md 13). */
	ABYSSFIRE_API void SetScreenSaverAllowed(bool bAllowed);
}
