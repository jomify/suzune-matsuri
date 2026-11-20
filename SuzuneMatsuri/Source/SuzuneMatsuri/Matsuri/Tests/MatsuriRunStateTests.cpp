// Suzune's Night Festival - run state / omamori / record unit tests.
// Run: Automation RunTests SuzuneMatsuri.Run

#include "Misc/AutomationTest.h"
#include "MatsuriOmamori.h"
#include "MatsuriRecord.h"
#include "MatsuriRunState.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags MatsuriTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriScoreRemoveTest, "SuzuneMatsuri.Score.RemovePointsClampsAtZero", MatsuriTestFlags)
bool FMatsuriScoreRemoveTest::RunTest(const FString&)
{
	FMatsuriScoreState State;
	State.BeginFloor(2);
	State.AddPoints(2);

	const int32 Removed = State.RemovePoints(3);

	TestEqual(TEXT("only what existed is removed"), Removed, 2);
	TestEqual(TEXT("run score floors at 0"), State.RunScore, 0);
	TestEqual(TEXT("floor score floors at 0"), State.FloorScore, 0);
	TestTrue(TEXT("a cleared floor stays cleared"), State.bFloorCleared);
	TestEqual(TEXT("non-positive ignored"), State.RemovePoints(-1), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriRunHurtTest, "SuzuneMatsuri.Run.HurtCostsHealth", MatsuriTestFlags)
bool FMatsuriRunHurtTest::RunTest(const FString&)
{
	FMatsuriRunState Run;
	Run.Reset(1, EMatsuriHero::Mio);
	Run.BeginFloor(50);
	Run.Collect(EMatsuriCollectible::Magatama);

	TestEqual(TEXT("small damage leaves health"), Run.Hurt(30.0f), EMatsuriHurtResult::Hurt);
	TestEqual(TEXT("health reduced by damage"), Run.Health, 170.0f);
	TestEqual(TEXT("score is unchanged"), Run.Score.RunScore, 5);
	TestEqual(TEXT("fatal damage ends the run"), Run.Hurt(1000.0f), EMatsuriHurtResult::Died);
	TestEqual(TEXT("health clamps at zero"), Run.Health, 0.0f);
	TestEqual(TEXT("invalid damage is ignored"), Run.Hurt(0.0f), EMatsuriHurtResult::Ignored);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriRunDamagePreservesScoreTest, "SuzuneMatsuri.Run.DamagePreservesCoinScore", MatsuriTestFlags)
bool FMatsuriRunDamagePreservesScoreTest::RunTest(const FString&)
{
	FMatsuriRunState Run;
	Run.Reset(1, EMatsuriHero::Mio);
	Run.BeginFloor(50);
	Run.Collect(EMatsuriCollectible::Magatama);
	const int32 ScoreBeforeDamage = Run.Score.RunScore;

	TestEqual(TEXT("a non-fatal hit is reported"), Run.Hurt(), EMatsuriHurtResult::Hurt);
	TestEqual(TEXT("damage does not remove collected coin score"), Run.Score.RunScore, ScoreBeforeDamage);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriHealthRulesTest, "SuzuneMatsuri.Run.HealthRules", MatsuriTestFlags)
bool FMatsuriHealthRulesTest::RunTest(const FString&)
{
	FMatsuriRunState Mio;
	Mio.Reset(1, EMatsuriHero::Mio);
	TestTrue(TEXT("ordinary heroine starts at 200 HP"), FMath::IsNearlyEqual(Mio.Health, 200.0f));

	FMatsuriRunState Sakura;
	Sakura.Reset(1, EMatsuriHero::Sakura);
	TestTrue(TEXT("Sakura starts at 250 HP"), FMath::IsNearlyEqual(Sakura.Health, 250.0f));

	TestEqual(TEXT("overheal is clamped"), Mio.Heal(50.0f), 0.0f);
	Mio.Hurt(75.0f);
	TestTrue(TEXT("partial damage is applied"), FMath::IsNearlyEqual(Mio.Health, 125.0f));
	TestTrue(TEXT("heal returns actual amount"), FMath::IsNearlyEqual(Mio.Heal(100.0f), 75.0f));
	TestTrue(TEXT("heal stops at max HP"), FMath::IsNearlyEqual(Mio.Health, Mio.GetMaxHealth()));

	Mio.Hurt(75.0f);
	const float BeforeKenkou = Mio.Health;
	Mio.TakeCharm(EMatsuriOmamori::Kenkou);
	TestTrue(TEXT("Kenkou adds 50 max HP"), FMath::IsNearlyEqual(Mio.GetMaxHealth(), 250.0f));
	TestTrue(TEXT("Kenkou heals 50 when hurt"), FMath::IsNearlyEqual(Mio.Health, BeforeKenkou + 50.0f));

	Mio.Hurt(40.0f);
	Mio.BeginFloor(50);
	TestTrue(TEXT("beginning a floor restores full health"), FMath::IsNearlyEqual(Mio.Health, Mio.GetMaxHealth()));

	const float BeforeInvalid = Mio.Health;
	TestEqual(TEXT("negative damage is ignored"), Mio.Hurt(-1.0f), EMatsuriHurtResult::Ignored);
	TestEqual(TEXT("NaN damage is ignored"), Mio.Hurt(std::numeric_limits<float>::quiet_NaN()), EMatsuriHurtResult::Ignored);
	TestEqual(TEXT("infinite damage is ignored"), Mio.Hurt(std::numeric_limits<float>::infinity()), EMatsuriHurtResult::Ignored);
	TestTrue(TEXT("invalid damage does not change HP"), FMath::IsNearlyEqual(Mio.Health, BeforeInvalid));
	TestEqual(TEXT("negative healing is ignored"), Mio.Heal(-1.0f), 0.0f);
	TestEqual(TEXT("NaN healing is ignored"), Mio.Heal(std::numeric_limits<float>::quiet_NaN()), 0.0f);
	TestEqual(TEXT("infinite healing is ignored"), Mio.Heal(std::numeric_limits<float>::infinity()), 0.0f);
	TestTrue(TEXT("invalid healing does not change HP"), FMath::IsNearlyEqual(Mio.Health, BeforeInvalid));

	TestEqual(TEXT("fatal damage is reported"), Mio.Hurt(10000.0f), EMatsuriHurtResult::Died);
	TestTrue(TEXT("fatal damage clamps HP"), FMath::IsNearlyEqual(Mio.Health, 0.0f));
	TestEqual(TEXT("dead HP cannot be healed"), Mio.Heal(100.0f), 0.0f);
	TestTrue(TEXT("dead HP stays zero"), FMath::IsNearlyEqual(Mio.Health, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriRunFoxTest, "SuzuneMatsuri.Run.FoxBlessingAbsorbsOneHitPerFloor", MatsuriTestFlags)
bool FMatsuriRunFoxTest::RunTest(const FString&)
{
	FMatsuriRunState Run;
	Run.Reset(1, EMatsuriHero::Mio);	// base health, 20-round magazine
	Run.TakeCharm(EMatsuriOmamori::Kitsune);
	Run.BeginFloor(10);

	TestEqual(TEXT("blessing absorbs first hit"), Run.Hurt(), EMatsuriHurtResult::Blessed);
	TestEqual(TEXT("health untouched"), Run.Health, Run.GetMaxHealth());
	TestEqual(TEXT("second hit costs health"), Run.Hurt(), EMatsuriHurtResult::Hurt);

	Run.AdvanceFloor();
	Run.BeginFloor(10);
	TestEqual(TEXT("blessing refills on the next floor"), Run.Hurt(), EMatsuriHurtResult::Blessed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriRunAmmoTest, "SuzuneMatsuri.Run.AmmoRespectsCapAndBalance", MatsuriTestFlags)
bool FMatsuriRunAmmoTest::RunTest(const FString&)
{
	FMatsuriRunState Run;
	Run.Reset(1, EMatsuriHero::Mio);	// base health, 20-round magazine

	const int32 Cap = Matsuri::Heroes::Get(EMatsuriHero::Mio).Weapon.AmmoCap;
	TestEqual(TEXT("starts with half a magazine"), Run.Ammo, Cap / 2);
	TestEqual(TEXT("add clamps to cap"), Run.AddAmmo(100), Cap - Cap / 2);
	TestEqual(TEXT("full"), Run.Ammo, Cap);
	TestTrue(TEXT("consume when enough"), Run.ConsumeAmmo(Cap));
	TestFalse(TEXT("cannot consume when empty"), Run.ConsumeAmmo(1));
	TestEqual(TEXT("empty stays empty"), Run.Ammo, 0);

	Run.TakeCharm(EMatsuriOmamori::Dangoke);
	TestEqual(TEXT("dangoke raises cap by half"), Run.GetAmmoCap(), FMath::RoundToInt32(Cap * 1.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriRunCharmsTest, "SuzuneMatsuri.Run.CharmsChangeTheRules", MatsuriTestFlags)
bool FMatsuriRunCharmsTest::RunTest(const FString&)
{
	FMatsuriRunState Run;
	Run.Reset(1, EMatsuriHero::Suzune);	// no ginka passive, so charm effects stand alone
	Run.BeginFloor(100);

	Run.TakeCharm(EMatsuriOmamori::Kinun);
	TestEqual(TEXT("kinun: koban worth 2"), Run.PointsFor(EMatsuriCollectible::Koban), 2);
	TestEqual(TEXT("kinun leaves ginka alone"), Run.PointsFor(EMatsuriCollectible::Ginka), 2);

	for (int32 i = 0; i < 10; ++i)
	{
		Run.TakeCharm(EMatsuriOmamori::Kenkou);
	}
	TestEqual(TEXT("kenkou caps max health"), Run.GetMaxHealth(), Matsuri::Run::MaxHealth);

	const FMatsuriModifiers Twice = Matsuri::Omamori::Apply(Matsuri::Omamori::Apply(FMatsuriModifiers(), EMatsuriOmamori::Toki), EMatsuriOmamori::Toki);
	TestEqual(TEXT("toki stacks"), Twice.ExtraSeconds, 2.0f * Matsuri::Omamori::TokiSeconds);
	TestEqual(TEXT("apply does not touch the input"), FMatsuriModifiers().ExtraSeconds, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriOmamoriOfferTest, "SuzuneMatsuri.Run.OmamoriOfferIsDistinctAndDeterministic", MatsuriTestFlags)
bool FMatsuriOmamoriOfferTest::RunTest(const FString&)
{
	for (int32 Seed = 1; Seed <= 30; ++Seed)
	{
		const TArray<EMatsuriOmamori> Offer = Matsuri::Omamori::RollOffer(Seed, 0);
		TestEqual(TEXT("three cards"), Offer.Num(), Matsuri::Omamori::OfferSize);
		TestEqual(TEXT("no duplicates"), TSet<EMatsuriOmamori>(Offer).Num(), Offer.Num());
		TestTrue(TEXT("same seed same offer"), Offer == Matsuri::Omamori::RollOffer(Seed, 0));
		for (const EMatsuriOmamori Charm : Offer)
		{
			TestFalse(TEXT("every card has a name"), Matsuri::Omamori::DisplayName(Charm).IsEmpty());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriRecordTest, "SuzuneMatsuri.Run.RecordKeepsBest", MatsuriTestFlags)
bool FMatsuriRecordTest::RunTest(const FString&)
{
	const FMatsuriRecord Empty;
	const FMatsuriRecord First = Matsuri::Record::WithRun(Empty, 40, 2, false);
	const FMatsuriRecord Second = Matsuri::Record::WithRun(First, 25, 3, true);

	TestEqual(TEXT("best kept"), Second.BestScore, 40);
	TestEqual(TEXT("deepest floor kept"), Second.DeepestFloor, 3);
	TestEqual(TEXT("runs counted"), Second.RunsPlayed, 2);
	TestEqual(TEXT("victories counted"), Second.Victories, 1);
	TestTrue(TEXT("41 beats 40"), Matsuri::Record::IsNewBest(Second, 41));
	TestFalse(TEXT("40 does not beat 40"), Matsuri::Record::IsNewBest(Second, 40));
	TestEqual(TEXT("input untouched"), Empty.RunsPlayed, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
