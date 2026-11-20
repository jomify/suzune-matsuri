// Suzune's Night Festival - victory card shown when the floor target is reached.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatsuriVictoryWidget.generated.h"

class UVerticalBox;

/**
 *  Floors 1-2: "the torii opens" + Choose Omamori (opens the charm picker).
 *  Last floor: run complete + record + Play Again / Main Menu.
 */
UCLASS()
class SUZUNEMATSURI_API UMatsuriVictoryWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

private:
	void AddFloorClearedContent(UVerticalBox& Column);
	void AddRunCompleteContent(UVerticalBox& Column);

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandlePlayAgainClicked();

	UFUNCTION()
	void HandleMenuClicked();
};
