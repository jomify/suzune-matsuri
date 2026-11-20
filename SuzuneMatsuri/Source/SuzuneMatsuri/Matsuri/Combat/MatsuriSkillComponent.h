// Suzune's Night Festival - Shift / E / Q skills of the three heroines.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MatsuriHeroes.h"
#include "MatsuriSkillComponent.generated.h"

class UInputAction;
class UInputMappingContext;

DECLARE_MULTICAST_DELEGATE_OneParam(FMatsuriSkillUsedSignature, EMatsuriSkillSlot);

/**
 *  Added to the hero pawn by the floor game mode next to the combat component.
 *  Shift and E run on cooldowns from FMatsuriHeroDef; Q needs a full ultimate charge.
 *
 *    Sakura  Shift 結界 invulnerable + knockback   E お祓い cone stun      Q 大祓 purge nearby yokai
 *    Suzune  Shift 瞬身 blink (stops at walls)       E 分身 decoy            Q 影縫い homing, free ammo
 *    Mio     Shift 招き猫 loot magnet                E 投げ銭 score -> heavy coin   Q 千客万来 pull all loot + double drops
 */
UCLASS(ClassGroup=(Matsuri), meta=(BlueprintSpawnableComponent))
class SUZUNEMATSURI_API UMatsuriSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMatsuriSkillComponent();

	/** Uses the skill if it is off cooldown (Q: if the ultimate is charged). @return whether it fired. */
	bool TryUseSkill(EMatsuriSkillSlot Slot);

	/** Seconds until Shift / E are ready (0 = ready). Q always reports 0; check the ultimate charge. */
	float GetCooldownRemaining(EMatsuriSkillSlot Slot) const;

	bool IsMagnetActive() const;
	void SetUltimateShowcaseLock(bool bLocked) { bUltimateShowcaseLocked = bLocked; }

	FMatsuriSkillUsedSignature OnSkillUsed;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindInput();
	void UnbindInput();
	void HandleShift() { TryUseSkill(EMatsuriSkillSlot::Shift); }
	void HandleE() { TryUseSkill(EMatsuriSkillSlot::E); }
	void HandleQ() { TryUseSkill(EMatsuriSkillSlot::Q); }

	/** Runs the heroine's skill. @return false if it could not be used (nothing changes, no cooldown). */
	bool Perform(EMatsuriHero Hero, EMatsuriSkillSlot Slot);

	bool SakuraBarrier();
	bool SakuraPurifyCone();
	bool SakuraGreatPurge();
	bool SuzuneBlink();
	bool SuzuneDecoy();
	bool SuzuneShadowStitch();
	bool MioBeckoningCat();
	bool MioZeni();
	bool MioFullHouse();

	void StartMagnet(float Radius, float Seconds, float PullSpeed);
	void TickMagnet(float DeltaTime);
	bool GetView(FVector& OutLocation, FVector& OutDirection) const;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ShiftAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> EAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> QAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Mapping;

	TArray<uint32> BindingHandles;
	double ReadyAt[3] = { 0.0, 0.0, 0.0 };
	double MagnetUntil = -1.0;
	float MagnetRadius = 0.0f;
	float MagnetSpeed = 0.0f;
	bool bUltimateShowcaseLocked = false;
};
