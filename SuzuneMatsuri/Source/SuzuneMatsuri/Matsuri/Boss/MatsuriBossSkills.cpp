// Suzune's Night Festival - Tamamo's six skills: what each does, its limits, and the program that fires it.

#include "MatsuriBossSkills.h"

#define LOCTEXT_NAMESPACE "MatsuriBossSkills"

namespace
{
	using namespace Matsuri::Danmaku;

	constexpr float RingInterval = 0.55f;
	constexpr float SpiralInterval = 0.1f;
	constexpr float PursuitInterval = 0.5f;
	constexpr int32 PursuitPerVolley = 9;
	constexpr float PursuitHomingSeconds = 1.2f;
	constexpr float SweepInterval = 2.2f;			// a second wall follows from the other side
	constexpr float SweepSpacing = 90.0f;			// bullets 48 cm wide: gaps of 42 cm, narrower than the 44 cm hit sphere
	constexpr float SweepRowSpacing = 80.0f;
	constexpr float SweepLowMin = 40.0f, SweepLowMax = 200.0f;		// ground level: jump onto a platform, or hide
	constexpr float SweepHighMin = 430.0f, SweepHighMax = 590.0f;	// platform level: drop to the ground, or climb higher
	constexpr float RainWarnSeconds = 0.6f;			// the marker on the ground before the first drop
	constexpr float RainStep = 0.1f;
	constexpr float RainHeight = 1400.0f;
	constexpr float PhantomInterval = 0.7f;
	constexpr float PhantomRadius = 1300.0f;
	constexpr int32 PhantomPerVolley = 3;
	constexpr int32 MaxTauntChars = 40;
	constexpr int32 MaxWhyChars = 160;

	float Param(const FMatsuriBossAction& Action, int32 Index, float Low, float High)
	{
		return FMath::Lerp(Low, High, Action.Params[Index]);
	}

	int32 ParamCountOf(const FMatsuriBossAction& Action, int32 Index, int32 Low, int32 High)
	{
		return FMath::RoundToInt32(Param(Action, Index, static_cast<float>(Low), static_cast<float>(High)));
	}

	float PhaseSpeed(int32 Phase)
	{
		return Phase >= 3 ? 1.15f : Phase == 2 ? 1.05f : 1.0f;
	}

	FMatsuriBullet Template(EMatsuriBossSkill Skill, EMatsuriBulletStyle Style, float Radius, float Lifetime, float Damage)
	{
		FMatsuriBullet Bullet;
		Bullet.Style = Style;
		Bullet.Radius = Radius;
		Bullet.Lifetime = Lifetime;
		Bullet.Skill = static_cast<uint8>(Skill);
		Bullet.Damage = Damage;
		return Bullet;
	}

	float SweepTravel(const FMatsuriBossView& View)
	{
		return View.HalfExtent * 2.4f;	// from outside one edge to past the other
	}

	/** Fires every shot that has come due; Fire(ShotIndex) emits one. */
	template <typename TFire>
	void FireDue(FMatsuriSkillProgram& Program, int32 TotalShots, float Interval, TFire&& Fire)
	{
		while (Program.Shots < TotalShots && Program.Time >= Program.NextShot)
		{
			Fire(Program.Shots);
			++Program.Shots;
			Program.NextShot += Interval;
		}
	}
}

