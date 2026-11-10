// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/OutputDevice.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Logging/LogVerbosity.h"
#include "Misc/VarArgs.h"
#include "UObject/NameTypes.h"

namespace ELogTimes { enum Type { None, UTC, SinceGStartTime, Local, Timecode }; }

class FOutputDevice
{
public:
	FOutputDevice();
	FOutputDevice(FOutputDevice&&) = default;
	FOutputDevice(const FOutputDevice&) = default;
	FOutputDevice& operator=(FOutputDevice&&) = default;
	FOutputDevice& operator=(const FOutputDevice&) = default;
	virtual ~FOutputDevice() = default;
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) = 0;
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, const double Time);
	virtual void Flush();
	virtual void TearDown();
	void SetSuppressEventTag(bool bInSuppressEventTag);
	bool GetSuppressEventTag() const;
	void SetAutoEmitLineTerminator(bool bInAutoEmitLineTerminator);
	bool GetAutoEmitLineTerminator() const;
	virtual void Dump(class FArchive& Ar);
	virtual bool IsMemoryOnly() const;
	virtual bool CanBeUsedOnAnyThread() const;
	virtual bool CanBeUsedOnMultipleThreads() const;
	virtual bool CanBeUsedOnPanicThread() const;
	void Log(const TCHAR* S);
	void Log(ELogVerbosity::Type Verbosity, const TCHAR* S);
	void Log(const FName& Category, ELogVerbosity::Type Verbosity, const TCHAR* Str);
	void Log(const FString& S);
	void Log(const FText& S);
	void Log(ELogVerbosity::Type Verbosity, const FString& S);
	void Log(const FName& Category, ELogVerbosity::Type Verbosity, const FString& S);
	template <typename... Types>
	void Logf(UE::Core::TCheckedFormatString<TCHAR, Types...> Fmt, Types... Args) UE_STUB_BODY
	template <typename... Types>
	void Logf(ELogVerbosity::Type Verbosity, UE::Core::TCheckedFormatString<TCHAR, Types...> Fmt, Types... Args) UE_STUB_BODY
	template <typename... Types>
	void CategorizedLogf(const FName& Category, ELogVerbosity::Type Verbosity, UE::Core::TCheckedFormatString<TCHAR, Types...> Fmt, Types... Args) UE_STUB_BODY
protected:
	bool bSuppressEventTag;
	bool bAutoEmitLineTerminator;
};
