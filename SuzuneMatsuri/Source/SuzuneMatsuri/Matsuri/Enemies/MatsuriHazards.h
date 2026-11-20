// Suzune's Night Festival - invulnerable hazards: avoid them, you cannot shoot them.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MatsuriHazards.generated.h"

class UBoxComponent;
class USphereComponent;
class UStaticMeshComponent;

/** Shared: an overlap trigger that hurts the player while the hazard is armed. */
UCLASS(Abstract)
class SUZUNEMATSURI_API AMatsuriHazard : public AActor
{
	GENERATED_BODY()

public:
	AMatsuriHazard();

	bool IsArmed() const { return bArmed; }

protected:
	virtual void BeginPlay() override;

	/** Arms / disarms; arming hurts a player already standing inside. */
	void SetArmed(bool bNewArmed);

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Subclasses create their trigger shape and assign it here in their constructor. */
	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> Trigger;

	bool bArmed = true;
};

/** 狐火: a floating fox flame sweeping back and forth along a line. Always armed. */
UCLASS()
class SUZUNEMATSURI_API AMatsuriFoxfire : public AMatsuriHazard
{
	GENERATED_BODY()

public:
	AMatsuriFoxfire();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Flame;

	FVector Origin = FVector::ZeroVector;
	FVector Axis = FVector::ForwardVector;
	float Phase = 0.0f;
};

/** 地刺: floor spikes on a fixed rhythm with a red warning before they rise. */
UCLASS()
class SUZUNEMATSURI_API AMatsuriSpikeTrap : public AMatsuriHazard
{
	GENERATED_BODY()

public:
	AMatsuriSpikeTrap();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Plate;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Spikes;

	float CycleTime = 0.0f;
	bool bWarning = false;
};