float Matsuri::BossSkills::BulletDamageFor(const FMatsuriBossAction& Action)
{
	float Low = 10.0f, High = 10.0f;
	float PatternCount = 1.0f, MinCount = 1.0f, MaxCount = 1.0f;
	const auto P = [&Action](int32 Index) { return FMath::Clamp(FMath::IsFinite(Action.Params[Index]) ? Action.Params[Index] : 0.5f, 0.0f, 1.0f); };
	switch (Action.Skill)
	{
	case EMatsuriBossSkill::Ring:
		Low = 14.0f; High = 10.0f;
		PatternCount = FMath::RoundToFloat(FMath::Lerp(24.0f, 56.0f, P(0))) * FMath::RoundToFloat(FMath::Lerp(3.0f, 7.0f, P(2))) * (1.0f - P(3) / 6.0f);
		MinCount = 24.0f * 3.0f * (5.0f / 6.0f); MaxCount = 56.0f * 7.0f;
		break;
	case EMatsuriBossSkill::Spiral:
		Low = 12.0f; High = 8.0f;
		PatternCount = FMath::RoundToFloat(FMath::Lerp(3.0f, 8.0f, P(0))) * FMath::Lerp(3.0f, 6.0f, P(3)) / SpiralInterval;
		MinCount = 3.0f * 3.0f / SpiralInterval; MaxCount = 8.0f * 6.0f / SpiralInterval;
		break;
	case EMatsuriBossSkill::Pursuit:
		Low = 24.0f; High = 18.0f;
		PatternCount = FMath::RoundToFloat(FMath::Lerp(3.0f, 7.0f, P(1))) * PursuitPerVolley;
		MinCount = 3.0f * PursuitPerVolley; MaxCount = 7.0f * PursuitPerVolley;
		break;
	case EMatsuriBossSkill::TailSweep:
		Low = 36.0f; High = 28.0f;
		return P(3) < 0.5f ? Low : High;
	case EMatsuriBossSkill::FoxRain:
		Low = 10.0f; High = 6.0f;
		PatternCount = FMath::Lerp(30.0f, 90.0f, P(0)) * FMath::Lerp(3.0f, 6.0f, P(1));
		MinCount = 30.0f * 3.0f; MaxCount = 90.0f * 6.0f;
		break;
	case EMatsuriBossSkill::Phantom:
		Low = 32.0f; High = 22.0f;
		PatternCount = FMath::RoundToFloat(FMath::Lerp(2.0f, 4.0f, P(0))) * FMath::RoundToFloat(FMath::Lerp(2.0f, 5.0f, P(1))) * PhantomPerVolley;
		MinCount = 2.0f * 2.0f * PhantomPerVolley; MaxCount = 4.0f * 5.0f * PhantomPerVolley;
		break;
	default: break;
	}
	const float T = FMath::Clamp((PatternCount - MinCount) / FMath::Max(MaxCount - MinCount, 1.0f), 0.0f, 1.0f);
	return FMath::Max(0.0f, FMath::Lerp(Low, High, T));
}

const TCHAR* Matsuri::BossSkills::Id(EMatsuriBossSkill Skill)
{
	switch (Skill)
	{
	case EMatsuriBossSkill::Ring:		return TEXT("fox_ring");
	case EMatsuriBossSkill::Spiral:		return TEXT("fox_spiral");
	case EMatsuriBossSkill::Pursuit:	return TEXT("onibi_pursuit");
	case EMatsuriBossSkill::TailSweep:	return TEXT("tail_sweep");
	case EMatsuriBossSkill::FoxRain:	return TEXT("fox_rain");
	case EMatsuriBossSkill::Phantom:	return TEXT("phantom_step");
	case EMatsuriBossSkill::Count:		break;
	}
	return TEXT("");
}

bool Matsuri::BossSkills::FromId(const FString& InId, EMatsuriBossSkill& OutSkill)
{
	for (int32 i = 0; i < Count; ++i)
	{
		if (InId.Equals(Id(static_cast<EMatsuriBossSkill>(i)), ESearchCase::IgnoreCase))
		{
			OutSkill = static_cast<EMatsuriBossSkill>(i);
			return true;
		}
	}
	return false;
}

FText Matsuri::BossSkills::DisplayName(EMatsuriBossSkill Skill)
{
	switch (Skill)
	{
	case EMatsuriBossSkill::Ring:		return LOCTEXT("Ring", "狐火・環");
	case EMatsuriBossSkill::Spiral:		return LOCTEXT("Spiral", "狐火・螺旋");
	case EMatsuriBossSkill::Pursuit:	return LOCTEXT("Pursuit", "鬼火・追");
	case EMatsuriBossSkill::TailSweep:	return LOCTEXT("TailSweep", "九尾薙ぎ");
	case EMatsuriBossSkill::FoxRain:	return LOCTEXT("FoxRain", "狐の雨");
	case EMatsuriBossSkill::Phantom:	return LOCTEXT("Phantom", "幻影・瞬身");
	case EMatsuriBossSkill::Count:		break;
	}
	return FText::GetEmpty();
}

