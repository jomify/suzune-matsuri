// Suzune's Night Festival - the redemption story chapters and ending.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriHeroes.h"
#include "MatsuriStory.generated.h"

USTRUCT(BlueprintType)
struct SUZUNEMATSURI_API FMatsuriStoryLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Story")
	FText Speaker;

	UPROPERTY(BlueprintReadOnly, Category="Story")
	FText Text;
};

USTRUCT(BlueprintType)
struct SUZUNEMATSURI_API FMatsuriStoryChapter
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Story")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category="Story")
	FText Objective;

	UPROPERTY(BlueprintReadOnly, Category="Story")
	TArray<FMatsuriStoryLine> Lines;

	bool IsValid() const { return !Title.IsEmpty() && !Objective.IsEmpty() && Lines.Num() > 0; }
};

namespace Matsuri::Story
{
	/** Story is enabled by default; automation can disable the presentation with matsuri.Story 0. */
	SUZUNEMATSURI_API bool IsEnabled();

	/** Returns the chapter shown when entering a maze or the boss floor. */
	SUZUNEMATSURI_API FMatsuriStoryChapter ChapterFor(int32 FloorIndex, EMatsuriHero Hero);

	/** Returns the guardian-repair ending shown before the final victory card. */
	SUZUNEMATSURI_API FMatsuriStoryChapter EndingFor(EMatsuriHero Hero);
}
