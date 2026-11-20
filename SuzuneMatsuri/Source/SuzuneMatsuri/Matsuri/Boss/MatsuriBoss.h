// Suzune's Night Festival - 九尾狐「玉藻」: the boss herself.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MatsuriBossArena.h"
#include "MatsuriBossSkills.h"
#include "MatsuriBoss.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;
class AMatsuriDanmakuField;
class UMatsuriBossBrainComponent;
struct FMatsuriBossObservation;

DECLARE_MULTICAST_DELEGATE(FOnMatsuriBossDefeated);

/**
 *  Hovers over the courtyard and fights in turns: a short telegraph, one skill (chosen by her brain),
 *  a breath, the next. Three health bars: each new bar clears the screen, gives her two seconds and
 *  opens more skills. Player shots find her through the Yokai tag like any yokai.
 */
UCLASS()
class SUZUNEMATSURI_API AMatsuriBoss : public AActor
{
	GENERATED_BODY()

public:
	AMatsuriBoss();

	/** Call right after spawning. */
	void Setup(AMatsuriDanmakuField* InField, const FMatsuriBossArena& InArena);

	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** The player lost the run (or left): she records the fight as hers. */
	void EndFightAsWinner();

	/** Developer mode (F7): her next health bar starts now; on the last bar she is defeated. */
	void DevNextBar();

	/** Developer mode (F5): defeated at once. */
	void DevDefeat();

	float GetHealthFraction() const { return Health / MaxHealth; }
	float GetHealth() const { return Health; }
	int32 GetPhase() const { return Phase; }
	bool IsDefeated() const { return bDefeated; }
	bool IsShielded() const;
	/** The skill in progress, if any. */
	const FMatsuriSkillProgram* GetProgram() const { return bRunningSkill ? &Program : nullptr; }
	const FString& GetTaunt() const { return Taunt; }
	float GetTauntAge() const { return TauntAge; }
	UMatsuriBossBrainComponent* GetBrain() const { return Brain; }
	AMatsuriDanmakuField* GetField() const { return Field; }

	/** Phase N begins below this fraction of her health (index 1 and 2). */
	static float PhaseThreshold(int32 PhaseIndex);
	static int32 PhaseFor(float HealthFraction);

	FOnMatsuriBossDefeated OnDefeated;

	static constexpr float DefaultMaxHealth = 150.0f;
	static constexpr float IntroSeconds = 2.5f;
	static constexpr float PhaseShiftSeconds = 2.0f;
	static constexpr float ModelGraceSeconds = 1.2f;	// she holds her next skill this long for an answer on its way
	static constexpr int32 MagatamaDrops = 8;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Swaps the gray-box stand-in for the Tripo model if it exists. */
	void DressInArt();
	FMatsuriBossView MakeView() const;
	FMatsuriBossObservation Observe() const;
	void StartNextSkill();
	void BeginPhase(int32 NewPhase);
	void Defeat();
	void MoveAndPose(float DeltaSeconds);
	int32 PickAnchor(int32 Preferred) const;
	void SetTaunt(const FString& Line);
	float GapSeconds() const;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> Body;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Visual;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Halo;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Tails;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UMatsuriBossBrainComponent> Brain;

	/** Her Tripo model, when it has been imported (the gray-box stand-in otherwise). */
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Figure;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HaloMaterial;

	UPROPERTY(Transient)
	TObjectPtr<AMatsuriDanmakuField> Field;

	FMatsuriBossArena Arena;
	FMatsuriSkillProgram Program;
	FRandomStream Random;
	TArray<FMatsuriBullet> Fired;

	float MaxHealth = DefaultMaxHealth;
	float Health = DefaultMaxHealth;
	int32 Phase = 1;
	int32 Anchor = 0;
	float FightSeconds = 0.0f;
	float StateTime = 0.0f;
	float HitPunch = 0.0f;
	float Clock = 0.0f;
	FString Taunt;
	float TauntAge = 100.0f;
	float HealthAtStart = 0.0f;
	bool bIntro = true;
	bool bShifting = false;
	bool bRunningSkill = false;
	bool bDefeated = false;
	bool bFightOpen = false;
};