const TCHAR* Matsuri::BossSkills::ParamName(EMatsuriBossSkill Skill, int32 Index)
{
	static const TCHAR* const Names[Count][MatsuriBossParamCount] = {
		{ TEXT("density"), TEXT("speed"), TEXT("rings"), TEXT("gap") },
		{ TEXT("arms"), TEXT("turn_rate"), TEXT("speed"), TEXT("seconds") },
		{ TEXT("spread"), TEXT("volleys"), TEXT("homing"), TEXT("speed") },
		{ TEXT("height"), TEXT("speed"), TEXT("direction"), TEXT("sweeps") },
		{ TEXT("density"), TEXT("seconds"), TEXT("radius"), TEXT("fall_speed") },
		{ TEXT("phantoms"), TEXT("volleys"), TEXT("speed"), TEXT("spread") },
	};
	return Names[FMath::Clamp(static_cast<int32>(Skill), 0, Count - 1)][FMath::Clamp(Index, 0, MatsuriBossParamCount - 1)];
}

const TCHAR* Matsuri::BossSkills::ParamMeaning(EMatsuriBossSkill Skill, int32 Index)
{
	static const TCHAR* const Meanings[Count][MatsuriBossParamCount] = {
		{ TEXT("bullets per ring, 24..56"), TEXT("450..950 cm/s"), TEXT("3..7 rings"), TEXT("opening in each ring, 0..60 degrees (it rotates ring to ring)") },
		{ TEXT("3..8 arms"), TEXT("60..200 degrees/s"), TEXT("400..850 cm/s"), TEXT("3..6 s") },
		{ TEXT("fan width 20..80 degrees"), TEXT("3..7 volleys of 9"), TEXT("0..60 degrees/s for 1.2 s"), TEXT("800..1300 cm/s") },
		{ TEXT("<0.5 = low wall at ground level (dodge: stand on a platform or behind tall cover), >=0.5 = high wall at platform level (dodge: stay on the ground or the roofs)"), TEXT("700..1300 cm/s"), TEXT("direction the wall travels, 0..1 = 0..360 degrees"), TEXT("<0.5 = one wall, >=0.5 = two walls from opposite sides") },
		{ TEXT("30..90 drops per second"), TEXT("3..6 s"), TEXT("300..700 cm"), TEXT("800..1300 cm/s") },
		{ TEXT("2..4 phantoms around the player"), TEXT("2..5 volleys"), TEXT("700..1200 cm/s"), TEXT("0..40 degrees") },
	};
	return Meanings[FMath::Clamp(static_cast<int32>(Skill), 0, Count - 1)][FMath::Clamp(Index, 0, MatsuriBossParamCount - 1)];
}

const TCHAR* Matsuri::BossSkills::Description(EMatsuriBossSkill Skill)
{
	switch (Skill)
	{
	case EMatsuriBossSkill::Ring:		return TEXT("Rings of foxfire widen from her, tilted to the player's height; the player must find the rotating gap.");
	case EMatsuriBossSkill::Spiral:		return TEXT("Turning spiral arms; the player must keep circling or change height.");
	case EMatsuriBossSkill::Pursuit:	return TEXT("Aimed fans that home briefly; punishes standing still or hiding in one place in the open.");
	case EMatsuriBossSkill::TailSweep:	return TEXT("A wall of bullets crosses the whole courtyard at one height; only the right height (or tall cover for the low wall) is safe.");
	case EMatsuriBossSkill::FoxRain:	return TEXT("Rain falls on one zone after a short warning; drives the player out of a cover or off a platform (target = zone index, -1 = where the player stands).");
	case EMatsuriBossSkill::Phantom:	return TEXT("She blinks to an anchor (target = anchor index, -1 = farthest from the player) and phantoms fire from the player's flanks; cover facing her no longer protects. Phase 3 only.");
	case EMatsuriBossSkill::Count:		break;
	}
	return TEXT("");
}

bool Matsuri::BossSkills::AllowedIn(EMatsuriBossSkill Skill, int32 Phase)
{
	switch (Skill)
	{
	case EMatsuriBossSkill::Ring:
	case EMatsuriBossSkill::Spiral:
	case EMatsuriBossSkill::Pursuit:	return Phase >= 1;
	case EMatsuriBossSkill::TailSweep:
	case EMatsuriBossSkill::FoxRain:	return Phase >= 2;
	case EMatsuriBossSkill::Phantom:	return Phase >= 3;
	case EMatsuriBossSkill::Count:		break;
	}
	return false;
}

