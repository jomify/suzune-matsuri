// Suzune's Night Festival - 分身, the ninja's decoy that yokai chase instead of the player.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MatsuriDecoy.generated.h"

class UStaticMeshComponent;

/** A short-lived stand-in. Yokai within lure range target it (AMatsuriYokai::FindTarget). */
UCLASS()
class SUZUNEMATSURI_API AMatsuriDecoy : public AActor
{
	GENERATED_BODY()

public:
	AMatsuriDecoy();

	static constexpr float LifeSeconds = 4.0f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Torso;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Head;
};
