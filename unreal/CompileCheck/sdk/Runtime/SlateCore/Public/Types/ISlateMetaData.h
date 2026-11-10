// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Types/ISlateMetaData.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"

class ISlateMetaData
{
public:
	template <class TType> bool IsOfType() const UE_STUB_BODY
	virtual ~ISlateMetaData() {}
protected:
	virtual bool IsOfTypeImpl(const FName& Type) const { return false; }
};
class FTagMetaData : public ISlateMetaData
{
public:
	FTagMetaData(FName InTag) : Tag(InTag) {}
	FName Tag;
};