bool Matsuri::BossSkills::Clamp(FMatsuriBossAction& Action, int32 Phase, int32 ZoneCount, int32 AnchorCount)
{
	if (static_cast<int32>(Action.Skill) >= Count || !AllowedIn(Action.Skill, Phase))
	{
		return false;
	}
	for (float& Value : Action.Params)
	{
		Value = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 1.0f) : 0.5f;
	}
	const int32 Targets = Action.Skill == EMatsuriBossSkill::FoxRain ? ZoneCount : Action.Skill == EMatsuriBossSkill::Phantom ? AnchorCount : 0;
	Action.Target = Action.Target >= 0 && Action.Target < Targets ? Action.Target : -1;
	Action.Taunt = Action.Taunt.Left(MaxTauntChars);
	Action.Why = Action.Why.Left(MaxWhyChars);
	return true;
}

FMatsuriBossAction Matsuri::BossSkills::Preset(EMatsuriBossSkill Skill, int32 Preset)
{
	static const float Presets[Count][PresetsPerSkill][MatsuriBossParamCount] = {
		{ { 0.3f, 0.4f, 0.4f, 0.6f }, { 0.7f, 0.5f, 0.5f, 0.3f }, { 0.5f, 0.8f, 0.7f, 0.2f } },	// ring: open / dense / fast
		{ { 0.3f, 0.4f, 0.5f, 0.4f }, { 0.6f, 0.7f, 0.4f, 0.6f }, { 0.9f, 0.3f, 0.6f, 0.5f } },	// spiral
		{ { 0.3f, 0.4f, 0.2f, 0.5f }, { 0.6f, 0.6f, 0.6f, 0.5f }, { 0.9f, 0.8f, 0.4f, 0.8f } },	// pursuit
		{ { 0.2f, 0.4f, 0.0f, 0.2f }, { 0.8f, 0.5f, 0.25f, 0.2f }, { 0.2f, 0.6f, 0.5f, 0.8f } },	// sweep: low / high / low twice
		{ { 0.4f, 0.4f, 0.5f, 0.4f }, { 0.7f, 0.5f, 0.3f, 0.6f }, { 0.5f, 0.8f, 0.8f, 0.5f } },	// rain: on the player's zone
		{ { 0.2f, 0.4f, 0.4f, 0.3f }, { 0.6f, 0.6f, 0.6f, 0.5f }, { 1.0f, 0.5f, 0.7f, 0.8f } },	// phantoms
	};
	FMatsuriBossAction Action;
	Action.Skill = Skill;
	const int32 SkillIndex = FMath::Clamp(static_cast<int32>(Skill), 0, Count - 1);
	const int32 PresetIndex = FMath::Clamp(Preset, 0, PresetsPerSkill - 1);
	for (int32 i = 0; i < MatsuriBossParamCount; ++i)
	{
		Action.Params[i] = Presets[SkillIndex][PresetIndex][i];
	}
	return Action;
}

float Matsuri::BossSkills::Duration(const FMatsuriBossAction& Action)
{
	switch (Action.Skill)
	{
	case EMatsuriBossSkill::Ring:		return TelegraphSeconds + (ParamCountOf(Action, 2, 3, 7) - 1) * RingInterval;
	case EMatsuriBossSkill::Spiral:		return TelegraphSeconds + Param(Action, 3, 3.0f, 6.0f);
	case EMatsuriBossSkill::Pursuit:	return TelegraphSeconds + (ParamCountOf(Action, 1, 3, 7) - 1) * PursuitInterval;
	case EMatsuriBossSkill::TailSweep:	return TelegraphSeconds + (Action.Params[3] >= 0.5f ? SweepInterval : 0.0f);
	case EMatsuriBossSkill::FoxRain:	return TelegraphSeconds + RainWarnSeconds + Param(Action, 1, 3.0f, 6.0f);
	case EMatsuriBossSkill::Phantom:	return TelegraphSeconds + (ParamCountOf(Action, 1, 2, 5) - 1) * PhantomInterval;
	case EMatsuriBossSkill::Count:		break;
	}
	return TelegraphSeconds;
}

