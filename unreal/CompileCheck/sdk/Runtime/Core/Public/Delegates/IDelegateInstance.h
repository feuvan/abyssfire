// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Delegates/IDelegateInstance.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Templates/TypeHash.h"

class FDelegateHandle
{
public:
	enum EGenerateNewHandleType { GenerateNewHandle };
	FDelegateHandle() : ID(0) {}
	explicit FDelegateHandle(EGenerateNewHandleType);
	bool IsValid() const { return ID != 0; }
	void Reset() { ID = 0; }
	bool operator==(const FDelegateHandle& Rhs) const { return ID == Rhs.ID; }
	bool operator!=(const FDelegateHandle& Rhs) const { return ID != Rhs.ID; }
	friend uint32 GetTypeHash(const FDelegateHandle& Key);
private:
	uint64 ID;
};
