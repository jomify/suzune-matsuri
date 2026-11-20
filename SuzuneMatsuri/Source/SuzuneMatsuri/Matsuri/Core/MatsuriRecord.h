// Suzune's Night Festival - persistent high-score record (pure data, unit tested).

#pragma once

#include "CoreMinimal.h"
#include "MatsuriRecord.generated.h"

USTRUCT(BlueprintType)
struct SUZUNEMATSURI_API FMatsuriRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Record")
	int32 BestScore = 0;

	/** Highest floor number reached (1-based). 0 = never played. */
	UPROPERTY(BlueprintReadOnly, Category="Record")
	int32 DeepestFloor = 0;

	UPROPERTY(BlueprintReadOnly, Category="Record")
	int32 RunsPlayed = 0;

	UPROPERTY(BlueprintReadOnly, Category="Record")
	int32 Victories = 0;
};

namespace Matsuri::Record
{
	/** Returns a new record that includes one finished run. Negative inputs are treated as zero. */
	SUZUNEMATSURI_API FMatsuriRecord WithRun(const FMatsuriRecord& Record, int32 RunScore, int32 FloorReached, bool bVictory);

	/** True when RunScore beats the stored best. */
	SUZUNEMATSURI_API bool IsNewBest(const FMatsuriRecord& Record, int32 RunScore);
}
