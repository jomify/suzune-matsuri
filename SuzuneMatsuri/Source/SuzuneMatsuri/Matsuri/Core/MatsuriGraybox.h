// Suzune's Night Festival - gray-box parts built from engine basic shapes (replaced by Tripo meshes in M3).

#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

namespace Matsuri::Graybox
{
	enum class EShape : uint8
	{
		Cube,
		Sphere,
		Cylinder,
		Cone
	};

	SUZUNEMATSURI_API UStaticMesh* Mesh(EShape Shape);

	/**
	 *  Constructor-time helper: creates a non-colliding, shadow-casting mesh part attached to Parent.
	 *  Scale is relative to the 1 m engine shape.
	 */
	SUZUNEMATSURI_API UStaticMeshComponent* MakePart(AActor& Owner, USceneComponent* Parent, FName Name, EShape Shape,
		const FVector& Offset, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator);

	/**
	 *  Runtime helper: tints a part through a dynamic material instance. bGlow uses the emissive
	 *  M_MatsuriGlow (falls back to the plain shape material if that asset is missing).
	 */
	SUZUNEMATSURI_API void Paint(UStaticMeshComponent* Part, const FLinearColor& Color, bool bGlow = false, float GlowStrength = 6.0f);

	enum class ESurface : uint8
	{
		Wood,	// shrine fence boards: walls, crates, painted posts
		Stone,	// granite: stone lanterns, plinths, wall bases
		Paving,	// flagstones: the ground
		Roof,	// roof tiles: the caps on walls
		Plaster	// earthen plaster (土塀): maze and courtyard walls
	};

	/**
	 *  Runtime helper: a tiling texture projected in world space (M_MatsuriSurface), tinted. Falls back to
	 *  Paint(Tint) when the material or texture is missing (matsuri.Art 0, or no art imported). With the
	 *  Mountain Temple pack in the project, wood, stone and roofs take its tileable textures instead.
	 */
	SUZUNEMATSURI_API void PaintSurface(UStaticMeshComponent* Part, ESurface Surface, const FLinearColor& Tint);
}
