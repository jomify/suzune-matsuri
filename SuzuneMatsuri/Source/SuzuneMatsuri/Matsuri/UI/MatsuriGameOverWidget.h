// Suzune's Night Festival - run over: out of lives or out of time.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatsuriGameOverWidget.generated.h"

/** Shows why the run ended, the run score against the saved best, and Try Again / Main Menu. */
UCLASS()
class SUZUNEMATSURI_API UMatsuriGameOverWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandleRetryClicked();

	UFUNCTION()
	void HandleMenuClicked();
};
