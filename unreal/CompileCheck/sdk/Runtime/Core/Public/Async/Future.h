// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Async/Future.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Misc/Timespan.h"
#include "Misc/DateTime.h"
#include "Templates/Function.h"

template <typename ResultType> class TSharedFuture;
namespace UE::Stub
{
	template <typename ResultType, typename Func> requires (!std::is_void_v<ResultType>)
	auto InvokeNext(Func& Continuation) -> decltype(Continuation(std::declval<ResultType>())) UE_STUB_BODY
	template <typename ResultType, typename Func> requires std::is_void_v<ResultType>
	auto InvokeNext(Func& Continuation) -> decltype(Continuation()) UE_STUB_BODY
}
template <typename ResultType>
class TFuture
{
public:
	TFuture() UE_STUB_BODY
	TFuture(TFuture&&) = default;
	TFuture& operator=(TFuture&& Other) = default;
	TFuture(const TFuture&) = delete;
	TFuture& operator=(const TFuture&) = delete;
	~TFuture() UE_STUB_BODY
	bool IsReady() const UE_STUB_BODY
	bool IsValid() const UE_STUB_BODY
	void Wait() const UE_STUB_BODY
	bool WaitFor(const FTimespan& Duration) const UE_STUB_BODY
	bool WaitUntil(const FDateTime& Time) const UE_STUB_BODY
	ResultType Get() const UE_STUB_BODY
	ResultType Consume() UE_STUB_BODY
	TSharedFuture<ResultType> Share() UE_STUB_BODY
	/** Continuation taking this (ready) future; returns a future of the continuation's result. */
	template <typename Func> auto Then(Func Continuation) -> TFuture<decltype(Continuation(std::declval<TFuture<ResultType>>()))> UE_STUB_BODY
	/** Continuation taking the result; returns a future of the continuation's result (the void form takes no argument). */
	template <typename Func> auto Next(Func Continuation) -> TFuture<decltype(UE::Stub::InvokeNext<ResultType>(Continuation))> UE_STUB_BODY
	void Reset() UE_STUB_BODY
};
/** TPromise: the general form, then the engine's two specialisations (a reference result, no result). SetValue takes
 *  exactly one value (copied or moved); only EmplaceValue forwards constructor arguments. Move-only (TPromiseBase is
 *  FNoncopyable; the base class itself is not declared here). */
template <typename ResultType>
class TPromise
{
public:
	TPromise() UE_STUB_BODY
	TPromise(TUniqueFunction<void()>&& CompletionCallback) UE_STUB_BODY
	TPromise(TPromise&& Other) = default;
	TPromise& operator=(TPromise&& Other) = default;
	TPromise(const TPromise&) = delete;
	TPromise& operator=(const TPromise&) = delete;
	~TPromise() UE_STUB_BODY
	TFuture<ResultType> GetFuture() UE_STUB_BODY
	void SetValue(const ResultType& Result) UE_STUB_BODY
	void SetValue(ResultType&& Result) UE_STUB_BODY
	template <typename... ArgTypes> requires std::is_constructible_v<ResultType, ArgTypes&&...>
	void EmplaceValue(ArgTypes&&... Args) UE_STUB_BODY
};
template <typename ResultType>
class TPromise<ResultType&>
{
public:
	TPromise() UE_STUB_BODY
	TPromise(TUniqueFunction<void()>&& CompletionCallback) UE_STUB_BODY
	TPromise(TPromise&& Other) = default;
	TPromise& operator=(TPromise&& Other) = default;
	TPromise(const TPromise&) = delete;
	TPromise& operator=(const TPromise&) = delete;
	~TPromise() UE_STUB_BODY
	TFuture<ResultType&> GetFuture() UE_STUB_BODY
	void SetValue(ResultType& Result) UE_STUB_BODY
	void EmplaceValue(ResultType& Result) UE_STUB_BODY
};
template <>
class TPromise<void>
{
public:
	TPromise();
	TPromise(TUniqueFunction<void()>&& CompletionCallback);
	TPromise(TPromise&& Other) = default;
	TPromise& operator=(TPromise&& Other) = default;
	TPromise(const TPromise&) = delete;
	TPromise& operator=(const TPromise&) = delete;
	~TPromise();
	TFuture<void> GetFuture();
	void SetValue();
	void EmplaceValue();
};
