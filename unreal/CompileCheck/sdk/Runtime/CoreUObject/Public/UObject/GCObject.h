// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/GCObject.h (+ UObject/UObjectGlobals.h for
// FReferenceCollector). From UE 5.x source knowledge. FGCObject::GetReferencerName is pure (5.x).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"

class FReferenceCollector
{
public:
	virtual ~FReferenceCollector();
	template <class UObjectType> void AddReferencedObject(TObjectPtr<UObjectType>& Object, const UObject* ReferencingObject = nullptr, const FProperty* ReferencingProperty = nullptr) UE_STUB_BODY
	template <class UObjectType> void AddReferencedObjects(TArray<TObjectPtr<UObjectType>>& ObjectArray, const UObject* ReferencingObject = nullptr, const FProperty* ReferencingProperty = nullptr) UE_STUB_BODY
	template <class UObjectType> void AddReferencedObjects(TSet<TObjectPtr<UObjectType>>& ObjectSet, const UObject* ReferencingObject = nullptr, const FProperty* ReferencingProperty = nullptr) UE_STUB_BODY
	template <typename KeyType, typename ValueType> void AddReferencedObjects(TMap<KeyType, ValueType>& Map, const UObject* ReferencingObject = nullptr, const FProperty* ReferencingProperty = nullptr) UE_STUB_BODY
	template <class UObjectType> void AddStableReference(TObjectPtr<UObjectType>* Object) UE_STUB_BODY
};

class FGCObject
{
public:
	FGCObject();
	FGCObject(const FGCObject& Other);
	FGCObject(FGCObject&& Other);
	virtual ~FGCObject();
	FGCObject& operator=(const FGCObject&) { return *this; }
	FGCObject& operator=(FGCObject&&) { return *this; }
	virtual void AddReferencedObjects(FReferenceCollector& Collector) = 0;
	virtual FString GetReferencerName() const = 0;
	virtual bool GetReferencerPropertyName(UObject* Object, FString& OutPropertyName) const;
};
