// Suzune's Night Festival - everything about one run (pure data, unit tested).

#pragma once

#include "CoreMinimal.h"
#include "MatsuriHeroes.h"
#include "MatsuriOmamori.h"
#include "MatsuriScoreState.h"
#include "MatsuriTypes.h"
#include "MatsuriRunState.generated.h"

UENUM(BlueprintType)
enum class EMatsuriHurtResult : uint8
{
	Ignored,	// invalid or zero damage, or already at zero HP
	Blessed,	// a fox blessing absorbed the hit
	Hurt,		// damage was accepted and HP remains above zero
	Died		// damage reduced HP to zero
};

UENUM(BlueprintType)
enum class EMatsuriRunEnd : uint8
{
	None,
	OutOfHealth,
	TimeUp,
	Victory
};

namespace Matsuri::Run
{
	constexpr float StartHealth = 200.0f;
	constexpr float MaxHealth = 600.0f;
	constexpr int32 ZeniCost = 5;		// Mio's 投げ銭 spends this much score
}

USTRUCT(BlueprintType)
struct SUZUNEMATSURI_API FMatsuriRunState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Run")
	FMatsuriScoreState Score;

	UPROPERTY(BlueprintReadOnly, Category="Run")
	FMatsuriModifiers Modifiers;

	UPROPERTY(BlueprintReadOnly, Category="Run")
	TArray<EMatsuriOmamori> Charms;

	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Seed = 0;

	UPROPERTY(BlueprintReadOnly, Category="Run")
	EMatsuriHero Hero = EMatsuriHero::Sakura;

	/** 0..1; the Q ultimate is usable at 1. */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	float UltCharge = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 FloorIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category="Run")
	float Health = Matsuri::Run::StartHealth;

	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Ammo = 0;

	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 FoxBlessingsLeft = 0;

	/** Fresh run with the given seed and heroine: her health and ammo, half a magazine to start. */
	void Reset(int32 InSeed, EMatsuriHero InHero);

	const FMatsuriHeroDef& GetHero() const { return Matsuri::Heroes::Get(Hero); }

	/** Starts the current floor: resets floor score and refills fox blessings. */
	void BeginFloor(int32 TargetScore);

	/** Moves on to the next floor index. */
	void AdvanceFloor();

	/** Points a collectible is worth with the current modifiers. */
	int32 PointsFor(EMatsuriCollectible Kind) const;

	/** Scores the pickup and charges the ultimate. @return true only on the pickup that clears the floor. */
	bool Collect(EMatsuriCollectible Kind);

	/** Charges the ultimate for damage dealt. */
	void AddDamageDealt(float Damage);

	bool IsUltReady() const { return UltCharge >= 1.0f; }

	/** @return false (and changes nothing) unless the ultimate is fully charged. */
	bool ConsumeUlt();

	/** Spends run score (Mio's 投げ銭). @return false (and changes nothing) when there is not enough. */
	bool SpendScore(int32 Amount);

	/** Applies damage without changing score. Fox blessings absorb one hit. */
	EMatsuriHurtResult Hurt(float Damage = 30.0f);

	/** Maximum HP from the base, heroine passive and health charms. */
	float GetMaxHealth() const;

	/** Heals up to max HP and returns the actual amount restored. */
	float Heal(float Amount);

	int32 GetAmmoCap() const;

	/** Adds ammo up to the cap. @return how much was actually added. */
	int32 AddAmmo(int32 Amount);

	/** @return false (and changes nothing) when there is not enough ammo. */
	bool ConsumeAmmo(int32 Amount);

	void TakeCharm(EMatsuriOmamori Charm);
};
