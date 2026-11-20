// Suzune's Night Festival - pick one of three omamori between floors.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatsuriOmamori.h"
#include "MatsuriOmamoriWidget.generated.h"

class UButton;

/** Three charm cards; click one (or press 1 / 2 / 3) to take it and load the next floor. */
UCLASS()
class SUZUNEMATSURI_API UMatsuriOmamoriWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Takes card Index (0-based) of the current offer. Ignores out-of-range indices. */
	void ChooseCard(int32 Index);

	const TArray<EMatsuriOmamori>& GetOffer() const { return Offer; }

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UWidget* BuildCard(int32 Index);

	UFUNCTION()
	void HandleCard0() { ChooseCard(0); }

	UFUNCTION()
	void HandleCard1() { ChooseCard(1); }

	UFUNCTION()
	void HandleCard2() { ChooseCard(2); }

	TArray<EMatsuriOmamori> Offer;
	bool bChosen = false;
};
