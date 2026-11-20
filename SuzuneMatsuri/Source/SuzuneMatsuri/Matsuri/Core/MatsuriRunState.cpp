// Suzune's Night Festival - everything about one run.

#include "MatsuriRunState.h"

void FMatsuriRunState::Reset(int32 InSeed, EMatsuriHero InHero)
{
	*this = FMatsuriRunState();
	Seed = InSeed;
	Hero = InHero;
	Health = GetMaxHealth();
	Ammo = GetAmmoCap() / 2;
}

void FMatsuriRunState::BeginFloor(int32 TargetScore)
{
	Score.BeginFloor(TargetScore);
	FoxBlessingsLeft = Modifiers.FoxBlessingsPerFloor;
	Health = GetMaxHealth();
}

void FMatsuriRunState::AdvanceFloor()
{
	++FloorIndex;
}

int32 FMatsuriRunState::PointsFor(EMatsuriCollectible Kind) const
{
	const int32 KobanBonus = Kind == EMatsuriCollectible::Koban ? Modifiers.KobanBonus : 0;
	const int32 GinkaBonus = Kind == EMatsuriCollectible::Ginka ? GetHero().GinkaBonus : 0;
	return Matsuri::PointsFor(Kind) + KobanBonus + GinkaBonus;
}

bool FMatsuriRunState::Collect(EMatsuriCollectible Kind)
{
	const int32 Points = PointsFor(Kind);
	UltCharge = FMath::Min(1.0f, UltCharge + Points * Matsuri::Heroes::UltPerPoint);
	return Score.AddPoints(Points);
}

void FMatsuriRunState::AddDamageDealt(float Damage)
{
	UltCharge = FMath::Clamp(UltCharge + FMath::Max(0.0f, Damage) * Matsuri::Heroes::UltPerDamage, 0.0f, 1.0f);
}

bool FMatsuriRunState::ConsumeUlt()
{
	if (!IsUltReady())
	{
		return false;
	}
	UltCharge = 0.0f;
	return true;
}

bool FMatsuriRunState::SpendScore(int32 Amount)
{
	if (Amount <= 0 || Score.RunScore < Amount)
	{
		return false;
	}
	Score.RemovePoints(Amount);
	return true;
}

EMatsuriHurtResult FMatsuriRunState::Hurt(float Damage)
{
	if (!FMath::IsFinite(Damage) || Damage <= 0.0f || Health <= 0.0f)
	{
		return EMatsuriHurtResult::Ignored;
	}

	if (FoxBlessingsLeft > 0)
	{
		--FoxBlessingsLeft;
		return EMatsuriHurtResult::Blessed;
	}

	Health = FMath::Max(0.0f, Health - Damage);
	return Health > 0.0f ? EMatsuriHurtResult::Hurt : EMatsuriHurtResult::Died;
}

float FMatsuriRunState::GetMaxHealth() const
{
	return FMath::Min(Matsuri::Run::MaxHealth, Matsuri::Run::StartHealth + static_cast<float>(GetHero().ExtraHealth) + static_cast<float>(Modifiers.MaxHealthBonus));
}

float FMatsuriRunState::Heal(float Amount)
{
	if (!FMath::IsFinite(Amount) || Amount <= 0.0f || Health <= 0.0f || Health >= GetMaxHealth())
	{
		return 0.0f;
	}
	const float Before = Health;
	Health = FMath::Min(GetMaxHealth(), Health + Amount);
	return Health - Before;
}

int32 FMatsuriRunState::GetAmmoCap() const
{
	return FMath::RoundToInt32(GetHero().Weapon.AmmoCap * Modifiers.AmmoCapMultiplier);
}

int32 FMatsuriRunState::AddAmmo(int32 Amount)
{
	const int32 Added = FMath::Clamp(Amount, 0, GetAmmoCap() - Ammo);
	Ammo += Added;
	return Added;
}

bool FMatsuriRunState::ConsumeAmmo(int32 Amount)
{
	if (Amount <= 0 || Ammo < Amount)
	{
		return false;
	}
	Ammo -= Amount;
	return true;
}

void FMatsuriRunState::TakeCharm(EMatsuriOmamori Charm)
{
	Charms.Add(Charm);
	if (Charm == EMatsuriOmamori::Kenkou)
	{
		Modifiers = Matsuri::Omamori::Apply(Modifiers, Charm);
		Heal(Matsuri::Omamori::KenkouHealth);
		return;
	}
	Modifiers = Matsuri::Omamori::Apply(Modifiers, Charm);
}
