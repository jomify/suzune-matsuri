// Suzune's Night Festival - death replay overlay.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatsuriReplayWidget.generated.h"

class UButton;
class UProgressBar;
class UTextBlock;
class AMatsuriReplayDirector;

UCLASS()
class SUZUNEMATSURI_API UMatsuriReplayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetDirector(AMatsuriReplayDirector* InDirector) { Director = InDirector; }
	void Configure(const FText& Cause, bool bHasSource);
	void SetProgress(float Progress, float Seconds);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandleSkipClicked();

	TWeakObjectPtr<AMatsuriReplayDirector> Director;
	FText BaseStatus;
	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY()
	TObjectPtr<UProgressBar> ProgressBar;
	UPROPERTY()
	TObjectPtr<UButton> SkipButton;
};

