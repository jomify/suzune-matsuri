// Suzune's Night Festival - each kind of shot flies by its own physics (unit tests, no world).
// Playtest: "arrows and thrown knives should obey physics, and look different from each other".
// Run: Automation RunTests SuzuneMatsuri.Shots

#include "Misc/AutomationTest.h"
#include "MatsuriHeroes.h"
#include "MatsuriProjectile.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags MatsuriTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr float Gravity = 980.0f;			// cm/s^2, the engine default
	constexpr float CorridorCm = 1500.0f;
	constexpr float MaxCorridorDropCm = 30.0f;	// aiming stays fair: the drop across a corridor is under a hand's width
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriShotKindsTest, "SuzuneMatsuri.Shots.EachKindHasItsOwnPhysics", MatsuriTestFlags)
bool FMatsuriShotKindsTest::RunTest(const FString&)
{
	using namespace Matsuri::Shots;
	const FMatsuriBallistics Arrow = BallisticsFor(EMatsuriShotLook::Arrow);
	const FMatsuriBallistics Kunai = BallisticsFor(EMatsuriShotLook::Star);
	const FMatsuriBallistics Bell = BallisticsFor(EMatsuriShotLook::Orb);
	const FMatsuriBallistics Coin = BallisticsFor(EMatsuriShotLook::Coin);
	const FMatsuriBallistics Cucumber = BallisticsFor(EMatsuriShotLook::Cucumber);

	TestTrue(TEXT("arrows arc a little and fly point-first"), Arrow.GravityScale > 0.0f && Arrow.SpinDegreesPerSecond == 0.0f);
	TestTrue(TEXT("arrows stick where they land"), Arrow.WorldImpact == EMatsuriShotImpact::Stick);
	TestTrue(TEXT("kunai tumble end over end"), Kunai.SpinDegreesPerSecond > 0.0f && Kunai.SpinAxis == EMatsuriSpinAxis::EndOverEnd);
	TestTrue(TEXT("kunai stick where they land"), Kunai.WorldImpact == EMatsuriShotImpact::Stick);
	TestTrue(TEXT("bells are heavier than arrows"), Bell.GravityScale > Arrow.GravityScale);
	TestTrue(TEXT("bells bounce off walls"), Bell.WorldImpact == EMatsuriShotImpact::Bounce && Bell.WorldBounces >= 1 && Bell.Bounciness > 0.0f);
	TestTrue(TEXT("coins flip and bounce"), Coin.SpinDegreesPerSecond > 0.0f && Coin.WorldImpact == EMatsuriShotImpact::Bounce);
	TestTrue(TEXT("cucumbers are lobbed and splat"), Cucumber.GravityScale > 0.0f && Cucumber.WorldImpact == EMatsuriShotImpact::Vanish);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriShotDropTest, "SuzuneMatsuri.Shots.DropStaysFairAcrossACorridor", MatsuriTestFlags)
bool FMatsuriShotDropTest::RunTest(const FString&)
{
	const EMatsuriShotLook Looks[] = { EMatsuriShotLook::Arrow, EMatsuriShotLook::Star, EMatsuriShotLook::Orb };
	for (int32 i = 0; i < Matsuri::Heroes::Count; ++i)
	{
		const FMatsuriHeroDef& Hero = Matsuri::Heroes::Get(static_cast<EMatsuriHero>(i));
		const float Seconds = CorridorCm / Hero.Weapon.Speed;
		const float Drop = 0.5f * Gravity * Matsuri::Shots::BallisticsFor(Looks[i]).GravityScale * Seconds * Seconds;
		TestTrue(FString::Printf(TEXT("%s's shot drops %.1f cm over 15 m (<= %.0f)"), *Hero.Name.ToString(), Drop, MaxCorridorDropCm), Drop <= MaxCorridorDropCm);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriLobTest, "SuzuneMatsuri.Shots.LobLandsOnTheTarget", MatsuriTestFlags)
bool FMatsuriLobTest::RunTest(const FString&)
{
	// The kappa aims its cucumber higher so gravity brings it down on the target.
	const FVector From(0.0f, 0.0f, 100.0f);
	const FVector Targets[] = { FVector(800.0f, 0.0f, 100.0f), FVector(600.0f, 500.0f, 60.0f), FVector(1200.0f, -300.0f, 150.0f) };
	constexpr float Speed = 1400.0f;
	constexpr float GravityScale = 0.5f;
	for (const FVector& Target : Targets)
	{
		const FVector Direction = Matsuri::Shots::LobDirection(From, Target, Speed, GravityScale);
		TestTrue(TEXT("a unit direction"), FMath::IsNearlyEqual(Direction.Size(), 1.0f, 0.001f));
		// Fly it: closest approach to the target.
		FVector Location = From;
		FVector Velocity = Direction * Speed;
		float Closest = TNumericLimits<float>::Max();
		for (int32 Step = 0; Step < 2000; ++Step)
		{
			constexpr float Dt = 0.001f;
			Velocity.Z -= Gravity * GravityScale * Dt;
			Location += Velocity * Dt;
			Closest = FMath::Min(Closest, static_cast<float>(FVector::Dist(Location, Target)));
		}
		TestTrue(FString::Printf(TEXT("lands within 10 cm of %s (closest %.1f cm)"), *Target.ToCompactString(), Closest), Closest <= 10.0f);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
