// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Templates/SharedPointer.h (+ SharedPointerInternals.h).
// From UE 5.x source knowledge. The conversion rules are the engine's:
//   * TSharedRef is never null: no default constructor, no construction from nullptr or from a TSharedPtr (use
//     ToSharedRef()), and it converts implicitly to TSharedPtr / TWeakPtr;
//   * derived -> base conversions are implicit, base -> derived needs StaticCastSharedPtr / StaticCastSharedRef;
//   * MakeShareable(new T) returns a proxy both pointer types accept; MakeShared<T>(Args...) constructs in place;
//   * TSharedFromThis<T>::AsShared() / SharedThis(this) need the object to be owned by a shared pointer already.
#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointerFwd.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/TypeHash.h"

namespace SharedPointerInternals
{
	struct FStaticCastTag {};
	struct FConstCastTag {};
	struct FNullTag {};
	template <class ObjectType>
	struct TRawPtrProxy
	{
		ObjectType* Object;
		TRawPtrProxy(TYPE_OF_NULLPTR) : Object(nullptr) {}
		explicit TRawPtrProxy(ObjectType* InObject) : Object(InObject) {}
	};
}

template <class ObjectType, ESPMode InMode>
class TSharedRef
{
public:
	using ElementType = ObjectType;
	static constexpr ESPMode Mode = InMode;

	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	explicit TSharedRef(OtherType* InObject) : Object(InObject) {}
	template <class OtherType, class DeleterType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TSharedRef(OtherType* InObject, DeleterType&& InDeleter) : Object(InObject) {}
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TSharedRef(const SharedPointerInternals::TRawPtrProxy<OtherType>& InRawPtrProxy) : Object(InRawPtrProxy.Object) {}
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TSharedRef(const TSharedRef<OtherType, InMode>& InSharedRef) : Object(InSharedRef.Object) {}
	template <class OtherType>
	TSharedRef(const TSharedRef<OtherType, InMode>& InSharedRef, SharedPointerInternals::FStaticCastTag)
		: Object(static_cast<ObjectType*>(InSharedRef.Object)) {}
	template <class OtherType>
	TSharedRef(const TSharedRef<OtherType, InMode>& InSharedRef, SharedPointerInternals::FConstCastTag)
		: Object(const_cast<ObjectType*>(InSharedRef.Object)) {}
	TSharedRef(const TSharedRef& InSharedRef) = default;
	TSharedRef(TSharedRef&& InSharedRef) = default;
	TSharedRef& operator=(const TSharedRef& InSharedRef) = default;
	TSharedRef& operator=(TSharedRef&& InSharedRef) = default;
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TSharedRef& operator=(const SharedPointerInternals::TRawPtrProxy<OtherType>& InRawPtrProxy) { Object = InRawPtrProxy.Object; return *this; }

	TSharedPtr<ObjectType, InMode> ToSharedPtr() const;
	TWeakPtr<ObjectType, InMode> ToWeakPtr() const;
	ObjectType& Get() const { return *Object; }
	ObjectType& operator*() const { return *Object; }
	ObjectType* operator->() const { return Object; }
	int32 GetSharedReferenceCount() const UE_STUB_BODY
	bool IsUnique() const UE_STUB_BODY

private:
	template <class OtherType, ESPMode OtherMode> friend class TSharedRef;
	template <class OtherType, ESPMode OtherMode> friend class TSharedPtr;
	template <class OtherType, ESPMode OtherMode> friend class TWeakPtr;
	ObjectType* Object;
};

template <class ObjectType, ESPMode InMode>
class TSharedPtr
{
public:
	using ElementType = ObjectType;
	static constexpr ESPMode Mode = InMode;

