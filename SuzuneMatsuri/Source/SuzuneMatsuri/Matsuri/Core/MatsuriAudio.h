// Suzune's Night Festival - sound effects (Kenney CC0 + synthesised; tools/audio/build_sfx.py).

#pragma once

#include "CoreMinimal.h"

class UAudioComponent;
class USoundBase;

enum class EMatsuriSfx : uint8
{
	Footstep,
	CoinPickup,
	GemPickup,
	TalismanPickup,
	BowDraw,
	BowRelease,
	ArrowWhoosh,
	ArrowThunk,
	KunaiThrow,
	BellShot,
	BellBounce,
	ZeniThrow,
	CoinBounce,
	MeleeSwing,
	MeleeHit,
	HitTick,
	Kill,
	YokaiDeath,
	PlayerHurt,
	DryFire,
	UiClick,
	UiConfirm,
	Loading,
	FloorClear,
	Graze,			// a boss bullet brushed past (Touhou's graze)
	BossShot,		// she releases a pattern
	BossPhase,		// her next health bar begins
	BossTeleport,
	GrappleFire,
	BossDefeat,
	Count
};

/**
 *  Every effect is optional like the art: a missing sound is silent. World sounds fall off with distance;
 *  repeated ones (steps, ticks, coins) vary a little in pitch so they do not sound like a machine.
 */
namespace Matsuri::Audio
{
	/** The asset name without the SFX_ prefix, e.g. "BowRelease" (Footstep has numbered variants). */
	SUZUNEMATSURI_API const TCHAR* Name(EMatsuriSfx Sfx);

	/** The sound (a random variant for footsteps); nullptr when not imported. */
	SUZUNEMATSURI_API USoundBase* Sound(EMatsuriSfx Sfx);

	/** Heard as the player's own (UI, her weapon, hit confirms). */
	SUZUNEMATSURI_API void Play2D(const UObject* WorldContext, EMatsuriSfx Sfx, float Volume = 1.0f);

	/** Like Play2D, but returns the playing sound so it can be cut short (the bow's draw on release). */
	SUZUNEMATSURI_API UAudioComponent* Spawn2D(const UObject* WorldContext, EMatsuriSfx Sfx, float Volume = 1.0f);

	/** Loads every effect now, so none is read from disk the first time it plays (a hitch mid-fight). */
	SUZUNEMATSURI_API void Preload();

	/** Heard from Location in the world. */
	SUZUNEMATSURI_API void PlayAt(const UObject* WorldContext, EMatsuriSfx Sfx, const FVector& Location, float Volume = 1.0f);

	/** How many times Sfx was asked for since the editor started (tests check events make their sounds). */
	SUZUNEMATSURI_API int32 PlayCount(EMatsuriSfx Sfx);
}