FLinearColor Matsuri::BossSkills::Color(EMatsuriBossSkill Skill)
{
	switch (Skill)
	{
	case EMatsuriBossSkill::Ring:		return FLinearColor(1.0f, 0.35f, 0.08f);
	case EMatsuriBossSkill::Spiral:		return FLinearColor(1.0f, 0.22f, 0.65f);
	case EMatsuriBossSkill::Pursuit:	return FLinearColor(0.35f, 0.7f, 1.0f);
	case EMatsuriBossSkill::TailSweep:	return FLinearColor(1.0f, 0.8f, 0.2f);
	case EMatsuriBossSkill::FoxRain:	return FLinearColor(0.3f, 1.0f, 0.85f);
	case EMatsuriBossSkill::Phantom:	return FLinearColor(0.62f, 0.3f, 1.0f);
	case EMatsuriBossSkill::Count:		break;
	}
	return FLinearColor::White;
}

FMatsuriSkillProgram Matsuri::BossSkills::Begin(const FMatsuriBossAction& Action, const FMatsuriBossView& View)
{
	FMatsuriSkillProgram Program;
	Program.Action = Action;
	Program.Duration = Duration(Action);
	Program.NextShot = TelegraphSeconds;
	const FVector PlayerGround(View.Player.X, View.Player.Y, View.GroundZ);
	switch (Action.Skill)
	{
	case EMatsuriBossSkill::TailSweep:
		Program.Heading = FRotator(0.0f, Action.Params[2] * 360.0f, 0.0f).Vector();
		break;
	case EMatsuriBossSkill::FoxRain:
		Program.Center = View.Zones.IsValidIndex(Action.Target) ? View.Zones[Action.Target] : PlayerGround;
		Program.NextShot = TelegraphSeconds + RainWarnSeconds;
		break;
	case EMatsuriBossSkill::Phantom:
	{
		const int32 Phantoms = ParamCountOf(Action, 0, 2, 4);
		const float Side = (View.Player - View.Boss).Rotation().Yaw + 90.0f;	// her flank, not her front
		for (int32 i = 0; i < Phantoms; ++i)
		{
			const float Yaw = Side + 180.0f * i / FMath::Max(Phantoms - 1, 1);
			Program.Phantoms.Add(View.Player + FRotator(0.0f, Yaw, 0.0f).Vector() * PhantomRadius + FVector(0.0f, 0.0f, 60.0f));
		}
		break;
	}
	default:
		break;
	}
	return Program;
}