	TSharedPtr(SharedPointerInternals::FNullTag* = nullptr) : Object(nullptr) {}
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	explicit TSharedPtr(OtherType* InObject) : Object(InObject) {}
	template <class OtherType, class DeleterType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TSharedPtr(OtherType* InObject, DeleterType&& InDeleter) : Object(InObject) {}
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TSharedPtr(const SharedPointerInternals::TRawPtrProxy<OtherType>& InRawPtrProxy) : Object(InRawPtrProxy.Object) {}
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TSharedPtr(const TSharedPtr<OtherType, InMode>& InSharedPtr) : Object(InSharedPtr.Object) {}
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TSharedPtr(TSharedPtr<OtherType, InMode>&& InSharedPtr) : Object(InSharedPtr.Object) {}
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TSharedPtr(const TSharedRef<OtherType, InMode>& InSharedRef) : Object(InSharedRef.Object) {}
	template <class OtherType>
	TSharedPtr(const TSharedPtr<OtherType, InMode>& InSharedPtr, SharedPointerInternals::FStaticCastTag)
		: Object(static_cast<ObjectType*>(InSharedPtr.Object)) {}
	template <class OtherType>
	TSharedPtr(const TSharedPtr<OtherType, InMode>& InSharedPtr, SharedPointerInternals::FConstCastTag)
		: Object(const_cast<ObjectType*>(InSharedPtr.Object)) {}
	template <class OtherType>
	TSharedPtr(const TSharedPtr<OtherType, InMode>& OtherSharedPtr, ObjectType* InObject) : Object(InObject) {}
	TSharedPtr(const TSharedPtr& InSharedPtr) = default;
	TSharedPtr(TSharedPtr&& InSharedPtr) = default;
	TSharedPtr& operator=(SharedPointerInternals::FNullTag*) { Object = nullptr; return *this; }
	TSharedPtr& operator=(const TSharedPtr& InSharedPtr) = default;
	TSharedPtr& operator=(TSharedPtr&& InSharedPtr) = default;
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TSharedPtr& operator=(const SharedPointerInternals::TRawPtrProxy<OtherType>& InRawPtrProxy) { Object = InRawPtrProxy.Object; return *this; }

	TSharedRef<ObjectType, InMode> ToSharedRef() const& { return TSharedRef<ObjectType, InMode>(Object); }
	TSharedRef<ObjectType, InMode> ToSharedRef() && { return TSharedRef<ObjectType, InMode>(Object); }
	TWeakPtr<ObjectType, InMode> ToWeakPtr() const;
	ObjectType* Get() const { return Object; }
	explicit operator bool() const { return Object != nullptr; }
	bool IsValid() const { return Object != nullptr; }
	decltype(auto) operator*() const requires (!std::is_void_v<ObjectType>) { return *Object; }
	ObjectType* operator->() const { return Object; }
	void Reset() { Object = nullptr; }
	int32 GetSharedReferenceCount() const UE_STUB_BODY
	bool IsUnique() const UE_STUB_BODY

private:
	template <class OtherType, ESPMode OtherMode> friend class TSharedRef;
	template <class OtherType, ESPMode OtherMode> friend class TSharedPtr;
	template <class OtherType, ESPMode OtherMode> friend class TWeakPtr;
	template <class OtherType, ESPMode OtherMode> friend class TSharedFromThis;
	ObjectType* Object;
};

template <class ObjectType, ESPMode InMode>
TSharedPtr<ObjectType, InMode> TSharedRef<ObjectType, InMode>::ToSharedPtr() const { return TSharedPtr<ObjectType, InMode>(*this); }

template <class ObjectType, ESPMode InMode>
class TWeakPtr
{
public:
	using ElementType = ObjectType;
	static constexpr ESPMode Mode = InMode;

	TWeakPtr(SharedPointerInternals::FNullTag* = nullptr) : Object(nullptr) {}
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TWeakPtr(const TSharedRef<OtherType, InMode>& InSharedRef) : Object(InSharedRef.Object) {}
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TWeakPtr(const TSharedPtr<OtherType, InMode>& InSharedPtr) : Object(InSharedPtr.Object) {}
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TWeakPtr(const TWeakPtr<OtherType, InMode>& InWeakPtr) : Object(InWeakPtr.Object) {}
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TWeakPtr(TWeakPtr<OtherType, InMode>&& InWeakPtr) : Object(InWeakPtr.Object) {}
	TWeakPtr(const TWeakPtr& InWeakPtr) = default;
	TWeakPtr(TWeakPtr&& InWeakPtr) = default;
	TWeakPtr& operator=(SharedPointerInternals::FNullTag*) { Object = nullptr; return *this; }
	TWeakPtr& operator=(const TWeakPtr& InWeakPtr) = default;
	TWeakPtr& operator=(TWeakPtr&& InWeakPtr) = default;
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TWeakPtr& operator=(const TSharedRef<OtherType, InMode>& InSharedRef) { Object = InSharedRef.Object; return *this; }
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TWeakPtr& operator=(const TSharedPtr<OtherType, InMode>& InSharedPtr) { Object = InSharedPtr.Object; return *this; }
	template <class OtherType> requires std::is_convertible_v<OtherType*, ObjectType*>
	TWeakPtr& operator=(const TWeakPtr<OtherType, InMode>& InWeakPtr) { Object = InWeakPtr.Object; return *this; }

