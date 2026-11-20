// Suzune's Night Festival - yokai tactics as pure decisions (unit-tested; the yokai feed in what they sense).

#pragma once

#include "CoreMinimal.h"

/**
 *  Playtest: "the yokai are not very smart". Each kind now fights by its nature:
 *    - the chochin (a floating lantern) closes in fast from your blind side and barely creeps while you
 *      look at it, then lunges once it is close and unseen;
 *    - the kasa (a hopping umbrella) lurks around a corner and pounces when you come within reach;
 *    - the kappa (a thrower) ducks into cover between throws and peeks out to throw.
 */
namespace Matsuri::YokaiBrain
{
	/** True when Point is inside the player's view cone (line of sight is checked by the caller). */
	SUZUNEMATSURI_API bool IsInViewCone(const FVector& Eye, const FVector& ViewDirection, const FVector& Point, float CosHalfAngle = 0.75f);

	enum class EChochinMove : uint8
	{
		Creep,		// watched: hangs almost still like an ordinary lantern, edging closer
		Stalk,		// unwatched: drifts in fast along the corridors
		Lunge		// unwatched and close: rushes straight in
	};
	SUZUNEMATSURI_API EChochinMove ChochinMove(bool bWatched, float Distance);
	SUZUNEMATSURI_API float ChochinSpeed(EChochinMove Move);
	inline constexpr float ChochinLungeRange = 320.0f;

	enum class EKasaMove : uint8
	{
		Path,		// hop along the corridors toward the player
		Lurk,		// out of sight and one corner away: wait, still
		Pounce		// in sight and in reach: one long leap onto the player
	};
	/** StepsAway: corridor steps to the player (INDEX_NONE off the maze). */
	SUZUNEMATSURI_API EKasaMove KasaMove(bool bHasSight, float Distance, int32 StepsAway);
	inline constexpr float KasaPounceRange = 480.0f;
	inline constexpr float KasaLurkRange = 1100.0f;

	/** A place the kappa could stand, as sensed around it. */
	struct FCoverSpot
	{
		FVector Location = FVector::ZeroVector;
		bool bReachable = false;			// nothing solid on the way there
		bool bHiddenFromPlayer = false;		// the player's line of sight to it is blocked
	};
	/** The nearest reachable hidden spot, or INDEX_NONE. */
	SUZUNEMATSURI_API int32 PickCover(const TArray<FCoverSpot>& Spots, const FVector& From);

	/** The nearest reachable spot the player can see (to step out and throw from), or INDEX_NONE. */
	SUZUNEMATSURI_API int32 PickPeek(const TArray<FCoverSpot>& Spots, const FVector& From);

	enum class EKappaMove : uint8
	{
		Hide,		// seen and reloading: get into cover
		Peek,		// ready to throw but cannot see the player: step out toward them
		Throw,		// ready and in sight: throw
		Hold,		// in cover, reloading
		Approach	// out of throwing range: stalk closer along the corridors
	};
	SUZUNEMATSURI_API EKappaMove KappaMove(bool bSeesPlayer, bool bReadyToThrow, bool bInRange);
}
