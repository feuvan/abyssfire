// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Logging/LogCategory.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Logging/LogVerbosity.h"
#include "UObject/NameTypes.h"

using FLogCategoryName = FName;

struct FLogCategoryBase
{
	FLogCategoryBase(const FLogCategoryName& InCategoryName, ELogVerbosity::Type InDefaultVerbosity, ELogVerbosity::Type InCompileTimeVerbosity);
	~FLogCategoryBase();
	bool IsSuppressed(ELogVerbosity::Type VerbosityLevel) const;
	const FLogCategoryName& GetCategoryName() const;
	ELogVerbosity::Type GetVerbosity() const;
	void SetVerbosity(ELogVerbosity::Type Verbosity);
	ELogVerbosity::Type GetCompileTimeVerbosity() const;
};
template <ELogVerbosity::Type InDefaultVerbosity, ELogVerbosity::Type InCompileTimeVerbosity>
struct FLogCategory : public FLogCategoryBase
{
	enum
	{
		CompileTimeVerbosity = InCompileTimeVerbosity
	};
	FLogCategory(const FLogCategoryName& InCategoryName) : FLogCategoryBase(InCategoryName, InDefaultVerbosity, InCompileTimeVerbosity) {}
};
