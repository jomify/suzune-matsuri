// Suzune's Night Festival - danmaku: the boss's bullets as plain data, and the shapes they are fired in.

#pragma once

#include "CoreMinimal.h"

/** How a bullet is drawn (one instanced mesh per style). */
enum class EMatsuriBulletStyle : uint8
{
	Orb,	// round foxfire: rings, spirals, rain
	Rice,	// small and fast: aimed volleys, phantoms
	Large,	// the tail sweep's wall
	Count
};

/**
 *  One bullet. Bullets are not actors: hundreds fly at once, so they live in an array that the danmaku
 *  field advances and draws in one go (see AMatsuriDanmakuField). Everything here is game-thread data.
 */
struct FMatsuriBullet
{
	FVector Position = FVector::ZeroVector;
	FVector PreviousPosition = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Radius = 14.0f;				// cm, what touches the player's hit sphere
	float Age = 0.0f;
	float Lifetime = 8.0f;
	float Delay = 0.0f;					// seconds frozen in place before it moves (a telegraph)
	float Accel = 0.0f;					// cm/s^2 along its heading (negative slows it)
	float MinSpeed = 0.0f;
	float MaxSpeed = 3000.0f;
	float HomingDegreesPerSecond = 0.0f;
	float HomingSeconds = 0.0f;			// turns toward the player only this long, so it can be outrun
	float Damage = 10.0f;				// damage carried by this individual bullet
	EMatsuriBulletStyle Style = EMatsuriBulletStyle::Orb;
	uint8 Skill = 0;					// which skill fired it (colour, statistics)
	float ActiveStartFraction = 0.0f;	// part of the latest step spent waiting for Delay
	bool bHasPreviousPosition = false;
	bool bGrazed = false;
	bool bTouched = false;				// reached the player (counted once, even through invulnerability)
	bool bDead = false;
};

namespace Matsuri::Danmaku
{
	/** Advances one bullet by Dt, turning toward Target while it still homes. */
	SUZUNEMATSURI_API void Step(FMatsuriBullet& Bullet, float Dt, const FVector& Target);

	/** True when the latest active bullet segment overlaps a sphere; unstepped bullets use their position. */
	SUZUNEMATSURI_API bool Touches(const FMatsuriBullet& Bullet, const FVector& Center, float Radius);

	/** Earliest 0..1 contact along the latest segment, including relative movement of the player. */
	SUZUNEMATSURI_API bool PlayerImpact(const FMatsuriBullet& Bullet, const FVector& PreviousCenter, const FVector& Center,
		float Radius, float& OutFraction);

	/** Earliest contact with a static box expanded by the bullet radius. */
	SUZUNEMATSURI_API bool BoxImpact(const FMatsuriBullet& Bullet, const FBox& Box, float& OutFraction);

	/** Direction from From that reaches Target's height at Target's distance, at Yaw degrees around up. */
	SUZUNEMATSURI_API FVector ConeDirection(const FVector& From, const FVector& Target, float YawDegrees);

	/**
	 *  Count bullets round a cone from Origin, tilted so the ring passes through Target's height at Target's
	 *  distance; GapDegrees leaves an opening (centred on GapYaw) to slip through.
	 */
	SUZUNEMATSURI_API void Ring(TArray<FMatsuriBullet>& Out, const FMatsuriBullet& Template, const FVector& Origin, const FVector& Target,
		int32 Count, float Speed, float StartYaw, float GapDegrees, float GapYaw);

	/** Count bullets fanned SpreadDegrees wide, centred on Origin -> Target. */
	SUZUNEMATSURI_API void Fan(TArray<FMatsuriBullet>& Out, const FMatsuriBullet& Template, const FVector& Origin, const FVector& Target,
		int32 Count, float SpreadDegrees, float Speed);

	/**
	 *  A wall: rows of bullets HeightMin..HeightMax above GroundZ, spanning Width across Heading's side,
	 *  starting at Start and moving along Heading.
	 */
	SUZUNEMATSURI_API void Wall(TArray<FMatsuriBullet>& Out, const FMatsuriBullet& Template, const FVector& Start, const FVector& Heading,
		float Width, float Spacing, float GroundZ, float HeightMin, float HeightMax, float RowSpacing, float Speed);

	/** Count drops spawned Height above a disc (Center, Radius), falling at Speed. */
	SUZUNEMATSURI_API void Rain(TArray<FMatsuriBullet>& Out, const FMatsuriBullet& Template, const FVector& Center, float Radius,
		float Height, int32 Count, float Speed, FRandomStream& Random);
}
