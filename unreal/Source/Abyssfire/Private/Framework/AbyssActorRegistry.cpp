#include "Framework/AbyssActorRegistry.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Framework/AbyssPresenter.h"

UAbyssActorRegistry* UAbyssActorRegistry::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UAbyssActorRegistry>() : nullptr;
}

bool UAbyssActorRegistry::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAbyssActorRegistry::Register(abyss::EntityId Id, AActor* Actor, abyss::EntityKind Kind)
{
	check(IsInGameThread());
	if (Id == abyss::kNoEntity || Actor == nullptr)
	{
		return;
	}
	if (const FEntry* Previous = Entries.Find(Id))
	{
		if (AActor* PreviousActor = Previous->Actor.Get(); PreviousActor != nullptr && PreviousActor != Actor)
		{
			IdsByActor.Remove(TObjectKey<AActor>(PreviousActor));
		}
	}
	FEntry& Entry = Entries.FindOrAdd(Id);
	Entry.Actor = Actor;
	Entry.Kind = Kind;
	IdsByActor.Add(TObjectKey<AActor>(Actor), Id);
}

void UAbyssActorRegistry::Unregister(abyss::EntityId Id, const AActor* Expected)
{
	check(IsInGameThread());
	const FEntry* Entry = Entries.Find(Id);
	if (Entry == nullptr)
	{
		return;
	}
	AActor* Current = Entry->Actor.Get();
	if (Expected != nullptr && Current != nullptr && Current != Expected)
	{
		return;
	}
	if (Current != nullptr)
	{
		IdsByActor.Remove(TObjectKey<AActor>(Current));
	}
	Entries.Remove(Id);
}

void UAbyssActorRegistry::Reset()
{
	Entries.Reset();
	IdsByActor.Reset();
}

AActor* UAbyssActorRegistry::Find(abyss::EntityId Id) const
{
	const FEntry* Entry = Entries.Find(Id);
	return Entry ? Entry->Actor.Get() : nullptr;
}

IAbyssPresenter* UAbyssActorRegistry::FindPresenter(abyss::EntityId Id) const
{
	return Cast<IAbyssPresenter>(Find(Id));
}

abyss::EntityKind UAbyssActorRegistry::FindKind(abyss::EntityId Id) const
{
	const FEntry* Entry = Entries.Find(Id);
	return Entry ? Entry->Kind : abyss::EntityKind::None;
}

abyss::EntityId UAbyssActorRegistry::FindId(const AActor* Actor) const
{
	if (Actor == nullptr)
	{
		return abyss::kNoEntity;
	}
	const uint32* Id = IdsByActor.Find(TObjectKey<AActor>(Actor));
	return Id ? *Id : abyss::kNoEntity;
}

void UAbyssActorRegistry::ForEach(TFunctionRef<void(abyss::EntityId, AActor*, abyss::EntityKind)> Visitor) const
{
	// Copy first: the visitor may register / unregister.
	TArray<TPair<uint32, FEntry>> Copy;
	Copy.Reserve(Entries.Num());
	for (const TPair<uint32, FEntry>& Pair : Entries)
	{
		Copy.Add(Pair);
	}
	for (const TPair<uint32, FEntry>& Pair : Copy)
	{
		if (AActor* Actor = Pair.Value.Actor.Get())
		{
			Visitor(Pair.Key, Actor, Pair.Value.Kind);
		}
	}
}

void UAbyssActorRegistry::Deinitialize()
{
	Reset();
	Super::Deinitialize();
}
