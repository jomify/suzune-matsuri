// Suzune's Night Festival - shots: arrows, shuriken, bells, thrown coins, kappa cucumbers.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MatsuriArtTable.h"
#include "MatsuriProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UPointLightComponent;

enum class EMatsuriShotLook : uint8
{
	Orb,		// Mio's bells
	Arrow,		// Sakura's arrows: long and thin, point-first
	Star,		// Suzune's kunai
	Coin,		// Mio's thrown zeni
	Cucumber	// the kappa's
};

/** What a shot does when it meets a wall or the floor. */
enum class EMatsuriShotImpact : uint8
{
	Vanish,		// flash and gone
	Stick,		// embeds and stays a moment (arrows, kunai)
	Bounce		// ricochets (bells, coins)
};

enum class EMatsuriSpinAxis : uint8
{
	None,
	EndOverEnd,	// a thrown knife
	Wobble,		// a bell swinging on its cord
	Flip		// a tossed coin
};

/** How a kind of shot flies: one table, so arrows, kunai and bells each obey their own physics. */
struct FMatsuriBallistics
{
	float GravityScale = 0.0f;
	float SpinDegreesPerSecond = 0.0f;
	EMatsuriSpinAxis SpinAxis = EMatsuriSpinAxis::None;
	EMatsuriShotImpact WorldImpact = EMatsuriShotImpact::Vanish;
	int32 WorldBounces = 0;
	float Bounciness = 0.0f;
	float EmbedDepth = 0.0f;		// cm a stuck shot sinks in
	float TrailWidth = 6.0f;		// cm
	float TrailSeconds = 0.035f;	// the tracer is as long as the shot flies in this time
	bool bGlints = false;			// sheds little sparks as it flies (bells, coins)
};

namespace Matsuri::Shots
{
	SUZUNEMATSURI_API FMatsuriBallistics BallisticsFor(EMatsuriShotLook Look);

	/** The launch direction (low arc) that brings a shot of Speed under GravityScale down on To. */
	SUZUNEMATSURI_API FVector LobDirection(const FVector& From, const FVector& To, float Speed, float GravityScale);
}

/** Everything a shot needs to know when it leaves the hand. */
struct FMatsuriShot
{
	FVector Direction = FVector::ForwardVector;	// world space
	float Speed = 5000.0f;
	float Damage = 1.0f;
	bool bHostileToPlayer = false;
	int32 Pierce = 0;		// extra yokai it may pass through
	int32 Bounces = 0;		// times it may jump to the next yokai
	TWeakObjectPtr<USceneComponent> HomingTarget;
	FLinearColor Color = FLinearColor::White;
	float Scale = 0.12f;
	EMatsuriShotLook Look = EMatsuriShotLook::Orb;
	FMatsuriArtPose Art;	// Tripo mesh flown instead of the glowing shape, when imported
};

UCLASS()
class SUZUNEMATSURI_API AMatsuriProjectile : public AActor
{
	GENERATED_BODY()

public:
	AMatsuriProjectile();

	/** Call between SpawnActorDeferred and FinishSpawning. */
	void Configure(const FMatsuriShot& InShot);

	bool IsHostileToPlayer() const { return Shot.bHostileToPlayer; }

	/** Knocked out of the air by a melee strike: sparks, then gone. */
	void Deflect();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** Walls and floors: the shot sticks, bounces or ends (see FMatsuriBallistics). Hostile shots also end on the player. */
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Friendly shots pass into yokai: damage, then pierce, bounce or stop. */
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	bool bDressed = false;
	void StickInto(const FHitResult& Hit);
	void SpawnGlint();
	UFUNCTION()
	void HandleStop(const FHitResult& ImpactResult);
	void StrikeYokai(AActor* Yokai);
	AActor* FindBounceTarget() const;
	void ApplyLook();

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> Collision;

	/** Turns the body for spinning shots (the trail and the collision do not spin). */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Spinner;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> Glow;

	/** Tapered glowing tracer behind the shot; grows from the muzzle up to a fixed length. */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Trail;

	FVector LaunchLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UProjectileMovementComponent> Movement;

	FMatsuriShot Shot;
	FMatsuriBallistics Ballistics;
	int32 WorldBouncesLeft = 0;
	float SpinAngle = 0.0f;
	float GlintClock = 0.0f;
	TSet<TWeakObjectPtr<AActor>> Struck;
};
