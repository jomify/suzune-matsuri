// Suzune's Night Festival - the three killable yokai.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriYokai.h"
#include "MatsuriYokaiKinds.generated.h"

/** 提灯お化け: a lantern ghost. Floats over crates and hunts the hero through the corridors. 1 hit, drops koban. */
UCLASS()
class SUZUNEMATSURI_API AMatsuriChochin : public AMatsuriYokai
{
	GENERATED_BODY()

public:
	AMatsuriChochin();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void SpawnDrops() override;
	virtual FLinearColor GetFxColor() const override { return FLinearColor(1.0f, 0.45f, 0.08f); }

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Lantern;

	FVector WanderGoal = FVector::ZeroVector;
	bool bHasWanderGoal = false;
	float BobTime = 0.0f;
};

/** 唐傘お化け: an umbrella yokai. Hops after the hero along the corridors. 3 hits, drops ginka or ammo. */
UCLASS()
class SUZUNEMATSURI_API AMatsuriKasa : public AMatsuriYokai
{
	GENERATED_BODY()

public:
	AMatsuriKasa();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void SpawnDrops() override;
	virtual FLinearColor GetFxColor() const override { return FLinearColor(0.7f, 0.25f, 0.9f); }

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Canopy;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Handle;

	FVector HopVelocity = FVector::ZeroVector;
	float HopElapsed = 0.0f;
	float HopSeconds = 0.55f;	// a pounce is quicker and higher than a step
	float HopHeight = 150.0f;
	float RestLeft = 0.5f;
	bool bHopping = false;
};

/** 河童: stalks the hero to throwing range, keeps its distance, lobs cucumbers with lead. 4 hits, always drops ammo, sometimes a magatama. */
UCLASS()
class SUZUNEMATSURI_API AMatsuriKappa : public AMatsuriYokai
{
	GENERATED_BODY()

public:
	AMatsuriKappa();
	virtual void Tick(float DeltaSeconds) override;

	/** Straight-line distance at which it stops stalking and starts throwing from cover. */
	static constexpr float ThrowRange = 1800.0f;

protected:
	virtual void BeginPlay() override;
	virtual void SpawnDrops() override;
	virtual FLinearColor GetFxColor() const override { return FLinearColor(0.3f, 0.95f, 0.4f); }

private:
	void Throw(const AActor& Target);

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Shell;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Dish;

	float ThrowCooldown = 1.5f;

	/** The nearest reachable spot the player cannot see (bWantHidden) or can see (a place to throw from). */
	bool FindSpot(const AActor& Player, bool bWantHidden, FVector& OutSpot) const;
	FVector CoverSpot = FVector::ZeroVector;
	FVector PeekSpot = FVector::ZeroVector;
	bool bHasCover = false;
	bool bHasPeek = false;
	float CoverClock = 0.0f;
};
