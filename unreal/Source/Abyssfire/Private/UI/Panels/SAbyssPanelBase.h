// SAbyssPanelBase: common behaviour of every panel (save-ui-input.md 7.0).
//
// * Rebuild model (7.0.7): the panel's content is rebuilt from the snapshot by Refresh() whenever it is marked dirty (a
//   relevant core event, a local view change) and, for panels showing live values, every GetAutoRefreshSeconds(). Panels
//   copy what they show: snapshot pointers are never kept.
// * Placement: the web's design size and top-left in the 1280x720 layout (horizontally centred in wider layouts); on
//   touch the panel is scaled up to fill the screen (fitPanelForMobile, 7.0.6) and centred.
// * Modal panels add a dim backdrop that blocks the world and closes the panel on click (7.0.5).
// * Opening animation: scale 0.92 -> 1 and alpha 0 -> 1 over 150 ms Back.easeOut (7.0.4), driven by the host.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "abyss/sim/SimTypes.h"
#include "abyss/sim/Snapshot.h"

#include "UI/Core/AbyssUiContext.h"

class SBox;

class SAbyssPanelBase : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssPanelBase) {}
	SLATE_END_ARGS()

	/** Base construction: subclasses call it first, then build their frame with SetPanelContent. */
	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	virtual abyss::PanelId GetPanelId() const = 0;
	/** Rebuilds the content from the snapshot. */
	virtual void Refresh(const abyss::Snapshot& Snap) = 0;
	/** Web design size (logical px). */
	virtual FVector2D GetDesignSize() const = 0;
	/** Web top-left in 1280x720; X < 0 = centred horizontally, Y < 0 = centred vertically. */
	virtual FVector2D GetDesignPosition() const { return FVector2D(-1.0, -1.0); }
	virtual bool IsModal() const { return false; }
	virtual float GetBackdropAlpha() const { return 0.6f; }
	/** Backdrop click closes the panel (all modal panels except the system menu / confirms). */
	virtual bool CloseOnBackdrop() const { return true; }
	/** Esc / Back: return true when an inner state (popup, sub view) consumed it. */
	virtual bool HandleBack() { return false; }
	/** Seconds between automatic rebuilds (0 = only when dirty). */
	virtual double GetAutoRefreshSeconds() const { return 0.0; }
	/** Called when the panel becomes visible / is removed. */
	virtual void OnOpened() {}
	virtual void OnClosed() {}
	/** NPC this panel belongs to (core-owned modals). */
	virtual std::string GetNpcId() const { return std::string(); }
	/** Per frame while open (after the refresh check). */
	virtual void TickPanel(const abyss::Snapshot& Snap, double Now) {}

	void MarkDirty() { bDirty = true; }
	/** Refresh when dirty or due; returns true when it rebuilt. */
	bool MaybeRefresh(const abyss::Snapshot& Snap, double Now);
	void SetCloseHandler(FSimpleDelegate InHandler) { CloseHandler = MoveTemp(InHandler); }
	double GetOpenedTime() const { return OpenedTime; }
	void SetOpenedTime(double Time) { OpenedTime = Time; }

protected:
	/** Asks the host to close this panel (reports CmdClosePanel for UE-owned and core-owned panels alike). */
	void RequestClose();
	/** The frame / body widget (called once by subclasses). */
	void SetPanelContent(const TSharedRef<SWidget>& Content);

	TSharedPtr<FAbyssUiContext> Ctx;
	FSimpleDelegate CloseHandler;
	bool bDirty = true;
	double LastRefreshTime = -1.0;
	double OpenedTime = 0.0;
};
