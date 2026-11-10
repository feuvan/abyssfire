// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Types/ISlateMetaData.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"

class ISlateMetaData
{
public:
	/** Check if this metadata operation can cast safely to the specified template type. */
	template <class TType> bool IsOfType() const { return IsOfTypeImpl(TType::GetTypeId()); }
	virtual ~ISlateMetaData() {}
protected:
	virtual bool IsOfTypeImpl(const FName& Type) const { return false; }
};

/** All metadata-derived classes must put this macro in their class body: it gives them the GetTypeId() IsOfType needs. */
#define SLATE_METADATA_TYPE(TYPE, BASE) \
	static const FName& GetTypeId() { static FName Type(TEXT(#TYPE)); return Type; } \
	virtual bool IsOfTypeImpl(const FName& Type) const override { return GetTypeId() == Type || BASE::IsOfTypeImpl(Type); }

class FTagMetaData : public ISlateMetaData
{
public:
	SLATE_METADATA_TYPE(FTagMetaData, ISlateMetaData)
	FTagMetaData(FName InTag) : Tag(InTag) {}
	FName Tag;
};