bool Matsuri::BossSkills::Advance(FMatsuriSkillProgram& Program, float Dt, const FMatsuriBossView& View, FRandomStream& Random,
	TArray<FMatsuriBullet>& Out)
{
	Program.Time += Dt;
	const FMatsuriBossAction& Action = Program.Action;
	const float Speedup = PhaseSpeed(View.Phase);
	switch (Action.Skill)
	{
	case EMatsuriBossSkill::Ring:
	{
		const int32 Bullets = ParamCountOf(Action, 0, 24, 56);
		const float Speed = Param(Action, 1, 450.0f, 950.0f) * Speedup;
		const float Gap = Param(Action, 3, 0.0f, 60.0f);
		const FMatsuriBullet Shape = Template(Action.Skill, EMatsuriBulletStyle::Orb, 16.0f, 9.0f, BulletDamageFor(Action));
		const float ToPlayer = (View.Player - View.Boss).Rotation().Yaw;
		FireDue(Program, ParamCountOf(Action, 2, 3, 7), RingInterval, [&](int32 Shot)
		{
			Ring(Out, Shape, View.Boss, View.Player, Bullets, Speed, Shot * 180.0f / Bullets, Gap, ToPlayer + 50.0f * Shot);
		});
		break;
	}
	case EMatsuriBossSkill::Spiral:
	{
		const int32 Arms = ParamCountOf(Action, 0, 3, 8);
		const float Turn = Param(Action, 1, 60.0f, 200.0f);
		const float Speed = Param(Action, 2, 400.0f, 850.0f) * Speedup;
		const int32 Shots = FMath::FloorToInt32(Param(Action, 3, 3.0f, 6.0f) / SpiralInterval);
		const FMatsuriBullet Shape = Template(Action.Skill, EMatsuriBulletStyle::Orb, 14.0f, 9.0f, BulletDamageFor(Action));
		FireDue(Program, Shots, SpiralInterval, [&](int32 Shot)
		{
			const float Spin = Turn * Shot * SpiralInterval;
			for (int32 Arm = 0; Arm < Arms; ++Arm)
			{
				FMatsuriBullet& Bullet = Out.Add_GetRef(Shape);
				Bullet.Position = View.Boss;
				Bullet.Velocity = ConeDirection(View.Boss, View.Player, Spin + 360.0f * Arm / Arms) * Speed;
			}
		});
		break;
	}
	case EMatsuriBossSkill::Pursuit:
	{
		FMatsuriBullet Shape = Template(Action.Skill, EMatsuriBulletStyle::Rice, 10.0f, 5.0f, BulletDamageFor(Action));
		Shape.HomingDegreesPerSecond = Param(Action, 2, 0.0f, 60.0f);
		Shape.HomingSeconds = PursuitHomingSeconds;
		const float Spread = Param(Action, 0, 20.0f, 80.0f);
		const float Speed = Param(Action, 3, 800.0f, 1300.0f) * Speedup;
		FireDue(Program, ParamCountOf(Action, 1, 3, 7), PursuitInterval, [&](int32)
		{
			Fan(Out, Shape, View.Boss, View.Player, PursuitPerVolley, Spread, Speed);
		});
		break;
	}
	case EMatsuriBossSkill::TailSweep:
	{
		const bool bHigh = Action.Params[0] >= 0.5f;
		const float Speed = Param(Action, 1, 700.0f, 1300.0f) * Speedup;
		FMatsuriBullet Shape = Template(Action.Skill, EMatsuriBulletStyle::Large, 24.0f, SweepTravel(View) / Speed + 0.5f, BulletDamageFor(Action));
		const FVector Mid(View.Center.X, View.Center.Y, View.GroundZ);
		FireDue(Program, Action.Params[3] >= 0.5f ? 2 : 1, SweepInterval, [&](int32 Shot)
		{
			const FVector Heading = Shot % 2 == 0 ? Program.Heading : -Program.Heading;
			const FVector Start = Mid - Heading * View.HalfExtent * 1.2f;
			Wall(Out, Shape, Start, Heading, View.HalfExtent * 2.9f, SweepSpacing, View.GroundZ,
				bHigh ? SweepHighMin : SweepLowMin, bHigh ? SweepHighMax : SweepLowMax, SweepRowSpacing, Speed);
		});
		break;
	}
	case EMatsuriBossSkill::FoxRain:
	{
		const float PerSecond = Param(Action, 0, 30.0f, 90.0f);
		const float Radius = Param(Action, 2, 300.0f, 700.0f);
		const float Fall = Param(Action, 3, 800.0f, 1300.0f) * Speedup;
		const int32 Steps = FMath::FloorToInt32(Param(Action, 1, 3.0f, 6.0f) / RainStep);
		const FMatsuriBullet Shape = Template(Action.Skill, EMatsuriBulletStyle::Orb, 13.0f, RainHeight / Fall + 0.6f, BulletDamageFor(Action));
		FireDue(Program, Steps, RainStep, [&](int32 Shot)
		{
			// Whole drops per step, carrying the fraction so the average rate is exact.
			const int32 Drops = FMath::FloorToInt32((Shot + 1) * PerSecond * RainStep) - FMath::FloorToInt32(Shot * PerSecond * RainStep);
			Rain(Out, Shape, Program.Center, Radius, RainHeight, Drops, Fall, Random);
		});
		break;
	}
	case EMatsuriBossSkill::Phantom:
	{
		const float Speed = Param(Action, 2, 700.0f, 1200.0f) * Speedup;
		const float Spread = Param(Action, 3, 0.0f, 40.0f);
		const FMatsuriBullet Shape = Template(Action.Skill, EMatsuriBulletStyle::Rice, 10.0f, 4.0f, BulletDamageFor(Action));
		FireDue(Program, ParamCountOf(Action, 1, 2, 5), PhantomInterval, [&](int32)
		{
			for (const FVector& From : Program.Phantoms)
			{
				Fan(Out, Shape, From, View.Player, PhantomPerVolley, Spread, Speed);
			}
		});
		break;
	}
	case EMatsuriBossSkill::Count:
		return false;
	}
	return Program.Time < Program.Duration + KINDA_SMALL_NUMBER;
}

#undef LOCTEXT_NAMESPACE
