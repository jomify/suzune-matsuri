// Suzune's Night Festival - the heroine's weapon held in view (first-person view model).

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "MatsuriArtTable.h"
#include "MatsuriViewModel.generated.h"

class ACharacter;
class UStaticMeshComponent;
class UMatsuriCombatComponent;

/**
 *  Rides on the player's camera, so it follows the final view every frame (the camera turns to the
 *  control rotation during the camera update, after all ticks; children move with it). Drawn in world
 *  space, not with first-person FOV: the weapon tip on screen is exactly where shots leave from.
 *  It sits inside the capsule's reach so it never pokes into walls.
 *
 *  Animation in the Overwatch manner: sway against fast turns, a bob while walking, a recoil kick per
 *  shot, a swing for melee, and the bow drawn back while charging.
 */
UCLASS()
class SUZUNEMATSURI_API UMatsuriViewModelComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UMatsuriViewModelComponent();

	/** Puts the heroine's weapon in view (once per pawn). nullptr when there is no camera or no art. */
	static UMatsuriViewModelComponent* Equip(ACharacter& Hero, EMatsuriHero Heroine);

	/** World position of the weapon's tip: shots start here. */
	FVector GetMuzzleLocation() const;

	/** The bow: an arrow on the string, drawn back as the shot charges (GetDrawLocation). */
	bool IsBow() const { return bBow; }

	/** The arrow's nock on the string, pulled back as the shot charges. */
	FVector GetDrawLocation() const { return NockWorld; }

	/** The nocked arrow is shown (it is gone for a moment after each shot, and when out of ammo). */
	bool IsArrowNocked() const;

	/** Camera height above the capsule centre: her eyes (the camera no longer rides the template's head bone). */
	static constexpr float EyeHeight = 55.0f;

	/** Rest pose relative to the camera (forward, right, up), before any animation. */
	FVector GetRestOffset() const { return Pose.Offset; }

	UStaticMeshComponent* GetWeapon() const { return Weapon; }


	void Kick();
	void Swing();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyPose(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Weapon;

	TWeakObjectPtr<UMatsuriCombatComponent> Combat;
	FDelegateHandle ShotHandle;
	FDelegateHandle SwingHandle;

	FMatsuriArtPose Pose;
	FVector MuzzleLocal = FVector::ZeroVector;	// tip of the weapon in this component's space

	void PoseBow(float Draw);
	bool bBow = false;
	FVector NockLocal = FVector::ZeroVector;	// the string's middle, in the bow mesh's space
	FVector TopTipLocal = FVector::ZeroVector;	// where the string meets the bow
	FVector BottomTipLocal = FVector::ZeroVector;
	FVector NockWorld = FVector::ZeroVector;
	float ArrowLength = 0.0f;
	double LastShotTime = -1000.0;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Arrow;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> StringTop;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> StringBottom;

	UPROPERTY(Transient)
	TObjectPtr<class UPointLightComponent> TipGlow;

	float RestFieldOfView = 0.0f;

public:
	/** How far the view has narrowed for the draw (tests: it must come back on release). */
	float GetDrawZoom() const;
	FRotator LastView = FRotator::ZeroRotator;
	bool bHasLastView = false;
	FRotator Sway = FRotator::ZeroRotator;
	float BobPhase = 0.0f;
	float BobWeight = 0.0f;
	float KickAlpha = 0.0f;
	float SwingAge = -1.0f;	// < 0: not swinging
};