	[[nodiscard]] TSharedPtr<ObjectType, InMode> Pin() const& UE_STUB_BODY
	[[nodiscard]] TSharedPtr<ObjectType, InMode> Pin() && UE_STUB_BODY
	[[nodiscard]] bool IsValid() const UE_STUB_BODY
	void Reset() { Object = nullptr; }
	bool HasSameObject(const void* InOtherPtr) const UE_STUB_BODY
	uint32 GetWeakPtrTypeHash() const UE_STUB_BODY

private:
	template <class OtherType, ESPMode OtherMode> friend class TWeakPtr;
	template <class OtherType, ESPMode OtherMode> friend class TSharedPtr;
	ObjectType* Object;
};

template <class ObjectType, ESPMode InMode>
TWeakPtr<ObjectType, InMode> TSharedRef<ObjectType, InMode>::ToWeakPtr() const { return TWeakPtr<ObjectType, InMode>(*this); }
template <class ObjectType, ESPMode InMode>
TWeakPtr<ObjectType, InMode> TSharedPtr<ObjectType, InMode>::ToWeakPtr() const { return TWeakPtr<ObjectType, InMode>(*this); }

template <class ObjectType, ESPMode Mode>
class TSharedFromThis
{
public:
	[[nodiscard]] TSharedRef<ObjectType, Mode> AsShared() UE_STUB_BODY
	[[nodiscard]] TSharedRef<ObjectType const, Mode> AsShared() const UE_STUB_BODY
	template <typename SubobjectType> [[nodiscard]] TSharedRef<SubobjectType, Mode> AsSharedSubobject(SubobjectType* SubobjectPtr) const UE_STUB_BODY
	[[nodiscard]] TWeakPtr<ObjectType, Mode> AsWeak() UE_STUB_BODY
	[[nodiscard]] TWeakPtr<ObjectType const, Mode> AsWeak() const UE_STUB_BODY
	[[nodiscard]] bool DoesSharedInstanceExist() const UE_STUB_BODY

protected:
	template <class OtherType>
	[[nodiscard]] static TSharedRef<OtherType, Mode> SharedThis(OtherType* ThisPtr)
	{
		return TSharedRef<OtherType, Mode>(ThisPtr->AsShared(), SharedPointerInternals::FStaticCastTag());
	}
	template <class OtherType>
	[[nodiscard]] static TSharedRef<OtherType const, Mode> SharedThis(const OtherType* ThisPtr)
	{
		return TSharedRef<OtherType const, Mode>(ThisPtr->AsShared(), SharedPointerInternals::FStaticCastTag());
	}
	TSharedFromThis() {}
	TSharedFromThis(TSharedFromThis const&) {}
	TSharedFromThis& operator=(TSharedFromThis const&) { return *this; }
	~TSharedFromThis() {}
};

// ---- comparisons ----
template <class A, class B, ESPMode Mode> bool operator==(const TSharedRef<A, Mode>& L, const TSharedRef<B, Mode>& R) { return &L.Get() == &R.Get(); }
template <class A, class B, ESPMode Mode> bool operator==(const TSharedPtr<A, Mode>& L, const TSharedPtr<B, Mode>& R) { return L.Get() == R.Get(); }
template <class A, class B, ESPMode Mode> bool operator==(const TSharedRef<A, Mode>& L, const TSharedPtr<B, Mode>& R) { return &L.Get() == R.Get(); }
template <class A, class B, ESPMode Mode> bool operator==(const TSharedPtr<A, Mode>& L, const TSharedRef<B, Mode>& R) { return L.Get() == &R.Get(); }
template <class A, ESPMode Mode> bool operator==(const TSharedPtr<A, Mode>& L, TYPE_OF_NULLPTR) { return !L.IsValid(); }
template <class A, ESPMode Mode> bool operator==(TYPE_OF_NULLPTR, const TSharedPtr<A, Mode>& R) { return !R.IsValid(); }
template <class A, class B, ESPMode Mode> bool operator==(const TWeakPtr<A, Mode>& L, const TWeakPtr<B, Mode>& R) UE_STUB_BODY
template <class A, class B, ESPMode Mode> bool operator==(const TWeakPtr<A, Mode>& L, const TSharedRef<B, Mode>& R) UE_STUB_BODY
template <class A, class B, ESPMode Mode> bool operator==(const TWeakPtr<A, Mode>& L, const TSharedPtr<B, Mode>& R) UE_STUB_BODY
template <class A, class B, ESPMode Mode> bool operator==(const TSharedRef<A, Mode>& L, const TWeakPtr<B, Mode>& R) UE_STUB_BODY
template <class A, class B, ESPMode Mode> bool operator==(const TSharedPtr<A, Mode>& L, const TWeakPtr<B, Mode>& R) UE_STUB_BODY
template <class A, ESPMode Mode> bool operator==(const TWeakPtr<A, Mode>& L, TYPE_OF_NULLPTR) UE_STUB_BODY
template <class A, ESPMode Mode> bool operator==(TYPE_OF_NULLPTR, const TWeakPtr<A, Mode>& R) UE_STUB_BODY
template <class A, class B, ESPMode Mode> bool operator<(const TSharedPtr<A, Mode>& L, const TSharedPtr<B, Mode>& R) UE_STUB_BODY

