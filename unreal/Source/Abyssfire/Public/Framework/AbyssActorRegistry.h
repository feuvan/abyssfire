// EntityId -> actor registry for one game world. The world view registers every actor it spawns for a core entity;
// the driver uses it to route entity-addressed events to IAbyssPresenter; VFX / UI / input use it to find actors
// (attach points, nameplates, picking). Entries are weak: a destroyed actor simply stops being found.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/Function.h"
#include "UObject/ObjectKey.h"
#include "UObject/WeakObjectPtr.h"

#include "abyss/base/Types.h"

#include "AbyssActorRegistry.generated.h"

class AActor;
class IAbyssPresenter;

UCLASS()
class ABYSSFIRE_API UAbyssActorRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAbyssActorRegistry* Get(const UObject* WorldContextObject);

	/** Registers (or replaces) the actor of an entity. */
	void Register(abyss::EntityId Id, AActor* Actor, abyss::EntityKind Kind);
	/** Removes an entity; when Expected is set, only if it is still the registered actor. */
	void Unregister(abyss::EntityId Id, const AActor* Expected = nullptr);
	/** Forgets everything (zone teardown / session end). Does not destroy actors. */
	void Reset();

	AActor* Find(abyss::EntityId Id) const;
	template <class T>
	T* FindAs(abyss::EntityId Id) const
	{
		return Cast<T>(Find(Id));
	}
	/** The actor's IAbyssPresenter, or nullptr. */
	IAbyssPresenter* FindPresenter(abyss::EntityId Id) const;
	abyss::EntityKind FindKind(abyss::EntityId Id) const;
	/** Reverse lookup (picking): kNoEntity when the actor is not registered. */
	abyss::EntityId FindId(const AActor* Actor) const;

	/** Every live registered actor (snapshot of the map; safe to unregister inside the callback). */
	void ForEach(TFunctionRef<void(abyss::EntityId, AActor*, abyss::EntityKind)> Visitor) const;
	int32 Num() const { return Entries.Num(); }

	// USubsystem
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FEntry
	{
		TWeakObjectPtr<AActor> Actor;
		abyss::EntityKind Kind = abyss::EntityKind::None;
	};

	TMap<uint32, FEntry> Entries;
	TMap<TObjectKey<AActor>, uint32> IdsByActor;
};
