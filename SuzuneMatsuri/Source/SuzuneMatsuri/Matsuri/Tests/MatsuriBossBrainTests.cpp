// Suzune's Night Festival - the boss's learning, without a world or a network: reward, memory, the local
// learner, the prompt and the reading of the model's answers.
// Run: Automation RunTests SuzuneMatsuri.Boss

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "MatsuriBossArena.h"
#include "MatsuriBossLearner.h"
#include "MatsuriBossMemory.h"
#include "MatsuriBossModelClient.h"
#include "MatsuriBossPrompt.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr auto BrainTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FMatsuriBossObservation Situation(int32 Phase, bool bShielded, bool bHigh, int32 Zone = -1)
	{
		FMatsuriBossObservation O;
		O.Phase = Phase;
		O.bPlayerShielded = bShielded;
		O.bPlayerHigh = bHigh;
		O.Zone = Zone;
		O.FavouriteZone = Zone;
		O.CoverShare = bShielded ? 0.8f : 0.1f;
		O.HighShare = bHigh ? 0.8f : 0.0f;
		O.DistanceMeters = 20.0f;
		O.PlayerAmmo = 6;
		return O;
	}

	FMatsuriBossStep StepWith(const FMatsuriBossObservation& O, EMatsuriBossSkill Skill, float Reward)
	{
		FMatsuriBossStep Step;
		Step.FightId = TEXT("fight-test");
		Step.Observation = O;
		Step.Action = Matsuri::BossSkills::Preset(Skill, 0);
		Step.Reward = Reward;
		return Step;
	}

	TArray<FString> ArenaZoneNames()
	{
		TArray<FString> Names;
		for (const FMatsuriArenaZone& Zone : Matsuri::BossArena::Layout().Zones)
		{
			Names.Add(Zone.Name.ToString());
		}
		return Names;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossHealthMemoryTest, "SuzuneMatsuri.Boss.HealthMemoryCompatibility", BrainTestFlags)
