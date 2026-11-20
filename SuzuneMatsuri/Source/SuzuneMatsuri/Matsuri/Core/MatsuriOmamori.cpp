// Suzune's Night Festival - omamori (charm) upgrades offered between floors.

#include "MatsuriOmamori.h"
#include "Math/RandomStream.h"

#define LOCTEXT_NAMESPACE "MatsuriOmamori"

FMatsuriModifiers Matsuri::Omamori::Apply(const FMatsuriModifiers& Base, EMatsuriOmamori Charm)
{
	FMatsuriModifiers Result = Base;
	switch (Charm)
	{
	case EMatsuriOmamori::Kinun:	Result.KobanBonus += KinunBonus; break;
	case EMatsuriOmamori::Hisshou:	Result.DamageMultiplier *= HisshouDamage; break;
	case EMatsuriOmamori::Dangoke:	Result.AmmoCapMultiplier *= DangokeAmmoCap; break;
	case EMatsuriOmamori::Gakugyou:	Result.AttackIntervalMultiplier *= GakugyouInterval; break;
	case EMatsuriOmamori::Toki:		Result.ExtraSeconds += TokiSeconds; break;
	case EMatsuriOmamori::Kitsune:	Result.FoxBlessingsPerFloor += 1; break;
	case EMatsuriOmamori::Koutsuu:	Result.MoveSpeedMultiplier *= KoutsuuSpeed; break;
	case EMatsuriOmamori::Kenkou: Result.MaxHealthBonus += static_cast<int32>(KenkouHealth); break;
	case EMatsuriOmamori::Count:	checkNoEntry(); break;
	}
	return Result;
}

TArray<EMatsuriOmamori> Matsuri::Omamori::RollOffer(int32 RunSeed, int32 FloorIndex, int32 Size)
{
	TArray<EMatsuriOmamori> Pool;
	for (int32 i = 0; i < static_cast<int32>(EMatsuriOmamori::Count); ++i)
	{
		Pool.Add(static_cast<EMatsuriOmamori>(i));
	}

	FRandomStream Rng(HashCombine(GetTypeHash(RunSeed), GetTypeHash(FloorIndex)));
	for (int32 i = Pool.Num() - 1; i > 0; --i)
	{
		Pool.Swap(i, Rng.RandRange(0, i));
	}
	Pool.SetNum(FMath::Clamp(Size, 0, Pool.Num()));
	return Pool;
}

FText Matsuri::Omamori::DisplayName(EMatsuriOmamori Charm)
{
	switch (Charm)
	{
	case EMatsuriOmamori::Kinun:	return LOCTEXT("Kinun", "金運守");
	case EMatsuriOmamori::Kenkou:	return LOCTEXT("Kenkou", "健康守");
	case EMatsuriOmamori::Hisshou:	return LOCTEXT("Hisshou", "必勝守");
	case EMatsuriOmamori::Dangoke:	return LOCTEXT("Dangoke", "弾除け守");
	case EMatsuriOmamori::Gakugyou:	return LOCTEXT("Gakugyou", "学業守");
	case EMatsuriOmamori::Toki:		return LOCTEXT("Toki", "時の守");
	case EMatsuriOmamori::Kitsune:	return LOCTEXT("Kitsune", "狐の加護");
	case EMatsuriOmamori::Koutsuu:	return LOCTEXT("Koutsuu", "交通安全守");
	case EMatsuriOmamori::Count:	break;
	}
	return FText::GetEmpty();
}

FText Matsuri::Omamori::Effect(EMatsuriOmamori Charm)
{
	switch (Charm)
	{
	case EMatsuriOmamori::Kinun:	return LOCTEXT("KinunFx", "Koban worth +1");
	case EMatsuriOmamori::Kenkou:	return LOCTEXT("KenkouFx", "+50 max HP, heal 50");
	case EMatsuriOmamori::Hisshou:	return LOCTEXT("HisshouFx", "Damage +25%");
	case EMatsuriOmamori::Dangoke:	return LOCTEXT("DangokeFx", "Ammo capacity +50%");
	case EMatsuriOmamori::Gakugyou:	return LOCTEXT("GakugyouFx", "Attack 20% faster");
	case EMatsuriOmamori::Toki:		return LOCTEXT("TokiFx", "+20 s on every floor");
	case EMatsuriOmamori::Kitsune:	return LOCTEXT("KitsuneFx", "First hit each floor is absorbed");
	case EMatsuriOmamori::Koutsuu:	return LOCTEXT("KoutsuuFx", "Move speed +10%");
	case EMatsuriOmamori::Count:	break;
	}
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
