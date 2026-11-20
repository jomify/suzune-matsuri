// Suzune's Night Festival - placing pieces of the Mountain Temple pack: on a point, to a height, into a box.

#pragma once

#include "CoreMinimal.h"

class UWorld;
class AStaticMeshActor;
class UNiagaraComponent;
class UInstancedStaticMeshComponent;

/**
 *  Pack meshes have their own pivots and sizes; these helpers place them by their bounds instead, so a
 *  lantern stands on the ground and a wall fills the fence it dresses. Everything placed is visual only:
 *  no collision (gameplay keeps its own boxes) and nothing happens when the pack is not in the project.
 */
namespace Matsuri::ShrineProps
{
	/** The mesh's size in cm (zero when missing). */
	SUZUNEMATSURI_API FVector Size(const TCHAR* Name);

	/** World bounds of Name placed with Place(Base, Yaw, Scale); false when missing. */
	SUZUNEMATSURI_API bool Bounds(const TCHAR* Name, const FVector& Base, float Yaw, const FVector& Scale, FBox& OutBounds);

	/** Places Name so the bottom-centre of its bounds lands on Base. Nullptr when the pack is missing. */
	SUZUNEMATSURI_API AStaticMeshActor* Place(UWorld& World, const TCHAR* Name, const FVector& Base, float Yaw, const FVector& Scale, bool bCastShadow = true);

	/** Uniform scale that makes Name Height cm tall. */
	SUZUNEMATSURI_API float ScaleToHeight(const TCHAR* Name, float Height);

	/** Per-axis scale (and a 0 / 90 degree yaw, long side along the box's) that makes Name fill BoxSize. */
	SUZUNEMATSURI_API FVector FitScale(const TCHAR* Name, const FVector& BoxSize, float& OutYaw);

	/** A looping pack effect (falling leaves, fireflies) at At. Nullptr when missing. */
	SUZUNEMATSURI_API UNiagaraComponent* Effect(UWorld& World, const TCHAR* Name, const FVector& At, float Scale = 1.0f);

	/**
	 *  The temple hall of the pack's own demo map (columns, beams, brackets, walls, two-tier roof, lamps),
	 *  rebuilt piece by piece. Origin is the middle of its floor; at Yaw 0 its front faces -X. About 19 m
	 *  long, 7.7 m deep and 12 m tall. Returns the actor holding it (nullptr without the pack).
	 */
	SUZUNEMATSURI_API AActor* PlaceTempleHall(UWorld& World, const FVector& Origin, float Yaw);

	/** Many copies of one small piece (fallen leaves, grass) as a single instanced component; no shadows. */
	SUZUNEMATSURI_API UInstancedStaticMeshComponent* Scatter(UWorld& World, const TCHAR* Name, const TArray<FTransform>& Instances);
}
