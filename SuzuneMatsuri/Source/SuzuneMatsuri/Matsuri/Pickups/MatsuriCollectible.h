// Suzune's Night Festival - koban / ginka / magatama pickup.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MatsuriTypes.h"
#include "MatsuriCollectible.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class URotatingMovementComponent;
class UStaticMesh;

/**
 *  A spinning collectible. When the player's pawn overlaps it, it adds its
 *  points to the run and destroys itself. Gray-box visuals are built from
 *  engine shapes until the Tripo meshes land in M3.
 */
UCLASS()
class SUZUNEMATSURI_API AMatsuriCollectible : public AActor
{
	GENERATED_BODY()

public:
	AMatsuriCollectible();

	/** Changes the tier and refreshes the visuals. */
	void SetKind(EMatsuriCollectible NewKind);

	EMatsuriCollectible GetKind() const { return Kind; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(EditAnywhere, Category="Matsuri")
	EMatsuriCollectible Kind = EMatsuriCollectible::Koban;

private:
	void ApplyVisuals();

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<URotatingMovementComponent> Spinner;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CoinMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> GemMesh;

	bool bCollected = false;
};
