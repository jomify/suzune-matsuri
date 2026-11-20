// Suzune's Night Festival - game mode for the title screen.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MatsuriMenuGameMode.generated.h"

class UMatsuriMainMenuWidget;

/** Shows the main menu with a free mouse cursor; the player only spectates. */
UCLASS()
class SUZUNEMATSURI_API AMatsuriMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMatsuriMenuGameMode();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category="Matsuri|UI")
	TSubclassOf<UMatsuriMainMenuWidget> MenuWidgetClass;
};
