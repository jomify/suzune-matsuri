// Suzune's Night Festival - skippable code-built story chapter card.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatsuriStory.h"
#include "MatsuriStoryWidget.generated.h"

class APlayerController;
class UTextBlock;
class UButton;

UCLASS()
class SUZUNEMATSURI_API UMatsuriStoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Creates and shows a chapter card. The callback runs once after input/pause is restored. */
	static UMatsuriStoryWidget* Show(APlayerController& PC, const FMatsuriStoryChapter& InChapter,
		FSimpleDelegate InOnFinished = FSimpleDelegate());

	UFUNCTION(BlueprintCallable, Category="Matsuri|Story")
	void NextLine();

	UFUNCTION(BlueprintCallable, Category="Matsuri|Story")
	void Finish();

	int32 GetLineIndex() const { return LineIndex; }
	bool IsFinished() const { return bFinished; }

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void Configure(const FMatsuriStoryChapter& InChapter, FSimpleDelegate InOnFinished);
	void Refresh();

	UFUNCTION()
	void HandleNextClicked() { NextLine(); }

	UFUNCTION()
	void HandleSkipClicked() { Finish(); }

	UPROPERTY()
	FMatsuriStoryChapter Chapter;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ObjectiveText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SpeakerText;

	UPROPERTY()
	TObjectPtr<UTextBlock> LineText;

	UPROPERTY()
	TObjectPtr<UTextBlock> PageText;

	UPROPERTY()
	TObjectPtr<UButton> NextButton;

	FSimpleDelegate OnFinished;
	TWeakObjectPtr<APlayerController> OwningController;
	bool bWasPaused = false;
	bool bWasCursorVisible = false;
	bool bFinished = false;
	int32 LineIndex = 0;
};
