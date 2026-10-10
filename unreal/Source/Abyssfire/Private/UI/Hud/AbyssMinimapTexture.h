// The zone's minimap texture (save-ui-input 6.11, world-map-nav 11, DECISIONS W2 / W4 / U8): one texel per tile, coloured
// by tile type, with the explored fog of the current visit (unexplored tiles dark). Built once per zone, re-uploaded when
// the explored set grows (checked every 250 ms, the web refresh). Shared by the HUD minimap and the world-map panel.
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

#include <string>

#include "abyss/sim/Snapshot.h"

class UAbyssUiSubsystem;
class UTexture2D;

class FAbyssMinimapTexture
{
public:
	explicit FAbyssMinimapTexture(UAbyssUiSubsystem& InOwner);
	~FAbyssMinimapTexture();

	/** Rebuilds for a new zone or refreshes the fog (cheap when nothing changed). */
	void Update(const abyss::Snapshot& Snap, double Now);
	void Reset();

	UTexture2D* GetTexture() const;
	int32 GetCols() const { return Cols; }
	int32 GetRows() const { return Rows; }
	const std::string& GetMapId() const { return MapId; }
	/** Bumped on every upload (brushes / caches keyed on it). */
	uint32 GetRevision() const { return Revision; }
	bool IsExplored(int32 Col, int32 Row) const;

	/** Tile colour of the web minimap (layer 1). */
	static FColor TileColor(uint8 TileType);

private:
	void Upload(bool bCreate);

	TWeakObjectPtr<UAbyssUiSubsystem> Owner;
	TWeakObjectPtr<UTexture2D> Texture;
	std::string MapId;
	TArray<uint8> Tiles;
	TArray<uint8> Explored;
	TArray<FColor> Pixels;
	int32 Cols = 0;
	int32 Rows = 0;
	int32 ExploredCount = -1;
	double NextCheck = 0.0;
	uint32 Revision = 0;
};
