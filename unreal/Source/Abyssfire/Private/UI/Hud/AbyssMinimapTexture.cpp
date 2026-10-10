#include "UI/Hud/AbyssMinimapTexture.h"

#include "Engine/Texture2D.h"
#include "RHI.h"
#include "TextureResource.h"

#include "abyss/world/Exploration.h"
#include "abyss/world/Grid.h"

#include "UI/AbyssUiSubsystem.h"

FAbyssMinimapTexture::FAbyssMinimapTexture(UAbyssUiSubsystem& InOwner)
	: Owner(&InOwner)
{
}

FAbyssMinimapTexture::~FAbyssMinimapTexture()
{
	Reset();
}

FColor FAbyssMinimapTexture::TileColor(uint8 TileType)
{
	switch (TileType)
	{
	case 0: return FColor(0x4a, 0x8c, 0x3f);   // grass
	case 1: return FColor(0x8b, 0x73, 0x55);   // dirt
	case 2: return FColor(0x6a, 0x6a, 0x6a);   // stone
	case 3: return FColor(0x1a, 0x52, 0x76);   // water
	case 4: return FColor(0x4a, 0x4a, 0x4a);   // wall
	case 5: return FColor(0x9e, 0x7c, 0x52);   // camp
	case 6: return FColor(0x5c, 0x48, 0x34);   // camp wall (palisade)
	default: return FColor(0x22, 0x22, 0x22);
	}
}

void FAbyssMinimapTexture::Reset()
{
	if (UTexture2D* Existing = Texture.Get())
	{
		if (UAbyssUiSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->ReleaseKeptAlive(Existing);
		}
	}
	Texture.Reset();
	MapId.clear();
	Tiles.Reset();
	Explored.Reset();
	Pixels.Reset();
	Cols = Rows = 0;
	ExploredCount = -1;
	NextCheck = 0.0;
}

UTexture2D* FAbyssMinimapTexture::GetTexture() const
{
	return Texture.Get();
}

bool FAbyssMinimapTexture::IsExplored(int32 Col, int32 Row) const
{
	if (Col < 0 || Row < 0 || Col >= Cols || Row >= Rows)
	{
		return false;
	}
	const int32 Index = Row * Cols + Col;
	return Explored.IsValidIndex(Index) && Explored[Index] != 0;
}

void FAbyssMinimapTexture::Update(const abyss::Snapshot& Snap, double Now)
{
	const abyss::ZoneGrid* Grid = Snap.zone.grid;
	if (Grid == nullptr || Grid->Cols() <= 0 || Grid->Rows() <= 0)
	{
		return;
	}
	const bool bNewZone = Snap.zone.mapId != MapId || Grid->Cols() != Cols || Grid->Rows() != Rows || !Texture.IsValid();
	if (!bNewZone && Now < NextCheck)
	{
		return;
	}
	NextCheck = Now + 0.25;

	if (bNewZone)
	{
		Reset();
		MapId = Snap.zone.mapId;
		Cols = Grid->Cols();
		Rows = Grid->Rows();
		Tiles.SetNumUninitialized(Cols * Rows);
		const std::vector<abyss::TileType>& Source = Grid->Tiles();
		for (int32 Index = 0; Index < Cols * Rows; ++Index)
		{
			Tiles[Index] = static_cast<size_t>(Index) < Source.size() ? static_cast<uint8>(Source[static_cast<size_t>(Index)]) : 255;
		}
		Explored.Init(0, Cols * Rows);
		Pixels.SetNumUninitialized(Cols * Rows);
	}

	// W2: explored fog of this visit (ExplorationGrid, radius 10 around the hero).
	int32 Count = 0;
	if (const abyss::ExplorationGrid* Exploration = Snap.exploration)
	{
		if (Exploration->Cols() == Cols && Exploration->Rows() == Rows)
		{
			const std::vector<uint8_t>& Bits = Exploration->Bits();
			for (int32 Index = 0; Index < Cols * Rows; ++Index)
			{
				const uint8 Value = static_cast<size_t>(Index) < Bits.size() && Bits[static_cast<size_t>(Index)] != 0 ? 1 : 0;
				Explored[Index] = Value;
				Count += Value;
			}
		}
	}
	if (!bNewZone && Count == ExploredCount)
	{
		return;
	}
	ExploredCount = Count;

	const FColor Fog(7, 6, 10, 235);
	for (int32 Index = 0; Index < Cols * Rows; ++Index)
	{
		if (Explored[Index] != 0)
		{
			FColor Color = TileColor(Tiles[Index]);
			Color.A = 230;
			Pixels[Index] = Color;
		}
		else
		{
			Pixels[Index] = Fog;
		}
	}
	Upload(bNewZone);
}

void FAbyssMinimapTexture::Upload(bool bCreate)
{
	if (Cols <= 0 || Rows <= 0)
	{
		return;
	}
	UTexture2D* Target = Texture.Get();
	if (bCreate || Target == nullptr)
	{
		Target = UTexture2D::CreateTransient(Cols, Rows, PF_B8G8R8A8, FName(TEXT("AbyssMinimap")));
		if (Target == nullptr)
		{
			return;
		}
		Target->Filter = TF_Nearest;
		Target->SRGB = true;
		Target->AddressX = TA_Clamp;
		Target->AddressY = TA_Clamp;
		Target->LODGroup = TEXTUREGROUP_UI;
		// FColor is BGRA in memory, matching PF_B8G8R8A8.
		FTexture2DMipMap& Mip = Target->GetPlatformData()->Mips[0];
		void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, Pixels.GetData(), static_cast<SIZE_T>(Pixels.Num()) * sizeof(FColor));
		Mip.BulkData.Unlock();
		Target->UpdateResource();
		Texture = Target;
		if (UAbyssUiSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->KeepAlive(Target);
		}
		++Revision;
		return;
	}
	// Fog update: copy the pixels for the render thread; the cleanup frees them once uploaded.
	const SIZE_T Bytes = static_cast<SIZE_T>(Pixels.Num()) * sizeof(FColor);
	uint8* Buffer = static_cast<uint8*>(FMemory::Malloc(Bytes));
	FMemory::Memcpy(Buffer, Pixels.GetData(), Bytes);
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, static_cast<uint32>(Cols), static_cast<uint32>(Rows));
	Target->UpdateTextureRegions(0, 1, Region, static_cast<uint32>(Cols * sizeof(FColor)), static_cast<uint32>(sizeof(FColor)), Buffer,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			FMemory::Free(SrcData);
			delete Regions;
		});
	++Revision;
}
