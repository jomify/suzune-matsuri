// Suzune's Night Festival - a damage number that pops off a struck yokai (hit feedback).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MatsuriDamageNumber.generated.h"

class UTextRenderComponent;

/**
 *  Playtest: hits needed clearer positive feedback. Each hit pops its damage above the yokai: it springs up,
 *  turns to the camera, and shrinks away. Strong hits are bigger and gold; the killing blow is red.
 */
UCLASS()
class SUZUNEMATSURI_API AMatsuriDamageNumber : public AActor
{
	GENERATED_BODY()

public:
	AMatsuriDamageNumber();

	static AMatsuriDamageNumber* Pop(UWorld* World, const FVector& Location, float Damage, bool bStrong, bool bKill);

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> Text;

	float Age = 0.0f;
	float BaseSize = 30.0f;
	FVector Drift = FVector::ZeroVector;
};
