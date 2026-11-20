// Suzune's Night Festival - square-cell maze: generation and path-following (pure data, unit tested).

#pragma once

#include "CoreMinimal.h"

/**
 *  Size x Size square cells covering [-HalfExtent, HalfExtent]^2 on the floor.
 *  Cell (0, 0) is the -X/-Y corner; X grows east (+X), Y grows north (+Y).
 *  Each cell stores which of its four walls are closed. The outer border is always closed.
 */
struct SUZUNEMATSURI_API FMatsuriMaze
{
	enum EWall : uint8
	{
		East = 1 << 0,	// +X
		North = 1 << 1,	// +Y
		West = 1 << 2,	// -X
		South = 1 << 3	// -Y
	};

	/** How far past a doorway a waypoint sits, as a fraction of the cell size. */
	static constexpr float DoorLead = 0.15f;

	int32 Size = 0;
	float HalfExtent = 0.0f;
	TArray<uint8> Walls;	// closed-wall bits per cell, index = Y * Size + X

	float CellSize() const { return Size > 0 ? 2.0f * HalfExtent / Size : 0.0f; }
	int32 CellCount() const { return Size * Size; }
	bool IsValid() const { return Size > 0 && Walls.Num() == CellCount(); }

	bool InBounds(const FIntPoint& Cell) const { return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Size && Cell.Y < Size; }
	int32 Index(const FIntPoint& Cell) const { return Cell.Y * Size + Cell.X; }
	FIntPoint CellOf(int32 CellIndex) const { return FIntPoint(CellIndex % Size, CellIndex / Size); }

	/** Cell containing a world position (clamped onto the grid). */
	FIntPoint CellAt(const FVector2D& World) const;
	FVector2D CellCenter(const FIntPoint& Cell) const;

	bool HasWall(const FIntPoint& Cell, EWall Wall) const { return (Walls[Index(Cell)] & Wall) != 0; }

	/** Cells reachable in one step (open wall between them). */
	TArray<FIntPoint> OpenNeighbours(const FIntPoint& Cell) const;

	/** Number of open passages between neighbouring cells (a perfect maze has CellCount() - 1). */
	int32 CountPassages() const;

	/** Breadth-first step counts from Target to every cell; INDEX_NONE where unreachable. */
	TArray<int32> StepsTo(const FIntPoint& Target) const;

	/**
	 *  Where to head next to reach Target from From along the corridors: Target itself when both are in
	 *  the same cell, otherwise just through the doorway into the neighbouring cell that is one step closer
	 *  (doorway middle + DoorLead of a cell). Doorways stay clear of crates (cell centres) and lanterns
	 *  (wall junctions), so ground walkers never steer into them.
	 *  OutSteps (optional) receives the remaining path length in cells, INDEX_NONE if unreachable.
	 */
	FVector2D NextWaypoint(const FVector2D& From, const FVector2D& Target, int32* OutSteps = nullptr) const;
};

namespace Matsuri::Maze
{
	/**
	 *  Recursive-backtracker maze (every cell reachable), then extra walls knocked out with LoopChance
	 *  so there are alternative routes, and Plazas 2x2 rooms opened for fights. Same seed, same maze.
	 */
	SUZUNEMATSURI_API FMatsuriMaze Generate(int32 Size, float HalfExtent, int32 Seed, float LoopChance, int32 Plazas);
}
