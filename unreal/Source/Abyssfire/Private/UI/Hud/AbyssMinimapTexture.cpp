#include "UI/Hud/AbyssMinimapTexture.h"

#include "Engine/Texture2D.h"
#include "RHI.h"
#include "TextureResource.h"

#include <string_view>

#include "abyss/data/DataStore.h"
#include "abyss/world/Exploration.h"
#include "abyss/world/Grid.h"

#include "Abyssfire.h"
#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssText.h"
#include "UI/AbyssUiSubsystem.h"
#include "UI/Hud/AbyssMinimapRules.h"
#include "World/AbyssThemeRules.h"

FAbyssMinimapTexture::FAbyssMinimapTexture(UAbyssUiSubsystem& InOwner)
	: Owner(&InOwner)
{
}

FAbyssMinimapTexture::~FAbyssMinimapTexture()
{
	Reset();
}

FColor FAbyssMinimapTexture::TileColor(uint8 TileType) const
{
	const uint32 Rgb = TileType < AbyssMinimapRules::TileKinds ? TileColors[TileType] : UnknownTileColor;
	return FColor(static_cast<uint8>((Rgb >> 16) & 0xff), static_cast<uint8>((Rgb >> 8) & 0xff), static_cast<uint8>(Rgb & 0xff));
}

void FAbyssMinimapTexture::ResolvePalette(const std::string& InMapId)
{
	// The zone's presentation theme picks the terrain style the colours come from (AbyssMinimapRules): the march and
	// the Pit are drawn in their own ground colours, a zone without a theme in the web minimap's.
	const UAbyssUiSubsystem* Subsystem = Owner.Get();
	const UAbyssGameInstance* GameInstance = Subsystem != nullptr ? Cast<UAbyssGameInstance>(Subsystem->GetGameInstance()) : nullptr;
	const abyss::DataStore* Data = GameInstance != nullptr ? GameInstance->GetData() : nullptr;
	const std::string_view Theme = Data != nullptr ? AbyssThemeRules::ThemeOf(Data->FindMap(InMapId)) : std::string_view();
	const AbyssMinimapRules::FPalette Palette =
		AbyssMinimapRules::PaletteFor(Data != nullptr ? Data->RawTable("terrain_styles.json") : nullptr, Theme);
	for (int32 Index = 0; Index < AbyssMinimapRules::TileKinds; ++Index)
	{
		TileColors[Index] = Palette.Tile[static_cast<size_t>(Index)];
	}
	UnknownTileColor = Palette.Unknown;
	UE_LOG(LogAbyss, Log, TEXT("Minimap palette for %s: %s"), *AbyssText::ToFString(InMapId),
		Palette.Style.empty() ? TEXT("web colours (no presentation theme)") : *AbyssText::ToFString(Palette.Style));
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
		ResolvePalette(MapId);
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
