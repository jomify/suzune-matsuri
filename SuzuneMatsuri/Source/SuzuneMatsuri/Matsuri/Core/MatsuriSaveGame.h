// Suzune's Night Festival - on-disk high-score record.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "MatsuriRecord.h"
#include "MatsuriSaveGame.generated.h"

/** Written to Saved/SaveGames/<slot>.sav; a readable JSON copy sits next to it. */
UCLASS()
class SUZUNEMATSURI_API UMatsuriSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FMatsuriRecord Record;
};
