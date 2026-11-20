// Suzune's Night Festival - omamori (charm) upgrades offered between floors.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriOmamori.generated.h"

UENUM(BlueprintType)
enum class EMatsuriOmamori : uint8
{
	Kinun,		// 金運守: koban +1 point
	Kenkou,		// 健康守: +50 max HP and heal 50
	Hisshou,	// 必勝守: weapon damage +25%
	Dangoke,	// 弾除け守: ammo cap +50%
	Gakugyou,	// 学業守: attack intervals -20%
	Toki,		// 時の守: +20 s on every later floor
	Kitsune,	// 狐の加護: first hit each floor is absorbed
	Koutsuu,	// 交通安全守: move speed +10%
	Count UMETA(Hidden)
};

/** Everything omamori can change. Neutral by default. */
USTRUCT(BlueprintType)
struct SUZUNEMATSURI_API FMatsuriModifiers
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Modifiers")
	int32 KobanBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category="Modifiers")
	float DamageMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="Modifiers")
	float AmmoCapMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="Modifiers")
	float AttackIntervalMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="Modifiers")
	float MoveSpeedMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="Modifiers")
	float ExtraSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Modifiers")
	int32 FoxBlessingsPerFloor = 0;

	UPROPERTY(BlueprintReadOnly, Category="Modifiers")
	int32 MaxHealthBonus = 0;
};

namespace Matsuri::Omamori
{
	constexpr int32 OfferSize = 3;
	constexpr int32 KinunBonus = 1;
	constexpr float HisshouDamage = 1.25f;
	constexpr float DangokeAmmoCap = 1.5f;
	constexpr float GakugyouInterval = 0.8f;
	constexpr float TokiSeconds = 20.0f;
	constexpr float KoutsuuSpeed = 1.1f;
	constexpr float KenkouHealth = 50.0f;

	/** Returns a copy of Base with the charm's effect stacked on. Kenkou HP is handled by the run state. */
	SUZUNEMATSURI_API FMatsuriModifiers Apply(const FMatsuriModifiers& Base, EMatsuriOmamori Charm);

	/** Deterministic offer of distinct charms for a floor of a run. */
	SUZUNEMATSURI_API TArray<EMatsuriOmamori> RollOffer(int32 RunSeed, int32 FloorIndex, int32 Size = OfferSize);

	SUZUNEMATSURI_API FText DisplayName(EMatsuriOmamori Charm);
	SUZUNEMATSURI_API FText Effect(EMatsuriOmamori Charm);
}
