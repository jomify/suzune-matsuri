// Suzune's Night Festival - persistent high-score record.

#include "MatsuriRecord.h"

FMatsuriRecord Matsuri::Record::WithRun(const FMatsuriRecord& Record, int32 RunScore, int32 FloorReached, bool bVictory)
{
	FMatsuriRecord Result = Record;
	Result.BestScore = FMath::Max(Record.BestScore, FMath::Max(0, RunScore));
	Result.DeepestFloor = FMath::Max(Record.DeepestFloor, FMath::Max(0, FloorReached));
	Result.RunsPlayed = Record.RunsPlayed + 1;
	Result.Victories = Record.Victories + (bVictory ? 1 : 0);
	return Result;
}

bool Matsuri::Record::IsNewBest(const FMatsuriRecord& Record, int32 RunScore)
{
	return RunScore > Record.BestScore;
}
