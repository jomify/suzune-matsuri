// Suzune's Night Festival - pick a heroine before the run.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatsuriHeroes.h"
#include "MatsuriHeroSelectWidget.generated.h"

/** One card per heroine (weapon, passive, Shift / E / Q). Click a card or press 1 / 2 / 3. */
UCLASS()
class SUZUNEMATSURI_API UMatsuriHeroSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Starts a run with Hero. Only the first choice counts. */
	void ChooseHero(EMatsuriHero Hero);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UWidget* BuildCard(EMatsuriHero Hero);

	UFUNCTION()
	void HandleSakura() { ChooseHero(EMatsuriHero::Sakura); }

	UFUNCTION()
	void HandleSuzune() { ChooseHero(EMatsuriHero::Suzune); }

	UFUNCTION()
	void HandleMio() { ChooseHero(EMatsuriHero::Mio); }

	bool bChosen = false;
};
