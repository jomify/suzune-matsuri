// Suzune's Night Festival - the boss fight's pure parts: bullets, the six skills, the courtyard.
// Run: Automation RunTests SuzuneMatsuri.Boss

#include "Misc/AutomationTest.h"
#include "MatsuriBossArena.h"
#include "MatsuriBossSkills.h"
#include "MatsuriDanmaku.h"
#include "MatsuriDanmakuField.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr float Dt = 1.0f / 60.0f;

	FMatsuriBossView TestView()
	{
		const FMatsuriBossArena Arena = Matsuri::BossArena::Layout();
		FMatsuriBossView View;
		View.Center = Arena.Center;
		View.GroundZ = Arena.GroundZ;
		View.HalfExtent = Arena.HalfExtent;
		View.Boss = Arena.Anchors[0];
		View.Player = FVector(0.0f, -1500.0f, 130.0f);
		for (const FMatsuriArenaZone& Zone : Arena.Zones)
		{
			View.Zones.Add(Zone.Point);
		}
		View.Anchors = Arena.Anchors;
		View.Phase = 3;
		return View;
	}

	/** Runs a skill to its end; returns every bullet it fired. */
	TArray<FMatsuriBullet> RunSkill(const FMatsuriBossAction& Action, const FMatsuriBossView& View)
	{
		FRandomStream Random(7);
		FMatsuriSkillProgram Program = Matsuri::BossSkills::Begin(Action, View);
		TArray<FMatsuriBullet> Out;
		int32 Guard = 0;
		while (Matsuri::BossSkills::Advance(Program, Dt, View, Random, Out) && ++Guard < 60 * 30) {}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriDanmakuShapesTest, "SuzuneMatsuri.Boss.DanmakuShapes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriDanmakuShapesTest::RunTest(const FString&)
{
	using namespace Matsuri::Danmaku;
	const FMatsuriBullet Shape;
	const FVector Origin(0.0f, 0.0f, 800.0f);
	const FVector Target(1000.0f, 0.0f, 130.0f);

	TArray<FMatsuriBullet> Ring24;
	Ring(Ring24, Shape, Origin, Target, 24, 800.0f, 0.0f, 0.0f, 0.0f);
	TestEqual(TEXT("a closed ring has every bullet"), Ring24.Num(), 24);
	TArray<FMatsuriBullet> Gapped;
	Ring(Gapped, Shape, Origin, Target, 24, 800.0f, 0.0f, 60.0f, 0.0f);
	TestTrue(TEXT("a 60 degree gap removes 3-5 of 24 bullets"), Gapped.Num() >= 19 && Gapped.Num() <= 21);

	// The ring is tilted so it passes through the player's height at the player's distance.
	FMatsuriBullet Aimed = Ring24[0];
	for (int32 i = 0; i < 600 && FVector::Dist2D(Aimed.Position, Origin) < 1000.0f; ++i)
	{
		Step(Aimed, Dt, Target);
	}
	TestTrue(FString::Printf(TEXT("ring reaches the target height at its distance (z %.0f)"), Aimed.Position.Z), FMath::Abs(Aimed.Position.Z - Target.Z) < 40.0f);

	TArray<FMatsuriBullet> Fan7;
	Fan(Fan7, Shape, Origin, Target, 7, 60.0f, 1000.0f);
	TestEqual(TEXT("fan count"), Fan7.Num(), 7);
	TestTrue(TEXT("the fan's middle bullet flies at the target"), FVector::DotProduct(Fan7[3].Velocity.GetSafeNormal(), (Target - Origin).GetSafeNormal()) > 0.999f);

	TArray<FMatsuriBullet> WallOut;
	Wall(WallOut, Shape, FVector(-3000, 0, 0), FVector(1, 0, 0), 900.0f, 90.0f, 0.0f, 40.0f, 200.0f, 80.0f, 1000.0f);
	TestEqual(TEXT("a 9 m wall of 3 rows at 90 cm spacing"), WallOut.Num(), 11 * 3);
	const float Gap = 90.0f - 2.0f * 24.0f;
	TestTrue(TEXT("the sweep's gaps are narrower than the player's hit sphere"), Gap < 2.0f * AMatsuriDanmakuField::PlayerHitRadius);

	FRandomStream Random(3);
	TArray<FMatsuriBullet> Drops;
	Rain(Drops, Shape, FVector(500, 500, 0), 400.0f, 1400.0f, 50, 1000.0f, Random);
	TestEqual(TEXT("rain count"), Drops.Num(), 50);
	bool bInDisc = true;
	for (const FMatsuriBullet& Drop : Drops)
	{
		bInDisc &= FVector::Dist2D(Drop.Position, FVector(500, 500, 0)) <= 400.0f && FMath::IsNearlyEqual(Drop.Position.Z, 1400.0f) && Drop.Velocity.Z < 0.0f;
	}
	TestTrue(TEXT("every drop starts above the disc and falls"), bInDisc);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriDanmakuMotionTest, "SuzuneMatsuri.Boss.BulletMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriDanmakuMotionTest::RunTest(const FString&)
{
	using namespace Matsuri::Danmaku;
	FMatsuriBullet Waiting;
	Waiting.Velocity = FVector(1000, 0, 0);
	Waiting.Delay = 0.5f;
	Step(Waiting, 0.25f, FVector::ZeroVector);
	TestTrue(TEXT("a delayed bullet waits in place"), Waiting.Position.IsNearlyZero());

	FMatsuriBullet Homing;
	Homing.Velocity = FVector(1000, 0, 0);
	Homing.HomingDegreesPerSecond = 60.0f;
	Homing.HomingSeconds = 1.0f;
	const FVector Behind(0, 5000, 0);	// 90 degrees off
	Step(Homing, 0.5f, Behind);
	const float Turned = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Homing.Velocity.GetSafeNormal(), FVector::ForwardVector)));
	TestTrue(FString::Printf(TEXT("homing turns at most its rate (%.1f degrees in 0.5 s)"), Turned), Turned <= 30.5f && Turned > 29.0f);
	for (int32 i = 0; i < 120; ++i)
	{
		Step(Homing, Dt, Behind);
	}
	const FVector Heading = Homing.Velocity.GetSafeNormal();
	Step(Homing, Dt, -Behind);
	TestTrue(TEXT("after its homing time it flies straight"), FVector::DotProduct(Homing.Velocity.GetSafeNormal(), Heading) > 0.9999f);

	FMatsuriBullet Old;
	Old.Lifetime = 1.0f;
	Step(Old, 1.1f, FVector::ZeroVector);
	TestTrue(TEXT("a bullet dies at the end of its life"), Old.bDead);

	FMatsuriBullet Near;
	Near.Radius = 10.0f;
	Near.Position = FVector(31, 0, 0);
	TestTrue(TEXT("touches when the spheres overlap"), Touches(Near, FVector::ZeroVector, 22.0f));
	Near.Position = FVector(33, 0, 0);
	TestFalse(TEXT("misses just outside"), Touches(Near, FVector::ZeroVector, 22.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriFastBulletCrossingHitsTest, "SuzuneMatsuri.Boss.FastBulletCrossingHits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriFastBulletCrossingHitsTest::RunTest(const FString&)
{
	FMatsuriBullet Bullet;
	Bullet.Radius = 10.0f;
	Bullet.Position = FVector(-37.375f, 0.0f, 0.0f);
	Bullet.Velocity = FVector(1495.0f, 0.0f, 0.0f);
	const FVector Player = FVector::ZeroVector;
	TestFalse(TEXT("the bullet starts outside the player's hit sphere"), Matsuri::Danmaku::Touches(Bullet, Player, 22.0f));
	Matsuri::Danmaku::Step(Bullet, 0.05f, Player);
	TestTrue(TEXT("the bullet crosses the player's hit sphere during a slow frame"), Matsuri::Danmaku::Touches(Bullet, Player, 22.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriDanmakuRelativeMotionTest, "SuzuneMatsuri.Boss.PlayerMotionCrossesBullet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriDanmakuRelativeMotionTest::RunTest(const FString&)
{
	FMatsuriBullet Bullet;
	Bullet.Radius = 10.0f;
	Matsuri::Danmaku::Step(Bullet, Dt, FVector(40, 0, 0));
	float Fraction = 0.0f;
	TestTrue(TEXT("a moving player cannot cross a stationary bullet between samples"),
		Matsuri::Danmaku::PlayerImpact(Bullet, FVector(-40, 0, 0), FVector(40, 0, 0), 22.0f, Fraction));
	TestTrue(TEXT("the impact is the first contact on the relative segment"), FMath::IsNearlyEqual(Fraction, 0.1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriDanmakuTelegraphTest, "SuzuneMatsuri.Boss.TelegraphDoesNotDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriDanmakuTelegraphTest::RunTest(const FString&)
{
	FMatsuriBullet Bullet;
	Bullet.Delay = 0.5f;
	Bullet.Velocity = FVector(1000, 0, 0);
	Matsuri::Danmaku::Step(Bullet, 0.4f, FVector::ZeroVector);
	TestFalse(TEXT("a frozen telegraph is not a live hit"), Matsuri::Danmaku::Touches(Bullet, FVector::ZeroVector, 22.0f));
	Matsuri::Danmaku::Step(Bullet, 0.2f, FVector::ZeroVector);
	TestTrue(TEXT("crossing the delay advances only the active tenth of a second"), FMath::IsNearlyEqual(Bullet.Position.X, 100.0, 0.001));
	TestTrue(TEXT("the live segment starts halfway through this frame"), FMath::IsNearlyEqual(Bullet.ActiveStartFraction, 0.5f, 1.e-5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriDanmakuCoverSweepTest, "SuzuneMatsuri.Boss.SweptCoverOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriDanmakuCoverSweepTest::RunTest(const FString&)
{
	FMatsuriBullet Bullet;
	Bullet.Radius = 10.0f;
	Bullet.Position = FVector(-120, 0, 0);
	Bullet.Velocity = FVector(2400, 0, 0);
	Matsuri::Danmaku::Step(Bullet, 0.1f, FVector::ZeroVector);
	float PlayerFraction = 0.0f, CoverFraction = 0.0f;
	TestTrue(TEXT("the full segment reaches the player"), Matsuri::Danmaku::PlayerImpact(Bullet, FVector::ZeroVector, FVector::ZeroVector, 22.0f, PlayerFraction));
	const FBox InFront(FVector(-80, -50, -50), FVector(-60, 50, 50));
	TestTrue(TEXT("a thin cover cannot be tunneled through"), Matsuri::Danmaku::BoxImpact(Bullet, InFront, CoverFraction));
	TestTrue(TEXT("cover in front wins the impact order"), CoverFraction < PlayerFraction);
	const FBox Behind(FVector(60, -50, -50), FVector(80, 50, 50));
	TestTrue(TEXT("the segment also reaches cover behind the player"), Matsuri::Danmaku::BoxImpact(Bullet, Behind, CoverFraction));
	TestTrue(TEXT("cover behind does not cancel an earlier player impact"), PlayerFraction < CoverFraction);
	const FBox Beside(FVector(-80, 30, -50), FVector(-60, 50, 50));
	TestFalse(TEXT("a parallel segment outside the expanded box misses"), Matsuri::Danmaku::BoxImpact(Bullet, Beside, CoverFraction));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossSkillTableTest, "SuzuneMatsuri.Boss.SkillTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriBossSkillTableTest::RunTest(const FString&)
{
	using namespace Matsuri::BossSkills;
	TestEqual(TEXT("six skills"), Count, 6);
	for (int32 i = 0; i < Count; ++i)
	{
		const EMatsuriBossSkill Skill = static_cast<EMatsuriBossSkill>(i);
		EMatsuriBossSkill Back;
		TestTrue(FString::Printf(TEXT("%s round-trips by id"), Id(Skill)), FromId(Id(Skill), Back) && Back == Skill);
		TestFalse(TEXT("has a display name"), DisplayName(Skill).IsEmpty());
		for (int32 p = 0; p < PresetsPerSkill; ++p)
		{
			const FMatsuriBossAction Action = Preset(Skill, p);
			TestTrue(TEXT("preset params are 0..1"), Action.Params[0] >= 0 && Action.Params[3] <= 1 && Action.Skill == Skill);
		}
	}
	EMatsuriBossSkill Unknown;
	TestFalse(TEXT("unknown ids are refused"), FromId(TEXT("summon_meteor"), Unknown));

	TestTrue(TEXT("phase 1: foxfire only"), AllowedIn(EMatsuriBossSkill::Spiral, 1) && !AllowedIn(EMatsuriBossSkill::TailSweep, 1) && !AllowedIn(EMatsuriBossSkill::Phantom, 1));
	TestTrue(TEXT("phase 2 adds sweep and rain"), AllowedIn(EMatsuriBossSkill::FoxRain, 2) && !AllowedIn(EMatsuriBossSkill::Phantom, 2));
	TestTrue(TEXT("phase 3 has all six"), AllowedIn(EMatsuriBossSkill::Phantom, 3));

	FMatsuriBossAction Wild;
	Wild.Skill = EMatsuriBossSkill::FoxRain;
	Wild.Params[0] = 7.0f;
	Wild.Params[1] = -3.0f;
	Wild.Params[2] = NAN;
	Wild.Target = 99;
	Wild.Taunt = FString::ChrN(200, TEXT('x'));
	TestFalse(TEXT("a skill not yet allowed is refused"), Clamp(Wild, 1, 18, 6));
	TestTrue(TEXT("allowed in phase 2"), Clamp(Wild, 2, 18, 6));
	TestTrue(TEXT("params clamped to 0..1, NaN to the middle"), Wild.Params[0] == 1.0f && Wild.Params[1] == 0.0f && Wild.Params[2] == 0.5f);
	TestEqual(TEXT("an out-of-range zone becomes 'where the player stands'"), Wild.Target, -1);
	TestTrue(TEXT("long taunts are cut"), Wild.Taunt.Len() <= 40);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossBulletDamageTableTest, "SuzuneMatsuri.Boss.BulletDamageByDensity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriBossBulletDamageTableTest::RunTest(const FString&)
{
	for (int32 i = 0; i < Matsuri::BossSkills::Count; ++i)
	{
		const EMatsuriBossSkill Skill = static_cast<EMatsuriBossSkill>(i);
		FMatsuriBossAction Sparse = Matsuri::BossSkills::Preset(Skill, 0);
		for (float& Value : Sparse.Params) { Value = 0.0f; }
		if (Skill == EMatsuriBossSkill::Ring) { Sparse.Params[3] = 1.0f; }
		FMatsuriBossAction Dense = Sparse;
		Dense.Params[0] = 1.0f;
		Dense.Params[1] = 1.0f;
		Dense.Params[2] = 1.0f;
		Dense.Params[3] = 1.0f;
		const float SparseDamage = Matsuri::BossSkills::BulletDamageFor(Sparse);
		const float DenseDamage = Matsuri::BossSkills::BulletDamageFor(Dense);
		const float ExpectedSparse[] = { 14.0f, 12.0f, 24.0f, 36.0f, 10.0f, 32.0f };
		const float ExpectedDense[] = { 10.0f, 8.0f, 18.0f, 28.0f, 6.0f, 22.0f };
		TestTrue(FString::Printf(TEXT("%s damage is finite and positive"), Matsuri::BossSkills::Id(Skill)),
			FMath::IsFinite(SparseDamage) && SparseDamage > 0.0f && FMath::IsFinite(DenseDamage) && DenseDamage > 0.0f);
		TestTrue(FString::Printf(TEXT("%s dense pattern has no stronger bullet"), Matsuri::BossSkills::Id(Skill)), DenseDamage <= SparseDamage);
		TestTrue(FString::Printf(TEXT("%s sparse value is fixed"), Matsuri::BossSkills::Id(Skill)), FMath::IsNearlyEqual(SparseDamage, ExpectedSparse[i]));
		if (Skill == EMatsuriBossSkill::Ring) { Dense.Params[3] = 0.0f; }
		TestTrue(FString::Printf(TEXT("%s dense value is fixed"), Matsuri::BossSkills::Id(Skill)),
			FMath::IsNearlyEqual(Matsuri::BossSkills::BulletDamageFor(Dense), ExpectedDense[i]));
	}
	// Increase each independent count axis while holding all others fixed.
	for (const auto Skill : { EMatsuriBossSkill::Ring, EMatsuriBossSkill::Spiral, EMatsuriBossSkill::Pursuit, EMatsuriBossSkill::FoxRain, EMatsuriBossSkill::Phantom })
	{
		for (int32 Axis = 0; Axis < 4; ++Axis)
		{
			const bool AffectsCount = Skill == EMatsuriBossSkill::Ring ? (Axis == 0 || Axis == 2)
				: Skill == EMatsuriBossSkill::Spiral ? (Axis == 0 || Axis == 3)
				: Skill == EMatsuriBossSkill::Pursuit ? Axis == 1 : (Axis == 0 || Axis == 1);
			if (!AffectsCount) { continue; }
			FMatsuriBossAction Light = Matsuri::BossSkills::Preset(Skill, 0);
			Light.Params[Axis] = 0.0f;
			FMatsuriBossAction Dense = Light;
			Dense.Params[Axis] = 1.0f;
			TestTrue(TEXT("increasing a count axis reduces individual bullet damage"), Matsuri::BossSkills::BulletDamageFor(Dense) < Matsuri::BossSkills::BulletDamageFor(Light));
		}
	}
	FMatsuriBossAction Wall = Matsuri::BossSkills::Preset(EMatsuriBossSkill::TailSweep, 0);
	Wall.Params[3] = 0.49f;
	TestEqual(TEXT("one wall is a 36 HP projectile"), Matsuri::BossSkills::BulletDamageFor(Wall), 36.0f);
	Wall.Params[3] = 0.5f;
	TestEqual(TEXT("two walls use 28 HP projectiles"), Matsuri::BossSkills::BulletDamageFor(Wall), 28.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossSkillProgramsTest, "SuzuneMatsuri.Boss.SkillPrograms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriBossSkillProgramsTest::RunTest(const FString&)
{
	using namespace Matsuri::BossSkills;
	const FMatsuriBossView View = TestView();
	for (int32 i = 0; i < Count; ++i)
	{
		const EMatsuriBossSkill Skill = static_cast<EMatsuriBossSkill>(i);
		FMatsuriBossAction Busiest = Preset(Skill, 0);
		for (float& Param : Busiest.Params)
		{
			Param = 1.0f;	// the most bullets every parameter allows
		}
		const TArray<FMatsuriBullet> Fired = RunSkill(Busiest, View);
		TestTrue(FString::Printf(TEXT("%s fires"), Id(Skill)), Fired.Num() > 0);
		TestTrue(FString::Printf(TEXT("%s stays inside the bullet budget (%d)"), Id(Skill), Fired.Num()), Fired.Num() <= AMatsuriDanmakuField::MaxBullets);
		bool bTagged = true;
		for (const FMatsuriBullet& Bullet : Fired)
		{
			bTagged &= Bullet.Skill == static_cast<uint8>(Skill);
		}
		TestTrue(TEXT("bullets remember their skill"), bTagged);
		bool bDamageTagged = true;
		const float ExpectedDamage = BulletDamageFor(Busiest);
		for (const FMatsuriBullet& Bullet : Fired)
		{
			bDamageTagged &= FMath::IsNearlyEqual(Bullet.Damage, ExpectedDamage);
		}
		TestTrue(FString::Printf(TEXT("%s bullets carry action damage"), Id(Skill)), bDamageTagged);

		FRandomStream Random(1);
		FMatsuriSkillProgram Program = Begin(Preset(Skill, 1), View);
		TArray<FMatsuriBullet> Early;
		Advance(Program, TelegraphSeconds * 0.5f, View, Random, Early);
		TestEqual(FString::Printf(TEXT("%s waits for its telegraph"), Id(Skill)), Early.Num(), 0);
	}

	// Rain: the drop rate is the one asked for, on the chosen zone.
	FMatsuriBossAction Rain = Preset(EMatsuriBossSkill::FoxRain, 0);
	Rain.Params[0] = 0.5f;	// 60 a second
	Rain.Params[1] = 0.0f;	// 3 s
	Rain.Target = 2;
	const TArray<FMatsuriBullet> Drops = RunSkill(Rain, View);
	TestTrue(FString::Printf(TEXT("rain drops 60/s for 3 s (%d)"), Drops.Num()), FMath::Abs(Drops.Num() - 180) <= 4);
	TestTrue(TEXT("on the chosen zone"), Drops.Num() > 0 && FVector::Dist2D(Drops[0].Position, View.Zones[2]) < 800.0f);

	// The low sweep is at ground level, the high one at platform level.
	FMatsuriBossAction Low = Preset(EMatsuriBossSkill::TailSweep, 0);
	FMatsuriBossAction High = Preset(EMatsuriBossSkill::TailSweep, 1);
	const TArray<FMatsuriBullet> LowWall = RunSkill(Low, View);
	const TArray<FMatsuriBullet> HighWall = RunSkill(High, View);
	TestTrue(TEXT("low wall covers a standing player's chest"), LowWall.Num() > 0 && LowWall[0].Position.Z < 130.0f);
	TestTrue(TEXT("high wall is above a standing player"), HighWall.Num() > 0 && HighWall[0].Position.Z > 400.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossArenaLayoutTest, "SuzuneMatsuri.Boss.ArenaLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriBossArenaLayoutTest::RunTest(const FString&)
{
	using namespace Matsuri::BossArena;
	const FMatsuriBossArena Arena = Layout();
	auto InsideSolid = [&Arena](const FVector& Point)
	{
		for (const FMatsuriArenaBox& Solid : Arena.Solids)
		{
			if (Solid.Bounds().ExpandBy(-1.0f).IsInside(Point))
			{
				return true;
			}
		}
		return false;
	};
	TestFalse(TEXT("the player starts in the open"), InsideSolid(Arena.PlayerStart));
	for (const FVector& Anchor : Arena.Anchors)
	{
		TestFalse(TEXT("her anchors are in the open"), InsideSolid(Anchor));
	}
	for (const FVector& Spot : Arena.AmmoSpots)
	{
		TestFalse(TEXT("ammo is not inside anything"), InsideSolid(Spot + FVector(0, 0, 70)));
	}
	for (const FVector& Hook : Arena.HookPoints)
	{
		TestFalse(TEXT("hook rings hang in the open"), InsideSolid(Hook));
	}
	TestEqual(TEXT("six anchors"), Arena.Anchors.Num(), 6);

	TestEqual(TEXT("the middle of the courtyard is 'center'"), ZoneAt(Arena, FVector(100, 50, 0)), 0);
	const int32 Platform = ZoneAt(Arena, FVector(-1700, 300, 400));
	TestTrue(TEXT("standing on the west platform is that zone"), Arena.Zones.IsValidIndex(Platform) && Arena.Zones[Platform].Name == TEXT("platform_w"));
	const int32 Below = ZoneAt(Arena, FVector(-1700, 300, -0.0f));
	TestTrue(TEXT("the ground beside it is not the platform"), !Arena.Zones.IsValidIndex(Below) || !Arena.Zones[Below].bHigh);

	// Behind the south-west lantern, seen from her front anchor.
	TestTrue(TEXT("a stone lantern shields the player"), IsShielded(Arena, Arena.Anchors[0], FVector(-900, -800, 130)));
	TestFalse(TEXT("the open middle does not"), IsShielded(Arena, Arena.Anchors[0], FVector(0, 0, 130)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