bool FMatsuriBossHealthMemoryTest::RunTest(const FString&)
{
	FMatsuriBossStep Step = StepWith(Situation(2, true, false), EMatsuriBossSkill::FoxRain, 1.0f);
	Step.Observation.PlayerHealth = 124.0f;
	Step.Observation.PlayerMaxHealth = 250.0f;
	const TSharedRef<FJsonObject> Json = Matsuri::BossMemory::ToJson(Step);
	FMatsuriBossStep Loaded;
	TestTrue(TEXT("new health observation loads"), Matsuri::BossMemory::FromJson(*Json, Loaded));
	TestTrue(TEXT("new health is exact, not converted to lives"), Loaded.Observation.bHealthKnown);
	TestEqual(TEXT("health round trips"), Loaded.Observation.PlayerHealth, 124.0f);
	TestEqual(TEXT("maximum health round trips"), Loaded.Observation.PlayerMaxHealth, 250.0f);

	const TSharedPtr<FJsonObject> Obs = Json->GetObjectField(TEXT("obs"));
	Obs->RemoveField(TEXT("health"));
	Obs->RemoveField(TEXT("max_health"));
	Obs->SetNumberField(TEXT("lives"), 2);
	TestTrue(TEXT("legacy life observations remain readable"), Matsuri::BossMemory::FromJson(*Json, Loaded));
	TestFalse(TEXT("old lives are not fabricated HP"), Loaded.Observation.bHealthKnown);
	TestTrue(TEXT("prompt labels legacy observations"), Matsuri::BossMemory::Describe(Loaded.Observation).Contains(TEXT("legacy record")));

	FMatsuriBossFight Fight;
	Fight.FightId = TEXT("health-migration-test");
	Fight.PlayerHealthLost = 128.0f;
	const TSharedRef<FJsonObject> FightJson = Matsuri::BossMemory::ToJson(Fight);
	FMatsuriBossFight LoadedFight;
	TestTrue(TEXT("health fight loads"), Matsuri::BossMemory::FromJson(*FightJson, LoadedFight));
	TestEqual(TEXT("health lost round trips"), LoadedFight.PlayerHealthLost, 128.0f);
	FightJson->RemoveField(TEXT("player_health_lost"));
	FightJson->SetNumberField(TEXT("player_lives_lost"), 2);
	TestTrue(TEXT("legacy fight summaries load"), Matsuri::BossMemory::FromJson(*FightJson, LoadedFight));
	TestFalse(TEXT("legacy summary is identified"), LoadedFight.bHealthKnown);
	TestEqual(TEXT("legacy lives remain intact"), LoadedFight.PlayerLivesLost, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossRewardTest, "SuzuneMatsuri.Boss.Reward", BrainTestFlags)
bool FMatsuriBossRewardTest::RunTest(const FString&)
{
	FMatsuriBossOutcome Outcome;
	Outcome.Touches = 2;
	TestEqual(TEXT("two bullets reached the player: +2"), Matsuri::BossMemory::Reward(Outcome), 2.0f);
	Outcome.Touches = 9;
	TestEqual(TEXT("touches count up to 3"), Matsuri::BossMemory::Reward(Outcome), 3.0f);
	Outcome = FMatsuriBossOutcome();
	Outcome.BossDamage = 0.2f;
	TestTrue(TEXT("losing a fifth of her life: -2"), FMath::IsNearlyEqual(Matsuri::BossMemory::Reward(Outcome), -2.0f));
	Outcome = FMatsuriBossOutcome();
	Outcome.Grazes = 100;
	TestTrue(TEXT("grazes are worth at most 0.5"), FMath::IsNearlyEqual(Matsuri::BossMemory::Reward(Outcome), 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossRetrieveTest, "SuzuneMatsuri.Boss.RetrieveSimilar", BrainTestFlags)
bool FMatsuriBossRetrieveTest::RunTest(const FString&)
{
	TArray<FMatsuriBossStep> Steps;
	const FMatsuriBossObservation Camping = Situation(2, true, false, 2);
	const FMatsuriBossObservation OnPlatform = Situation(2, false, true, 12);
	for (int32 i = 0; i < 10; ++i)
	{
		Steps.Add(StepWith(OnPlatform, EMatsuriBossSkill::TailSweep, 1.0f));		// unrelated situations
	}
	Steps.Add(StepWith(Camping, EMatsuriBossSkill::FoxRain, 2.0f));
	Steps.Add(StepWith(Camping, EMatsuriBossSkill::Ring, -1.0f));
	Steps.Add(StepWith(Camping, EMatsuriBossSkill::Spiral, 0.1f));
	Steps.Add(StepWith(Situation(1, true, false, 2), EMatsuriBossSkill::Pursuit, 3.0f));	// other phase

	const TArray<int32> Picked = Matsuri::BossMemory::Retrieve(Steps, Camping, 2);
	TestEqual(TEXT("K examples"), Picked.Num(), 2);
	TestTrue(TEXT("the best similar moment is shown"), Picked.Contains(10));
	TestTrue(TEXT("and the worst, so failures are seen too"), Picked.Contains(11));
	TestFalse(TEXT("another phase is not preferred over the same phase"), Picked.Contains(13));
	TestEqual(TEXT("no memory, no examples"), Matsuri::BossMemory::Retrieve({}, Camping, 8).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossMemoryTest, "SuzuneMatsuri.Boss.MemoryOnDisk", BrainTestFlags)
bool FMatsuriBossMemoryTest::RunTest(const FString&)
{
	const FString Folder = FPaths::ProjectSavedDir() / TEXT("BossMemory_UnitTest");
	FMatsuriBossStore Store(Folder);
	Store.Wipe();
	FMatsuriBossStep Step = StepWith(Situation(3, true, true, 5), EMatsuriBossSkill::Phantom, 1.25f);
	Step.Action.Params[2] = 0.33f;
	Step.Action.Target = 4;
	Step.Action.Source = EMatsuriBossSource::Model;
	Step.Action.Why = TEXT("she hides behind the north lantern");
	Step.Outcome.Touches = 2;
	Store.AppendStep(Step);
	Store.AppendStep(StepWith(Situation(1, false, false), EMatsuriBossSkill::Ring, -0.5f));
	FMatsuriBossFight Fight;
	Fight.FightId = TEXT("fight-test");
	Fight.bBossWon = true;
	Fight.MeanReward = 0.4f;
	Store.AppendFight(Fight);
	Store.SetLessons({ TEXT("rain on the lantern she camps"), TEXT("high sweep when she climbs") });

	FMatsuriBossStore Reloaded(Folder);
	Reloaded.Load();
	TestEqual(TEXT("steps come back"), Reloaded.GetSteps().Num(), 2);
	TestEqual(TEXT("fights come back"), Reloaded.GetFights().Num(), 1);
	TestEqual(TEXT("lessons come back"), Reloaded.GetLessons().Num(), 2);
	if (Reloaded.GetSteps().Num() == 2)
	{
		const FMatsuriBossStep& Back = Reloaded.GetSteps()[0];
		TestTrue(TEXT("skill, params, target, source and reason survive"), Back.Action.Skill == EMatsuriBossSkill::Phantom
			&& FMath::IsNearlyEqual(Back.Action.Params[2], 0.33f, 0.001f) && Back.Action.Target == 4
			&& Back.Action.Source == EMatsuriBossSource::Model && Back.Action.Why == Step.Action.Why);
		TestTrue(TEXT("the situation survives"), Back.Observation.Phase == 3 && Back.Observation.bPlayerShielded && Back.Observation.Zone == 5);
		TestTrue(TEXT("reward and outcome survive"), FMath::IsNearlyEqual(Back.Reward, 1.25f) && Back.Outcome.Touches == 2);
	}
	TestTrue(TEXT("the fight's verdict survives"), Reloaded.GetFights().Num() == 1 && Reloaded.GetFights()[0].bBossWon);
	Store.Wipe();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossLearnerAdaptsTest, "SuzuneMatsuri.Boss.LearnerAdapts", BrainTestFlags)
bool FMatsuriBossLearnerAdaptsTest::RunTest(const FString&)
{
	// A synthetic player with two habits. Camping behind cover, only rain reaches her; standing on a
	// platform, only the high tail sweep does. Everything else costs the boss (she is shot meanwhile).
	const FMatsuriBossObservation Camping = Situation(2, true, false, 2);
	const FMatsuriBossObservation OnPlatform = Situation(2, false, true, 12);
	auto RewardOf = [](const FMatsuriBossObservation& O, int32 Arm)
	{
		const EMatsuriBossSkill Skill = FMatsuriBossLearner::SkillOf(Arm);
		if (O.bPlayerShielded)
		{
			return Skill == EMatsuriBossSkill::FoxRain ? 1.5 : -0.4;
		}
		return Skill == EMatsuriBossSkill::TailSweep && FMatsuriBossLearner::PresetOf(Arm) == 1 ? 1.5 : -0.4;
	};
	FMatsuriBossLearner Learner;
	const int32 Before = Learner.Choose(Camping);
	int32 LateRain = 0, LateSweep = 0;
	constexpr int32 Rounds = 120;
	for (int32 i = 0; i < Rounds; ++i)
	{
		const FMatsuriBossObservation& Now = i % 2 == 0 ? Camping : OnPlatform;
		const int32 Arm = Learner.Choose(Now);
		Learner.Update(Now, Arm, RewardOf(Now, Arm));
		if (i >= Rounds - 30)
		{
			LateRain += i % 2 == 0 && FMatsuriBossLearner::SkillOf(Arm) == EMatsuriBossSkill::FoxRain ? 1 : 0;
			LateSweep += i % 2 == 1 && Arm == FMatsuriBossLearner::ArmFor(EMatsuriBossSkill::TailSweep, 1) ? 1 : 0;
		}
	}
	TestNotEqual(TEXT("she does not start out raining"), FMatsuriBossLearner::SkillOf(Before), EMatsuriBossSkill::FoxRain);
	TestEqual(TEXT("against a camper she learns to rain"), FMatsuriBossLearner::SkillOf(Learner.Choose(Camping)), EMatsuriBossSkill::FoxRain);
	TestEqual(TEXT("against a platform sitter, the high sweep"), Learner.Choose(OnPlatform), FMatsuriBossLearner::ArmFor(EMatsuriBossSkill::TailSweep, 1));
	TestTrue(FString::Printf(TEXT("and keeps doing it (%d/15 rain, %d/15 sweep late on)"), LateRain, LateSweep), LateRain >= 12 && LateSweep >= 12);
	TestFalse(TEXT("never a skill her phase forbids"), FMatsuriBossLearner::SkillOf(Learner.Choose(Situation(1, true, false, 2))) == EMatsuriBossSkill::FoxRain);

	// It survives a save and a load, and can be rebuilt from the fight records alone.
	const FString Path = FPaths::ProjectSavedDir() / TEXT("BossMemory_UnitTest") / TEXT("learner.json");
	TestTrue(TEXT("saves"), Learner.Save(Path));
	FMatsuriBossLearner Loaded;
	TestTrue(TEXT("loads"), Loaded.Load(Path));
	TestTrue(TEXT("same estimate after loading"), FMath::IsNearlyEqual(Loaded.Estimate(4 * 3, Camping), Learner.Estimate(4 * 3, Camping), 1e-6));
	IFileManager::Get().Delete(*Path);

	TArray<FMatsuriBossStep> Records;
	for (int32 i = 0; i < 20; ++i)
	{
		Records.Add(StepWith(Camping, EMatsuriBossSkill::FoxRain, 1.5f));
		Records.Add(StepWith(Camping, EMatsuriBossSkill::Ring, -0.5f));
	}
	FMatsuriBossLearner Rebuilt;
	Rebuilt.Rebuild(Records);
	TestTrue(TEXT("rebuilt from records, rain looks better than rings"),
		Rebuilt.Estimate(FMatsuriBossLearner::ArmFor(EMatsuriBossSkill::FoxRain, 0), Camping) > Rebuilt.Estimate(FMatsuriBossLearner::ArmFor(EMatsuriBossSkill::Ring, 0), Camping));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossPromptTest, "SuzuneMatsuri.Boss.Prompt", BrainTestFlags)
bool FMatsuriBossPromptTest::RunTest(const FString&)
{
	FMatsuriBossPromptInput Input;
	Input.Now = Situation(1, true, false, 2);
	Input.Now.ZoneName = TEXT("lantern_sw");
	Input.ZoneNames = ArenaZoneNames();
	for (const FMatsuriArenaZone& Zone : Matsuri::BossArena::Layout().Zones)
	{
		Input.ZoneHigh.Add(Zone.bHigh);
	}
	Input.AnchorCount = 6;
	Input.Lessons = { TEXT("she camps the south-west lantern") };
	Input.Examples = { StepWith(Input.Now, EMatsuriBossSkill::Spiral, 0.8f) };
	Input.ThisFight = { StepWith(Input.Now, EMatsuriBossSkill::Ring, -1.0f) };
	Input.LearnerHints = { TEXT("fox_spiral#0 +0.50 (3 tries)") };
	const FMatsuriBossPrompt Prompt = Matsuri::BossPrompt::Decision(Input);

	TestTrue(TEXT("the reward is explained"), Prompt.System.Contains(TEXT("+1 per bullet that reaches the player")));
	TestTrue(TEXT("every skill is described"), Prompt.System.Contains(TEXT("fox_rain")) && Prompt.System.Contains(TEXT("phantom_step")));
	TestTrue(TEXT("phase 1 marks the sweep as not allowed"), Prompt.System.Contains(TEXT("tail_sweep (NOT allowed in this phase)")));
	TestTrue(TEXT("zones are named for targeting"), Prompt.System.Contains(TEXT("lantern_sw")) && Prompt.System.Contains(TEXT("platform_w (high)")));
	TestTrue(TEXT("the notebook is in"), Prompt.User.Contains(TEXT("she camps the south-west lantern")));
	TestTrue(TEXT("past moments with rewards are in (ICL)"), Prompt.User.Contains(TEXT("fox_spiral")) && Prompt.User.Contains(TEXT("+0.80")));
	TestTrue(TEXT("this fight's rewards are in (in-context RL)"), Prompt.User.Contains(TEXT("This fight so far")) && Prompt.User.Contains(TEXT("-1.00")));
	TestTrue(TEXT("the learner's hint is in"), Prompt.User.Contains(TEXT("fox_spiral#0 +0.50")));
	TestTrue(TEXT("the allowed list matches the phase"), Prompt.User.Contains(TEXT("Allowed skills: fox_ring, fox_spiral, onibi_pursuit.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossParseTest, "SuzuneMatsuri.Boss.ParseModelReply", BrainTestFlags)
bool FMatsuriBossParseTest::RunTest(const FString&)
{
	const TArray<FString> Zones = ArenaZoneNames();
	FMatsuriBossAction Action;
	FString Error;

	const FString Fenced = TEXT("Sure!\n```json\n{\"skill\":\"fox_rain\",\"params\":{\"density\":0.9,\"seconds\":0.2,\"radius\":1.4,\"fall_speed\":0.5},")
		TEXT("\"target\":\"lantern_sw\",\"taunt\":\"そこね。\",\"why\":\"she hid there {twice}\"}\n```");
	TestTrue(TEXT("fenced JSON with named params and a zone name"), Matsuri::BossPrompt::ParseDecision(Fenced, 2, Zones, 6, Action, Error));
	TestTrue(TEXT("read as rain on that zone"), Action.Skill == EMatsuriBossSkill::FoxRain && Zones.IsValidIndex(Action.Target) && Zones[Action.Target] == TEXT("lantern_sw"));
	TestTrue(TEXT("named params land in their slots, clamped"), FMath::IsNearlyEqual(Action.Params[0], 0.9f) && Action.Params[2] == 1.0f);
	TestTrue(TEXT("braces inside strings do not confuse it"), Action.Why == TEXT("she hid there {twice}"));
	TestEqual(TEXT("marked as the model's"), Action.Source, EMatsuriBossSource::Model);

	TestTrue(TEXT("positional params"), Matsuri::BossPrompt::ParseDecision(TEXT("{\"skill\":\"fox_ring\",\"params\":[0.1,0.2,0.3,0.4],\"target\":-1}"), 1, Zones, 6, Action, Error)
		&& FMath::IsNearlyEqual(Action.Params[3], 0.4f));
	TestFalse(TEXT("an unknown skill is refused"), Matsuri::BossPrompt::ParseDecision(TEXT("{\"skill\":\"meteor\"}"), 3, Zones, 6, Action, Error));
	TestFalse(TEXT("a skill her phase forbids is refused"), Matsuri::BossPrompt::ParseDecision(TEXT("{\"skill\":\"phantom_step\"}"), 2, Zones, 6, Action, Error));
	TestFalse(TEXT("prose without JSON is refused"), Matsuri::BossPrompt::ParseDecision(TEXT("I would rain on them."), 3, Zones, 6, Action, Error));

	TArray<FString> Lessons;
	TestTrue(TEXT("lessons are read"), Matsuri::BossPrompt::ParseLessons(TEXT("{\"lessons\":[\"a\",\" \",\"b\"]}"), Lessons) && Lessons.Num() == 2);
	TestFalse(TEXT("an empty notebook is not accepted"), Matsuri::BossPrompt::ParseLessons(TEXT("{\"lessons\":[]}"), Lessons));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossRequestTest, "SuzuneMatsuri.Boss.ModelRequest", BrainTestFlags)
bool FMatsuriBossRequestTest::RunTest(const FString&)
{
	FMatsuriModelSettings DeepSeek;
	DeepSeek.Provider = TEXT("deepseek");
	DeepSeek.Model = TEXT("deepseek-flash");
	DeepSeek.Url = TEXT("https://api.deepseek.com/chat/completions");
	const FMatsuriBossModelClient Client(DeepSeek);
	const FString Decision = Client.BuildBody(TEXT("sys"), TEXT("user"), 300, false, true);
	TestTrue(TEXT("decisions: no thinking (fast)"), Decision.Contains(TEXT("\"thinking\":")) && Decision.Contains(TEXT("\"disabled\"")));
	TestTrue(TEXT("JSON output asked for"), Decision.Contains(TEXT("json_object")));
	TestTrue(TEXT("the model is named"), Decision.Contains(TEXT("deepseek-flash")));
	const FString Reflection = Client.BuildBody(TEXT("sys"), TEXT("user"), 2000, true, true);
	TestTrue(TEXT("reflection thinks first"), Reflection.Contains(TEXT("\"enabled\"")) && Reflection.Contains(TEXT("reasoning_effort")));
	TestFalse(TEXT("no key, no requests"), DeepSeek.IsUsable());

	const FMatsuriModelReply Good = FMatsuriBossModelClient::ParseResponse(
		TEXT("{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"{\\\"skill\\\":\\\"fox_ring\\\"}\",\"reasoning_content\":\"...\"}}],\"usage\":{\"prompt_tokens\":2900,\"completion_tokens\":80}}"));
	TestTrue(TEXT("content and token counts are read"), Good.bOk && Good.Content.Contains(TEXT("fox_ring")) && Good.PromptTokens == 2900 && Good.CompletionTokens == 80);
	const FMatsuriModelReply Bad = FMatsuriBossModelClient::ParseResponse(TEXT("{\"error\":{\"message\":\"Insufficient Balance\"}}"));
	TestTrue(TEXT("API errors are reported, not crashed on"), !Bad.bOk && Bad.Error.Contains(TEXT("Insufficient")));

	FMatsuriModelSettings Off;
	Off.Provider = TEXT("off");
	TestFalse(TEXT("off is off"), Off.IsUsable());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
