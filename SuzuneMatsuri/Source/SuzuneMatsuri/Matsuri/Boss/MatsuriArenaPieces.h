// Suzune's Night Festival - the boss courtyard's live pieces: fences that break, rings the grapple holds.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MatsuriArenaPieces.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/**
 *  玉垣: a wooden fence that blocks the boss's bullets until it has taken MaxHits of them, then breaks
 *  and grows back after RegrowSeconds. Nowhere is safe for ever - and where the player hides is
 *  something the boss can learn.
 */
UCLASS()
class SUZUNEMATSURI_API AMatsuriBreakableCover : public AActor
{
	GENERATED_BODY()

public:
	AMatsuriBreakableCover();

	/** Call before FinishSpawning: half size in cm. */
	void SetExtent(const FVector& InExtent);

	bool IsStanding() const { return bStanding; }
	FBox GetBounds() const;
	int32 GetHitsTaken() const { return Hits; }

	/** One bullet struck it. */
	void AbsorbBullet();

	static constexpr int32 MaxHits = 25;
	static constexpr float RegrowSeconds = 20.0f;

protected:
	virtual void BeginPlay() override;

private:
	void SetStanding(bool bNewStanding);

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Boards;

	/** The Mountain Temple pack's wall segment standing in for the boards (when the pack is present). */
	UPROPERTY(Transient)
	TObjectPtr<AActor> Dressing;

	FVector Extent = FVector(300.0f, 25.0f, 90.0f);
	int32 Hits = 0;
	bool bStanding = true;
	FTimerHandle RegrowTimer;
};

/** A glowing shimenawa ring the grapple (F) can pull the player to. */
UCLASS()
class SUZUNEMATSURI_API AMatsuriHookPoint : public AActor
{
	GENERATED_BODY()

public:
	AMatsuriHookPoint();

	virtual void Tick(float DeltaSeconds) override;

	/** Brighter while the player's view is on it and it is in reach. */
	void SetHighlighted(bool bInHighlighted) { bHighlighted = bInHighlighted; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Ring;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Core;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingMaterial;

	bool bHighlighted = false;
	float Pulse = 0.0f;
};
