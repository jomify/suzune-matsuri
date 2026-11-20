// Suzune's Night Festival - hero roster and ultimate charge unit tests.
// Run: Automation RunTests SuzuneMatsuri.Heroes

#include "Misc/AutomationTest.h"
#include "MatsuriRunState.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags MatsuriTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FMatsuriRunState RunWith(EMatsuriHero Hero)
	{
		FMatsuriRunState Run;
		Run.Reset(1, Hero);
		Run.BeginFloor(100);
		return Run;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriRosterTest, "SuzuneMatsuri.Heroes.EveryHeroIsComplete", MatsuriTestFlags)
bool FMatsuriRosterTest::RunTest(const FString&)
{
	TSet<FString> Names;
	for (int32 i = 0; i < Matsuri::Heroes::Count; ++i)
	{
		const FMatsuriHeroDef& Hero = Matsuri::Heroes::Get(static_cast<EMatsuriHero>(i));
		TestEqual(TEXT("id matches slot"), static_cast<int32>(Hero.Id), i);
		TestFalse(TEXT("has a name"), Hero.Name.IsEmpty());
		TestFalse(TEXT("has a weapon"), Hero.Weapon.Name.IsEmpty());
		TestFalse(TEXT("has a passive"), Hero.Passive.IsEmpty());
		TestTrue(TEXT("weapon holds ammo"), Hero.Weapon.AmmoCap > 0);
		for (const EMatsuriSkillSlot Slot : { EMatsuriSkillSlot::Shift, EMatsuriSkillSlot::E, EMatsuriSkillSlot::Q })
		{
			TestFalse(TEXT("every skill has a name"), Hero.Skill(Slot).Name.IsEmpty());
			TestFalse(TEXT("every skill is described"), Hero.Skill(Slot).Description.IsEmpty());
		}
		TestTrue(TEXT("Shift has a cooldown"), Hero.Skill(EMatsuriSkillSlot::Shift).Cooldown > 0.0f);
		TestTrue(TEXT("E has a cooldown"), Hero.Skill(EMatsuriSkillSlot::E).Cooldown > 0.0f);
		Names.Add(Hero.Name.ToString());
	}
	TestEqual(TEXT("three distinct heroines"), Names.Num(), Matsuri::Heroes::Count);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriPassivesTest, "SuzuneMatsuri.Heroes.PassivesShapeTheRun", MatsuriTestFlags)
bool FMatsuriPassivesTest::RunTest(const FString&)
{
	const FMatsuriRunState Sakura = RunWith(EMatsuriHero::Sakura);
	const FMatsuriRunState Suzune = RunWith(EMatsuriHero::Suzune);
	const FMatsuriRunState Mio = RunWith(EMatsuriHero::Mio);

	TestEqual(TEXT("sakura: +50 HP"), Sakura.GetMaxHealth(), Matsuri::Run::StartHealth + 50.0f);
	TestEqual(TEXT("suzune: normal HP"), Suzune.GetMaxHealth(), Matsuri::Run::StartHealth);
	TestTrue(TEXT("suzune carries the most ammo"), Suzune.GetAmmoCap() > Mio.GetAmmoCap() && Mio.GetAmmoCap() > Sakura.GetAmmoCap());
	TestEqual(TEXT("mio: ginka worth 3"), Mio.PointsFor(EMatsuriCollectible::Ginka), 3);
	TestEqual(TEXT("others: ginka worth 2"), Sakura.PointsFor(EMatsuriCollectible::Ginka), 2);
	TestTrue(TEXT("suzune is faster and double-jumps"), Suzune.GetHero().MoveSpeedMultiplier > 1.0f && Suzune.GetHero().JumpCount == 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriUltTest, "SuzuneMatsuri.Heroes.UltimateChargesAndSpends", MatsuriTestFlags)
bool FMatsuriUltTest::RunTest(const FString&)
{
	FMatsuriRunState Run = RunWith(EMatsuriHero::Sakura);
	TestFalse(TEXT("starts empty"), Run.IsUltReady());
	TestFalse(TEXT("cannot fire an empty ult"), Run.ConsumeUlt());

	Run.Collect(EMatsuriCollectible::Magatama);	// 5 points
	TestEqual(TEXT("collecting charges"), Run.UltCharge, 5 * Matsuri::Heroes::UltPerPoint);
	Run.AddDamageDealt(1000.0f);
	TestTrue(TEXT("damage charges, capped at full"), Run.IsUltReady() && Run.UltCharge == 1.0f);
	TestTrue(TEXT("ult fires when full"), Run.ConsumeUlt());
	TestEqual(TEXT("ult empties"), Run.UltCharge, 0.0f);
	Run.AddDamageDealt(-5.0f);
	TestEqual(TEXT("negative damage ignored"), Run.UltCharge, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriZeniTest, "SuzuneMatsuri.Heroes.ZeniSpendsScore", MatsuriTestFlags)
bool FMatsuriZeniTest::RunTest(const FString&)
{
	FMatsuriRunState Run = RunWith(EMatsuriHero::Mio);
	TestFalse(TEXT("cannot spend score you do not have"), Run.SpendScore(Matsuri::Run::ZeniCost));
	Run.Collect(EMatsuriCollectible::Magatama);
	Run.Collect(EMatsuriCollectible::Magatama);
	TestTrue(TEXT("spends when affordable"), Run.SpendScore(Matsuri::Run::ZeniCost));
	TestEqual(TEXT("run score reduced"), Run.Score.RunScore, 10 - Matsuri::Run::ZeniCost);
	TestEqual(TEXT("floor score reduced too"), Run.Score.FloorScore, 10 - Matsuri::Run::ZeniCost);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriShotSpeedTest, "SuzuneMatsuri.Heroes.ShotsLandWithoutDelay", MatsuriTestFlags)
bool FMatsuriShotSpeedTest::RunTest(const FString&)
{
	// Playtest: hits felt late. A shot must cross a 15 m maze corridor in 0.2 s or less.
	constexpr float CorridorCm = 1500.0f;
	constexpr float MaxFlightSeconds = 0.2f;
	for (int32 i = 0; i < Matsuri::Heroes::Count; ++i)
	{
		const FMatsuriHeroDef& Hero = Matsuri::Heroes::Get(static_cast<EMatsuriHero>(i));
		TestTrue(FString::Printf(TEXT("%s's shot crosses 15 m in <= 0.2 s"), *Hero.Name.ToString()),
			CorridorCm / Hero.Weapon.Speed <= MaxFlightSeconds);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
