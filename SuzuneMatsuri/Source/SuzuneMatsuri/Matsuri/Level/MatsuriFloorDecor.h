// Suzune's Night Festival - set dressing built from engine shapes: torii, lanterns, lamps, bamboo, stalls.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriFloors.h"
#include "MatsuriGraybox.h"

class UWorld;

namespace Matsuri::Decor
{
	/**
	 *  Dresses a built floor: a torii framing the start and one at the far end, festival lantern
	 *  strings with real light along the shrine fence, lit lamps on stone lanterns, fence caps,
	 *  plus a bamboo grove behind the fence (1F) or striped awnings over the stalls (2F).
	 *  Purely visual except the torii posts, which collide like any pillar.
	 */
	SUZUNEMATSURI_API void DressFloor(UWorld& World, const FMatsuriFloorLayout& Layout, const FMatsuriFloorSpec& Spec,
		const FMatsuriFloorTheme& Theme, int32 FloorIndex);

	/** A Tripo mesh as set dressing (no collision). False when the art is missing. */
	SUZUNEMATSURI_API bool PlaceArt(UWorld& World, const TCHAR* Name, const FVector& Location, const FRotator& Rotation, float Scale);

	/** A chochin (Tripo or gray-box), with a real light when LightIntensity > 0. */
	SUZUNEMATSURI_API void PlaceLantern(UWorld& World, const FVector& Center, const FLinearColor& Color, float LightIntensity);

	/** A gabled tiled roof (two slopes and a ridge, no collision) along the top of a wall Thick cm thick. */
	SUZUNEMATSURI_API void PlaceWallRoof(UWorld& World, const FVector2D& Center, float Length, float Thick, float WallTop, bool bAlongX);

	/** A box trim (no collision), e.g. a stone skirt or a beam band on a wall: textured, or a flat colour without Surface. */
	SUZUNEMATSURI_API void PlaceTrim(UWorld& World, const FVector& Center, const FVector& Size, TOptional<Matsuri::Graybox::ESurface> Surface,
		const FLinearColor& Tint);
}
