// Typed fan-out of the core's presentation events (abyss::Event = std::variant of Ev* structs, abyss/sim/Events.h).
//
// One native multicast delegate per event type, generated from the variant, so adding an event to the core needs no
// change here. Subscribing:
//
//   FAbyssEventRouter& Router = UAbyssGameInstance::Get(this)->GetEventRouter();
//   Router.On<abyss::EvHit>().AddUObject(this, &UMyThing::HandleHit);          // void HandleHit(const abyss::EvHit&)
//   Router.On<abyss::EvMusic>().AddWeakLambda(this, [this](const abyss::EvMusic& E) { ... });
//   ...
//   Router.RemoveAll(this);   // in Deinitialize / EndPlay / destructor
//
// Rules:
// * Game thread only. Events are delivered by UAbyssSimDriver right after GameSim::Frame / Step, in emission order;
//   handlers run synchronously inside that dispatch.
// * The order between subscribers of the SAME event type is unspecified (UE multicast delegates); ordering that matters
//   (actor spawn before anything that looks the actor up, despawn after) is done by the driver around the broadcast
//   (see AbyssSimDriver.h and README.md).
// * Event payloads and the Snapshot are only valid during the call: copy what you keep. Never keep the pointer members
//   of abyss::Snapshot (live views into the core) across frames.
// * A handler may Submit commands (queued for the next step). It must not start / end a session; the GameInstance flow
//   functions defer themselves to the end of the dispatch when called from a handler.
#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <variant>

#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssTypes.h"

namespace AbyssEventRouterPrivate
{
	template <class T, class... Ts>
	constexpr size_t IndexOf()
	{
		constexpr bool Matches[] = { std::is_same_v<T, Ts>... };
		for (size_t I = 0; I < sizeof...(Ts); ++I)
		{
			if (Matches[I])
			{
				return I;
			}
		}
		return sizeof...(Ts);
	}

	template <class T, class Variant>
	struct TVariantIndex;

	template <class T, class... Ts>
	struct TVariantIndex<T, std::variant<Ts...>>
	{
		static constexpr size_t Value = IndexOf<T, Ts...>();
	};

	template <class Variant>
	struct TDelegateTupleOf;

	template <class... Ts>
	struct TDelegateTupleOf<std::variant<Ts...>>
	{
		using Type = std::tuple<TMulticastDelegate<void(const Ts&)>...>;
	};

	/** Index of an event struct in abyss::Event. */
	template <class EventT>
	inline constexpr size_t EventIndex = TVariantIndex<EventT, abyss::Event>::Value;
}

class ABYSSFIRE_API FAbyssEventRouter
{
public:
	template <class EventT>
	using TEventDelegate = TMulticastDelegate<void(const EventT&)>;

	FAbyssEventRouter() = default;
	FAbyssEventRouter(const FAbyssEventRouter&) = delete;
	FAbyssEventRouter& operator=(const FAbyssEventRouter&) = delete;

	/** The delegate of one event type (abyss::EvHit, abyss::EvMusic, ...). */
	template <class EventT>
	TEventDelegate<EventT>& On()
	{
		constexpr size_t Index = AbyssEventRouterPrivate::EventIndex<EventT>;
		static_assert(Index < std::variant_size_v<abyss::Event>, "EventT is not an alternative of abyss::Event");
		return std::get<Index>(Delegates);
	}

	/** Every event, before its typed delegate (debug overlays, logging). */
	TMulticastDelegate<void(const abyss::Event&)> OnAnyEvent;
	/**
	 * Once per pumped frame after all events were dispatched, the world view synced and the UI root synced. The snapshot is
	 * the state after this frame's steps.
	 */
	TMulticastDelegate<void(const abyss::Snapshot&, const FAbyssFrameInfo&)> OnFrame;
	/** A session (NewGame / LoadGame) started: fired before its first events are dispatched. */
	TMulticastDelegate<void()> OnSessionStarted;
	/** The session ended (return to menu, quit): fired before the GameSim is destroyed. */
	TMulticastDelegate<void()> OnSessionEnded;

	/** OnAnyEvent, then the typed delegate. Called by UAbyssSimDriver only. */
	void Dispatch(const abyss::Event& Event) const;

	/** Unsubscribes an object from every delegate of the router. */
	void RemoveAll(const void* UserObject);

private:
	using FDelegateTuple = AbyssEventRouterPrivate::TDelegateTupleOf<abyss::Event>::Type;
	FDelegateTuple Delegates;
};
