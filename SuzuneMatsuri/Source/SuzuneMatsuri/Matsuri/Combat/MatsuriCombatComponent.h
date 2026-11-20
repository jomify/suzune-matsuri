// Suzune's Night Festival - the heroine's weapon: ammo-fed shots plus a free melee swing.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MatsuriCombatComponent.generated.h"

class UInputAction;
class UInputMappingContext;
struct FMatsuriShot;

DECLARE_MULTICAST_DELEGATE(FMatsuriAttackSignature);

/**
 *  Added to the hero pawn at spawn by the floor game mode. Fires the current heroine's weapon
 *  (FMatsuriHeroDef::Weapon): Sakura holds to charge piercing arrows, Suzune throws shuriken
 *  fans, Mio's bells bounce between yokai. Input actions are created at runtime (LMB fire,
 *  V melee), so the template character Blueprint stays untouched.
 */
UCLASS(ClassGroup=(Matsuri), meta=(BlueprintSpawnableComponent))
class SUZUNEMATSURI_API UMatsuriCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMatsuriCombatComponent();

	/** Fires one trigger pull of the heroine's weapon. ChargeAlpha (0..1) only matters for charge weapons. */
	bool TryFire(float ChargeAlpha = 0.0f);

	/**
	 *  Strikes everything in a wide arc in front: damage, knockback and a short stun on yokai, and
	 *  incoming cucumbers are swatted out of the air. Costs no ammo, so the player is never stuck.
	 */
	bool TryMelee();

	/** Mio's 投げ銭: a heavy coin shot that costs no ammo (the score cost is paid by the skill). */
	void ThrowZeni();

	/** Suzune's 影縫い: for Seconds, shots home in on yokai and cost no ammo. */
	void ActivateShadowStitch(float Seconds);
	bool IsShadowStitchActive() const;
	/** Temporarily blocks firing/melee during an ultimate close-up; releasing cancels a held bow safely. */
	void SetUltimateShowcaseLock(bool bLocked);

	bool IsCharging() const { return bCharging; }

	/** Fire button held / let go (input, and tests). A charge weapon draws; others fire on press. */
	void StartFiring() { HandleFirePressed(); }
	void StopFiring() { HandleFireReleased(); }

	/** Drawing a bow slows her to this share of her walking speed. */
	static constexpr float DrawWalkScale = 0.5f;
	float GetChargeAlpha() const;

	/** Fired after a successful shot or swing (HUD / tests). */
	FMatsuriAttackSignature OnAttack;

	/** Fired when the trigger is pulled with no ammo. */
	FMatsuriAttackSignature OnDryFire;

	/** Fired when a melee strike knocks a hostile shot out of the air. */
	FMatsuriAttackSignature OnDeflect;

	/** Fired the moment a shot or a strike of this weapon damages a yokai (HUD hit marker). */
	FMatsuriAttackSignature OnHitConfirmed;
	void ConfirmHit() { OnHitConfirmed.Broadcast(); }

	/** Fired when a yokai this weapon hit dies (HUD kill marker). */
	FMatsuriAttackSignature OnKillConfirmed;
	void ConfirmKill() { OnKillConfirmed.Broadcast(); }

	/** Fired per trigger pull that launched shots / per melee swing (view model recoil and swing). */
	FMatsuriAttackSignature OnShot;
	FMatsuriAttackSignature OnSwing;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category="Melee", meta=(ClampMin=0.1, Units="s"))
	float MeleeInterval = 0.55f;

	UPROPERTY(EditAnywhere, Category="Melee", meta=(ClampMin=0, Units="cm"))
	float MeleeReach = 260.0f;

	/** Half the swing arc: 55 = a 110 degree sweep in front. */
	UPROPERTY(EditAnywhere, Category="Melee", meta=(ClampMin=5, ClampMax=180, Units="deg"))
	float MeleeHalfAngle = 55.0f;

	UPROPERTY(EditAnywhere, Category="Melee", meta=(ClampMin=0))
	float MeleeDamage = 2.0f;

	UPROPERTY(EditAnywhere, Category="Melee", meta=(ClampMin=0, Units="cm"))
	float MeleeKnockback = 350.0f;

	UPROPERTY(EditAnywhere, Category="Melee", meta=(ClampMin=0, Units="s"))
	float MeleeStun = 0.5f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindInput();
	void UnbindInput();
	void HandleFirePressed();
	void HandleFireHeld();
	void HandleFireReleased();
	void HandleMeleeInput() { TryMelee(); }

	bool IsReady(double LastTime, float BaseInterval) const;
	bool GetView(FVector& OutLocation, FVector& OutDirection) const;
	FVector GetMuzzle(const FVector& ViewLocation, const FVector& ViewDirection) const;
	FVector GetAimPoint(const FVector& ViewLocation, const FVector& ViewDirection) const;
	USceneComponent* FindHomingTarget(const FVector& ViewLocation, const FVector& ViewDirection) const;
	void SpawnShot(const FMatsuriShot& Shot, const FVector& Muzzle);
	float ScaledDamage(float Base) const;
	bool InSwing(const FVector& Eye, const FVector& Facing2D, const FVector& Point, float Slack) const;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MeleeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Mapping;

	TArray<uint32> BindingHandles;
	double LastFireTime = -1000.0;
	double LastMeleeTime = -1000.0;
	double ChargeStartTime = 0.0;
	double ShadowStitchUntil = -1.0;
	double BufferedFireUntil = -1.0;	// a click just before the weapon is ready still fires (input buffer)
	float WalkSpeedBeforeDraw = 0.0f;
	void SlowForDraw(bool bDrawing);
	bool bCharging = false;
	bool bUltimateShowcaseLocked = false;

	/** The bow's draw sound while it plays (cut short when the arrow is loosed early). */
	TWeakObjectPtr<class UAudioComponent> DrawSound;
	static constexpr float DrawSoundFadeSeconds = 0.04f;
};
