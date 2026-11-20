// Suzune's Night Festival - score bookkeeping (pure data, unit tested).

#pragma once

#include "CoreMinimal.h"
#include "MatsuriScoreState.generated.h"

/**
 *  Run-wide and per-floor score.
 *  RunScore accumulates across floors (it is what the high score records);
 *  FloorScore resets on every floor and is compared against FloorTarget.
 */
USTRUCT(BlueprintType)
struct SUZUNEMATSURI_API FMatsuriScoreState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Score")
	int32 RunScore = 0;

	UPROPERTY(BlueprintReadOnly, Category="Score")
	int32 FloorScore = 0;

	UPROPERTY(BlueprintReadOnly, Category="Score")
	int32 FloorTarget = 1;

	UPROPERTY(BlueprintReadOnly, Category="Score")
	bool bFloorCleared = false;

	/** Clears everything for a fresh run. */
	void ResetRun();

	/** Starts a new floor: keeps RunScore, resets FloorScore. Target is clamped to >= 1. */
	void BeginFloor(int32 Target);

	/**
	 *  Adds points to both counters. Non-positive values are ignored.
	 *  @return true only on the call that makes FloorScore reach FloorTarget.
	 */
	bool AddPoints(int32 Points);

	/**
	 *  Takes points away from both counters, never below zero.
	 *  A cleared floor stays cleared. Non-positive values are ignored.
	 *  @return how many points were actually removed from the run score.
	 */
	int32 RemovePoints(int32 Points);

	/** FloorScore / FloorTarget clamped to [0, 1]. */
	float GetFloorProgress() const;
};
