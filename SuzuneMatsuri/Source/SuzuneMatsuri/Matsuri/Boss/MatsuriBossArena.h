// Suzune's Night Festival - 本殿・月下: the boss courtyard, as data (layout) and as actors (build).

#pragma once

#include "CoreMinimal.h"

class UWorld;
struct FMatsuriFloorTheme;

enum class EMatsuriArenaPiece : uint8
{
	Wall,		// the courtyard's outer fence: bounds the fight
	Platform,	// stone platforms and perches: stand on them (stairs or the grapple)
	Cover,		// stone lanterns, statues, the hall: block every bullet
	Fence		// 玉垣: blocks bullets until it has taken enough of them, then grows back
};

/** One solid box. Bullets die inside it; the player cannot pass it. */
struct FMatsuriArenaBox
{
	FVector Center = FVector::ZeroVector;
	FVector Extent = FVector(50.0f);	// half size, cm
	EMatsuriArenaPiece Piece = EMatsuriArenaPiece::Cover;
	FName Name;

	FBox Bounds() const { return FBox(Center - Extent, Center + Extent); }
	float Top() const { return Center.Z + Extent.Z; }
};

/** A named spot the boss can rain on, and that the player can be "at". */
struct FMatsuriArenaZone
{
	FName Name;
	FVector Point = FVector::ZeroVector;	// the ground (or the top, for perches) at its middle
	float Radius = 500.0f;					// how far round it counts as being there
	bool bHigh = false;						// on a platform or perch
};

struct FMatsuriBossArena
{
	FVector Center = FVector::ZeroVector;
	float HalfExtent = 3000.0f;
	float GroundZ = 0.0f;
	TArray<FMatsuriArenaBox> Solids;
	TArray<FMatsuriArenaZone> Zones;
	TArray<FVector> HookPoints;		// the grapple pulls the player to these
	TArray<FVector> AmmoSpots;
	TArray<FVector> Anchors;		// where she hovers
	FVector PlayerStart = FVector::ZeroVector;
	FRotator PlayerStartRotation = FRotator::ZeroRotator;
};

namespace Matsuri::BossArena
{
	/** The courtyard (fixed: the fight is learnable because the place stays the same). */
	SUZUNEMATSURI_API FMatsuriBossArena Layout();

	/** Index of the zone Point is in (nearest within its radius), or -1 in the open. */
	SUZUNEMATSURI_API int32 ZoneAt(const FMatsuriBossArena& Arena, const FVector& Point);

	/** True when the segment From -> To crosses a solid that is not a fence (fences break). */
	SUZUNEMATSURI_API bool IsShielded(const FMatsuriBossArena& Arena, const FVector& From, const FVector& To);

	/** Spawns ground, solids, fences, hook points, ammo talismans and lighting into World. */
	SUZUNEMATSURI_API void Build(UWorld& World, const FMatsuriBossArena& Arena, const FMatsuriFloorTheme& Theme);
}
