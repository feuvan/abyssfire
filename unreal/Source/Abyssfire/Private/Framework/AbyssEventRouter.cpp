#include "Framework/AbyssEventRouter.h"

void FAbyssEventRouter::Dispatch(const abyss::Event& Event) const
{
	OnAnyEvent.Broadcast(Event);
	std::visit(
		[this](const auto& Typed)
		{
			using FEventType = std::decay_t<decltype(Typed)>;
			std::get<AbyssEventRouterPrivate::EventIndex<FEventType>>(Delegates).Broadcast(Typed);
		},
		Event);
}

void FAbyssEventRouter::RemoveAll(const void* UserObject)
{
	OnAnyEvent.RemoveAll(UserObject);
	OnFrame.RemoveAll(UserObject);
	OnSessionStarted.RemoveAll(UserObject);
	OnSessionEnded.RemoveAll(UserObject);
	std::apply(
		[UserObject](auto&... Each)
		{
			(static_cast<void>(Each.RemoveAll(UserObject)), ...);
		},
		Delegates);
}
