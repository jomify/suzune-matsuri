// Suzune's Night Festival - turns a floor layout into actors in the world.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriFloors.h"

class UWorld;

namespace Matsuri::Builder
{
	/** Sun, sky atmosphere, real-time sky light and height fog in the floor's mood. */
	SUZUNEMATSURI_API void SpawnEnvironment(UWorld& World, const FMatsuriFloorTheme& Theme);

	/** Ground, walls and obstacles as colliding gray-box cubes. */
	SUZUNEMATSURI_API void SpawnBlocks(UWorld& World, const FMatsuriFloorLayout& Layout, const FMatsuriFloorTheme& Theme);

	SUZUNEMATSURI_API void SpawnCollectibles(UWorld& World, const FMatsuriFloorLayout& Layout);

	/** Ammo spots always; yokai and hazards only when bThreats (tests switch them off). */
	SUZUNEMATSURI_API void SpawnSpawns(UWorld& World, const FMatsuriFloorLayout& Layout, float ArenaHalfExtent, bool bThreats);
}
