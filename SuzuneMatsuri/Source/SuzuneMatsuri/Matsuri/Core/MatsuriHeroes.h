// Suzune's Night Festival - the playable heroines: weapons, passives and skills (pure data, unit tested).

#pragma once

#include "CoreMinimal.h"
#include "MatsuriHeroes.generated.h"

UENUM(BlueprintType)
enum class EMatsuriHero : uint8
{
	Sakura,		// 春日さくら, shrine maiden: survival
	Suzune,		// 影山すずね, ninja: mobility
	Mio,		// 猫宮みお, nekomata: greed
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EMatsuriSkillSlot : uint8
{
	Shift,
	E,
	Q		// ultimate: needs a full charge instead of a cooldown
};

/** How one trigger pull of the hero's weapon behaves. */
struct FMatsuriWeaponDef
{
	FText Name;
	int32 AmmoCap = 20;
	float FireInterval = 0.22f;
	float Damage = 1.0f;
	float Speed = 5000.0f;
	int32 ShotsPerTrigger = 1;		// > 1 = fan
	float FanDegrees = 0.0f;		// angle between neighbouring shots in a fan
	float MaxChargeSeconds = 0.0f;	// > 0 = hold to charge, release to fire
	float ChargedDamage = 1.0f;		// damage at full charge
	int32 Pierce = 0;				// extra enemies a shot passes through
	int32 Bounces = 0;				// times a shot jumps to the next enemy
	FLinearColor Color = FLinearColor::White;
	float ShotScale = 0.12f;
};

struct FMatsuriSkillDef
{
	FText Name;
	FText Description;
	float Cooldown = 10.0f;		// ignored for Q (ultimate)
};

struct FMatsuriHeroDef
{
	EMatsuriHero Id = EMatsuriHero::Sakura;
	FText Name;			// 春日 さくら
	FText Title;		// 巫女
	FText Role;			// short English role line
	FLinearColor Color = FLinearColor::White;
	FMatsuriWeaponDef Weapon;
	FText Passive;
	int32 ExtraHealth = 0;
	float MoveSpeedMultiplier = 1.0f;
	int32 JumpCount = 1;
	int32 GinkaBonus = 0;
	FMatsuriSkillDef Skills[3];

	const FMatsuriSkillDef& Skill(EMatsuriSkillSlot Slot) const { return Skills[static_cast<int32>(Slot)]; }
};

namespace Matsuri::Heroes
{
	constexpr int32 Count = static_cast<int32>(EMatsuriHero::Count);

	/** Ultimate charge (0..1) gained per score point collected and per point of damage dealt. */
	constexpr float UltPerPoint = 0.02f;
	constexpr float UltPerDamage = 0.04f;

	SUZUNEMATSURI_API const FMatsuriHeroDef& Get(EMatsuriHero Hero);
}
