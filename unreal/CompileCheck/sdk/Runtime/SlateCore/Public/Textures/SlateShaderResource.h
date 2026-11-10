// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Textures/SlateShaderResource.h (UE 5.x source knowledge).
// FSlateResourceHandle: what FSlateBrush::GetRenderingResource() / FSlateApplication::GetRenderer()->GetResourceHandle
// return, and what MakeCustomVerts takes.
#pragma once

#include "CoreMinimal.h"

class FSlateShaderResourceProxy;
namespace ESlateShaderResource { enum Type { NativeTexture, TextureObject, Material, PostProcess, Invalid }; }

class FSlateShaderResource
{
public:
	virtual uint32 GetWidth() const = 0;
	virtual uint32 GetHeight() const = 0;
	virtual ESlateShaderResource::Type GetType() const = 0;
	virtual ~FSlateShaderResource() {}
};
class FSlateResourceHandle
{
public:
	FSlateResourceHandle();
	bool IsValid() const;
	const FSlateShaderResourceProxy* GetResourceProxy() const;
};
