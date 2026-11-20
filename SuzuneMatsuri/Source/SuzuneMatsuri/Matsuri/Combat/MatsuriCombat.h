// Suzune's Night Festival - small shared combat helpers.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

namespace Matsuri::Combat
{
	/** The "Projectile" object channel defined in DefaultEngine.ini. */
	constexpr ECollisionChannel ProjectileChannel = ECC_GameTraceChannel1;

	/** Tag carried by every yokai so melee / tests can find damageable enemies. */
	inline const FName YokaiTag(TEXT("Yokai"));

	/** True when Actor is the pawn of a human player. */
	SUZUNEMATSURI_API bool IsPlayerPawn(const AActor* Actor);

	/** Routes "the player got hit by Source" to the floor game mode (lives, respawn, i-frames). */
	SUZUNEMATSURI_API void HurtPlayer(const UObject* WorldContext, const AActor* Source);
}
