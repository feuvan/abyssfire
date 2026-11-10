// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/EngineUtils.h (UE 5.x source knowledge).
// TActorIterator<T>(World) / TActorRange<T>(World): iterate the actors of a class.
#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

enum class EActorIteratorFlags
{
	AllActors = 0x00000000,
	SkipPendingKill = 0x00000001,
	OnlySelectedActors = 0x00000002,
	OnlyActiveLevels = 0x00000004,
};
ENUM_CLASS_FLAGS(EActorIteratorFlags);

template <typename ActorType>
class TActorIterator
{
public:
	explicit TActorIterator(const UWorld* InWorld, TSubclassOf<ActorType> InClass = ActorType::StaticClass(),
		EActorIteratorFlags InFlags = EActorIteratorFlags::OnlyActiveLevels | EActorIteratorFlags::SkipPendingKill)
	{
		static_assert(std::is_base_of_v<AActor, ActorType>, "TActorIterator<T>: T must derive from AActor");
	}
	void operator++() UE_STUB_BODY
	ActorType* operator*() const UE_STUB_BODY
	ActorType* operator->() const UE_STUB_BODY
	explicit operator bool() const UE_STUB_BODY
	bool operator!() const UE_STUB_BODY
	TActorIterator begin() const UE_STUB_BODY
	TActorIterator end() const UE_STUB_BODY
	bool operator!=(const TActorIterator& Rhs) const UE_STUB_BODY
};
template <typename ActorType>
class TActorRange
{
public:
	TActorRange(const UWorld* InWorld, TSubclassOf<ActorType> InClass = ActorType::StaticClass(),
		EActorIteratorFlags InFlags = EActorIteratorFlags::OnlyActiveLevels | EActorIteratorFlags::SkipPendingKill) UE_STUB_BODY
	struct FIt { ActorType* operator*() const UE_STUB_BODY FIt& operator++() UE_STUB_BODY bool operator!=(const FIt&) const UE_STUB_BODY };
	FIt begin() const UE_STUB_BODY
	FIt end() const UE_STUB_BODY
};
class FActorIterator
{
public:
	explicit FActorIterator(const UWorld* InWorld, EActorIteratorFlags InFlags = EActorIteratorFlags::OnlyActiveLevels | EActorIteratorFlags::SkipPendingKill);
	void operator++();
	AActor* operator*() const;
	AActor* operator->() const;
	explicit operator bool() const;
};
