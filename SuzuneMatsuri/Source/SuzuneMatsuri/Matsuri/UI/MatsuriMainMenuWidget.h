// Suzune's Night Festival - title screen with Start Game / Developer / Quit.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatsuriMainMenuWidget.generated.h"

UCLASS()
class SUZUNEMATSURI_API UMatsuriMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleQuitClicked();

	/** 開発者モード: the developer panel (it also turns the in-game developer keys on). */
	UFUNCTION()
	void HandleDevClicked();
};
