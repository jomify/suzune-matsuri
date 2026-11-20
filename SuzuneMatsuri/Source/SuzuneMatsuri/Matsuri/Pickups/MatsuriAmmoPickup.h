// Suzune's Night Festival - 弾薬御札, the ammo talisman.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MatsuriAmmoPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class URotatingMovementComponent;

/**
 *  Adds ammo on overlap. Floor spots respawn after a delay; enemy drops expire.
 *  A full player walks through it without consuming it.
 */
UCLASS()
class SUZUNEMATSURI_API AMatsuriAmmoPickup : public AActor
{
	GENERATED_BODY()

public:
	AMatsuriAmmoPickup();

	/** Call before FinishSpawning. Respawning spots never expire; drops never respawn. */
	void Configure(int32 InAmount, bool bInRespawns);

	bool IsAvailable() const { return bAvailable; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	void SetAvailable(bool bNewAvailable);
	void Respawn();

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Talisman;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<URotatingMovementComponent> Spinner;

	int32 Amount = 5;
	bool bRespawns = true;
	bool bAvailable = true;
	FTimerHandle RespawnTimer;
};