// ---- casts and factories ----
template <class CastToType, class CastFromType, ESPMode Mode>
[[nodiscard]] TSharedRef<CastToType, Mode> StaticCastSharedRef(TSharedRef<CastFromType, Mode> const& InSharedRef)
{
	return TSharedRef<CastToType, Mode>(InSharedRef, SharedPointerInternals::FStaticCastTag());
}
template <class CastToType, class CastFromType, ESPMode Mode>
[[nodiscard]] TSharedPtr<CastToType, Mode> StaticCastSharedPtr(TSharedPtr<CastFromType, Mode> const& InSharedPtr)
{
	return TSharedPtr<CastToType, Mode>(InSharedPtr, SharedPointerInternals::FStaticCastTag());
}
template <class CastToType, class CastFromType, ESPMode Mode>
[[nodiscard]] TWeakPtr<CastToType, Mode> StaticCastWeakPtr(TWeakPtr<CastFromType, Mode> const& InWeakPtr) UE_STUB_BODY
template <class CastToType, class CastFromType, ESPMode Mode>
[[nodiscard]] TSharedRef<CastToType, Mode> ConstCastSharedRef(TSharedRef<CastFromType, Mode> const& InSharedRef)
{
	return TSharedRef<CastToType, Mode>(InSharedRef, SharedPointerInternals::FConstCastTag());
}
template <class CastToType, class CastFromType, ESPMode Mode>
[[nodiscard]] TSharedPtr<CastToType, Mode> ConstCastSharedPtr(TSharedPtr<CastFromType, Mode> const& InSharedPtr)
{
	return TSharedPtr<CastToType, Mode>(InSharedPtr, SharedPointerInternals::FConstCastTag());
}
template <class ObjectType>
[[nodiscard]] SharedPointerInternals::TRawPtrProxy<ObjectType> MakeShareable(ObjectType* InObject)
{
	return SharedPointerInternals::TRawPtrProxy<ObjectType>(InObject);
}
// MakeShareable(Object, Deleter) and its TRawPtrProxyWithDeleter are not declared: unused by the module, and TSharedPtr /
// TSharedRef here have no constructor from that proxy.
template <typename InObjectType, ESPMode InMode = ESPMode::ThreadSafe, typename... InArgTypes>
[[nodiscard]] TSharedRef<InObjectType, InMode> MakeShared(InArgTypes&&... Args)
{
	return TSharedRef<InObjectType, InMode>(new InObjectType(static_cast<InArgTypes&&>(Args)...));
}

template <typename ObjectType, ESPMode Mode> uint32 GetTypeHash(const TSharedRef<ObjectType, Mode>& InSharedRef) UE_STUB_BODY
template <typename ObjectType, ESPMode Mode> uint32 GetTypeHash(const TSharedPtr<ObjectType, Mode>& InSharedPtr) UE_STUB_BODY
template <typename ObjectType, ESPMode Mode> uint32 GetTypeHash(const TWeakPtr<ObjectType, Mode>& InWeakPtr) UE_STUB_BODY
template <class T, ESPMode Mode> struct TIsWeakPointerType<TWeakPtr<T, Mode>> { enum { Value = true }; };
