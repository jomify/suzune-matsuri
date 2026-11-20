// Suzune's Night Festival - which Tripo mesh dresses which game object, and how it is posed.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriHeroes.h"
#include "MatsuriTypes.h"

/**
 *  Placement of an art mesh relative to the gray-box part it replaces. Tripo models are normalised to
 *  real size in Blender (tools/blender/normalize_matsuri.py); these fix facing and fine scale in game.
 *  Tuned from the art gallery screenshots (SuzuneMatsuri.Art.Gallery).
 */
struct FMatsuriArtPose
{
	const TCHAR* Mesh = TEXT("");
	FRotator Rotation = FRotator::ZeroRotator;
	float Scale = 1.0f;
	FVector Offset = FVector::ZeroVector;
};

namespace Matsuri::ArtTable
{
	FMatsuriArtPose Collectible(EMatsuriCollectible Kind);
	FMatsuriArtPose AmmoTalisman();
	FMatsuriArtPose Chochin();
	FMatsuriArtPose Kasa();
	FMatsuriArtPose Kappa();

	/** First-person weapon held in view (offset relative to the camera: forward, right, up). */
	FMatsuriArtPose ViewWeapon(EMatsuriHero Hero);

	/**
	 *  Sakura draws a bow: an arrow nocked on the string, drawn back as the shot charges. (The bow mesh
	 *  stands along +Z with its handle toward +X and the string on the -X side:
	 *  tools/blender/normalize_matsuri.py "align": "bow".)
	 */
	bool IsBow(EMatsuriHero Hero);

	/** Mesh flown by the heroine's shots (empty name = keep the glowing shape). */
	FMatsuriArtPose Shot(EMatsuriHero Hero);
	FMatsuriArtPose ZeniCoin();
}
