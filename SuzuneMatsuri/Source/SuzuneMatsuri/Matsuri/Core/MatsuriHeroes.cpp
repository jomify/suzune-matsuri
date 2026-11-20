// Suzune's Night Festival - the playable heroines.

#include "MatsuriHeroes.h"

#define LOCTEXT_NAMESPACE "MatsuriHeroes"

namespace
{
	FMatsuriHeroDef MakeSakura()
	{
		FMatsuriHeroDef Hero;
		Hero.Id = EMatsuriHero::Sakura;
		Hero.Name = LOCTEXT("SakuraName", "春日 さくら");
		Hero.Title = LOCTEXT("SakuraTitle", "巫女");
		Hero.Role = LOCTEXT("SakuraRole", "Survivor  ·  charged piercing arrows");
		Hero.Color = FLinearColor(0.95f, 0.30f, 0.35f);
	Hero.Passive = LOCTEXT("SakuraPassive", "+50 HP");
	Hero.ExtraHealth = 50;

		FMatsuriWeaponDef& Bow = Hero.Weapon;
		Bow.Name = LOCTEXT("SakuraWeapon", "破魔矢  Hama-ya");
		Bow.AmmoCap = 12;
		Bow.FireInterval = 0.45f;
		Bow.Damage = 1.0f;
		Bow.ChargedDamage = 3.0f;
		Bow.MaxChargeSeconds = 0.8f;
		Bow.Speed = 11000.0f;
		Bow.Pierce = 3;
		Bow.Color = FLinearColor(1.0f, 0.95f, 0.85f);
		Bow.ShotScale = 0.10f;

		Hero.Skills[0] = { LOCTEXT("SakuraShift", "結界"), LOCTEXT("SakuraShiftFx", "2 s invulnerable, pushes yokai away"), 15.0f };
		Hero.Skills[1] = { LOCTEXT("SakuraE", "お祓い"), LOCTEXT("SakuraEFx", "Stuns yokai in front for 3 s"), 10.0f };
		Hero.Skills[2] = { LOCTEXT("SakuraQ", "大祓"), LOCTEXT("SakuraQFx", "Purges every yokai within 15 m"), 0.0f };
		return Hero;
	}

	FMatsuriHeroDef MakeSuzune()
	{
		FMatsuriHeroDef Hero;
		Hero.Id = EMatsuriHero::Suzune;
		Hero.Name = LOCTEXT("SuzuneName", "影山 すずね");
		Hero.Title = LOCTEXT("SuzuneTitle", "忍者");
		Hero.Role = LOCTEXT("SuzuneRole", "Mobility  ·  shuriken fans");
		Hero.Color = FLinearColor(0.35f, 0.45f, 0.95f);
		Hero.Passive = LOCTEXT("SuzunePassive", "Move +15%, double jump");
		Hero.MoveSpeedMultiplier = 1.15f;
		Hero.JumpCount = 2;

		FMatsuriWeaponDef& Shuriken = Hero.Weapon;
		Shuriken.Name = LOCTEXT("SuzuneWeapon", "手裏剣  Shuriken");
		Shuriken.AmmoCap = 30;
		Shuriken.FireInterval = 0.18f;
		Shuriken.Damage = 1.0f;
		Shuriken.Speed = 8500.0f;
		Shuriken.ShotsPerTrigger = 3;
		Shuriken.FanDegrees = 5.0f;
		Shuriken.Color = FLinearColor(0.75f, 0.80f, 0.95f);
		Shuriken.ShotScale = 0.09f;

		Hero.Skills[0] = { LOCTEXT("SuzuneShift", "瞬身"), LOCTEXT("SuzuneShiftFx", "Blink 6 m forward (not through walls)"), 6.0f };
		Hero.Skills[1] = { LOCTEXT("SuzuneE", "分身"), LOCTEXT("SuzuneEFx", "Leave a decoy that lures yokai for 4 s"), 14.0f };
		Hero.Skills[2] = { LOCTEXT("SuzuneQ", "影縫い"), LOCTEXT("SuzuneQFx", "6 s: shuriken home in and cost no ammo"), 0.0f };
		return Hero;
	}

	FMatsuriHeroDef MakeMio()
	{
		FMatsuriHeroDef Hero;
		Hero.Id = EMatsuriHero::Mio;
		Hero.Name = LOCTEXT("MioName", "猫宮 みお");
		Hero.Title = LOCTEXT("MioTitle", "猫又");
		Hero.Role = LOCTEXT("MioRole", "Greed  ·  bouncing bell shots");
		Hero.Color = FLinearColor(1.0f, 0.75f, 0.25f);
		Hero.Passive = LOCTEXT("MioPassive", "Ginka worth +1");
		Hero.GinkaBonus = 1;

		FMatsuriWeaponDef& Bell = Hero.Weapon;
		Bell.Name = LOCTEXT("MioWeapon", "鈴弾  Suzu-dan");
		Bell.AmmoCap = 20;
		Bell.FireInterval = 0.25f;
		Bell.Damage = 1.0f;
		Bell.Speed = 8000.0f;
		Bell.Bounces = 2;
		Bell.Color = FLinearColor(1.0f, 0.80f, 0.20f);
		Bell.ShotScale = 0.13f;

		Hero.Skills[0] = { LOCTEXT("MioShift", "招き猫"), LOCTEXT("MioShiftFx", "8 s: coins and ammo fly to you"), 18.0f };
		Hero.Skills[1] = { LOCTEXT("MioE", "投げ銭"), LOCTEXT("MioEFx", "Spend 5 score: a heavy coin, no ammo"), 4.0f };
		Hero.Skills[2] = { LOCTEXT("MioQ", "千客万来"), LOCTEXT("MioQFx", "Pull all loot within 30 m, double drops 8 s"), 0.0f };
		return Hero;
	}
}

const FMatsuriHeroDef& Matsuri::Heroes::Get(EMatsuriHero Hero)
{
	static const FMatsuriHeroDef Roster[Count] = { MakeSakura(), MakeSuzune(), MakeMio() };
	const int32 Index = FMath::Clamp(static_cast<int32>(Hero), 0, Count - 1);
	return Roster[Index];
}

#undef LOCTEXT_NAMESPACE
