// Suzune's Night Festival - score bookkeeping.

#include "MatsuriScoreState.h"

void FMatsuriScoreState::ResetRun()
{
	*this = FMatsuriScoreState();
}

void FMatsuriScoreState::BeginFloor(int32 Target)
{
	FloorScore = 0;
	FloorTarget = FMath::Max(1, Target);
	bFloorCleared = false;
}

bool FMatsuriScoreState::AddPoints(int32 Points)
{
	if (Points <= 0)
	{
		return false;
	}

	RunScore += Points;
	FloorScore += Points;

	if (!bFloorCleared && FloorScore >= FloorTarget)
	{
		bFloorCleared = true;
		return true;
	}
	return false;
}

int32 FMatsuriScoreState::RemovePoints(int32 Points)
{
	if (Points <= 0)
	{
		return 0;
	}
	const int32 Removed = FMath::Min(Points, RunScore);
	RunScore -= Removed;
	FloorScore = FMath::Max(0, FloorScore - Points);
	return Removed;
}

float FMatsuriScoreState::GetFloorProgress() const
{
	return FMath::Clamp(static_cast<float>(FloorScore) / static_cast<float>(FloorTarget), 0.0f, 1.0f);
}
