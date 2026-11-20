// Suzune's Night Festival - shared yokai behaviour: health, contact damage, drops, movement helpers.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MatsuriTypes.h"
#include "MatsuriYokai.generated.h"

class USphereComponent;
class USceneComponent;

namespace Matsuri::Yokai
{
	/**
	 *  Where a blocked step goes instead: along the obstacle's face, never into it. Head-on (nothing left
	 *  to slide) it keeps the full step length and turns toward the obstacle's nearer edge, judged by
	 *  FromObstacleCentre, so a yokai walks round a crate instead of stalling against it.
	 */
	SUZUNEMATSURI_API FVector AroundObstacle(const FVector& Step, const FVector& HitNormal, const FVector& FromObstacleCentre);
}

/**
 *  Base for the killable yokai. Touching one hurts the player; shooting or
 *  striking it lowers its health; at zero it drops loot and vanishes.
 *  Movement is kinematic (swept SetActorLocation) because the gray-box floor is
 *  generated at runtime and has no navmesh.
 */
UCLASS(Abstract)
class SUZUNEMATSURI_API AMatsuriYokai : public AActor
{
	GENERATED_BODY()

public:
	AMatsuriYokai();

	/** Keeps the yokai inside the floor's walls. */
	void SetArenaHalfExtent(float HalfExtent) { ArenaHalfExtent = HalfExtent; }

	float GetHealth() const { return Health; }

	/** Freezes movement and attacks for Seconds (お祓い). */
	void Stun(float Seconds);
	bool IsStunned() const;

	/** Shoves the yokai by Offset (結界), sliding along walls. */
	void Knockback(const FVector& Offset);

	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** Subclasses spawn their loot here. */
	virtual void SpawnDrops() {}

	/** Colour of this yokai's hit / defeat effects. */
	virtual FLinearColor GetFxColor() const { return FLinearColor::White; }

	float HitPunchLeft = 0.0f;	// seconds of the hit swell still to play

	UFUNCTION()
	void HandleContact(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	APawn* FindPlayer() const;

	/** What the yokai chases and throws at: a nearby ninja decoy if there is one, else the player. */
	AActor* FindTarget() const;
	bool CanSee(const AActor* Target) const;

	/** The player is looking this way (view cone) and nothing blocks the line between us. */
	bool IsWatchedBy(const AActor* Target) const;

	/** Whether the player's eyes have a clear line to Spot (for picking cover). */
	bool IsVisibleFrom(const AActor* Target, const FVector& Spot) const;

	/**
	 *  Where to move next to reach Target: the next corridor waypoint on maze floors, Target itself on
	 *  the open test arena. OutSteps receives the corridor distance in cells (INDEX_NONE off-maze).
	 */
	FVector NextStepToward(const FVector& Target, int32* OutSteps = nullptr) const;

	/** Hunt when Target is within HuntCells along the corridors (maze) or ArenaRange in a straight line (arena). */
	bool ShouldHunt(const AActor* Target, int32 HuntCells, float ArenaRange) const;

	/** A place to amble to when not hunting: a random neighbouring cell (maze) or a nearby point (arena). */
	FVector PickWanderGoal() const;

	/** Moves by Delta, sliding along whatever it bumps into. */
	void MoveSwept(const FVector& Delta);

	/** Turns to face Target on the yaw axis only. */
	void FaceTowards(const FVector& Target);

	FVector ClampToArena(const FVector& Location) const;

	/** Drops land a little apart (random scatter), so double drops never stack on one spot. */
	void DropCollectible(EMatsuriCollectible Kind);
	void DropAmmo(int32 Amount);

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> Body;

	/** Parent of the visible parts, so subclasses can bob / hop the look without moving the collision. */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Visual;

	UPROPERTY(EditAnywhere, Category="Yokai", meta=(ClampMin=0.1))
	float MaxHealth = 1.0f;

	float Health = 1.0f;
	float ArenaHalfExtent = 2400.0f;
	bool bDead = false;
	double StunnedUntil = -1.0;
};
