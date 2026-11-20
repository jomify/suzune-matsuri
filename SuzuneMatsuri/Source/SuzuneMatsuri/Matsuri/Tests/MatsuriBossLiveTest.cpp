// Suzune's Night Festival - a live call to the boss's language model (not in the SuzuneMatsuri suite: it
// needs the network and DEEPSEEK_API_KEY, and costs a fraction of a cent). Sends one decision prompt and one
// after-fight reflection built from a realistic situation, and checks both answers are usable.
// Run: Automation RunTests MatsuriLive.BossModel   (matsuri.BossLLM ollama to try the local model instead)

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/AutomationCommon.h"
#include "MatsuriBossArena.h"
#include "MatsuriBossMemory.h"
#include "MatsuriBossModelClient.h"
#include "MatsuriBossPrompt.h"

namespace
{
	constexpr double DecisionTimeout = 30.0;
	constexpr double ReflectionTimeout = 120.0;

	struct FLiveState
	{
		TOptional<FMatsuriModelReply> Decision;
		TOptional<FMatsuriModelReply> Reflection;
		double Started = 0.0;
	};

	FMatsuriBossObservation Camper(int32 Phase)
	{
		FMatsuriBossObservation O;
		O.Phase = Phase;
		O.BossHealth = Phase == 2 ? 0.55f : 0.9f;
		O.Hero = TEXT("sakura (bow, charged arrows)");
		O.PlayerLives = 3;
		O.PlayerAmmo = 7;
		O.Zone = 2;
		O.ZoneName = TEXT("lantern_sw");
		O.bPlayerShielded = true;
		O.DistanceMeters = 24.0f;
		O.CoverShare = 0.75f;
		O.FavouriteZone = 2;
		O.FavouriteZoneName = TEXT("lantern_sw");
		return O;
	}

	FMatsuriBossStep Past(EMatsuriBossSkill Skill, float Reward, int32 Touches, float Damage)
	{
		FMatsuriBossStep Step;
		Step.FightId = TEXT("fight-20261028-210000");
		Step.Observation = Camper(2);
		Step.Action = Matsuri::BossSkills::Preset(Skill, 1);
		Step.Action.Target = Skill == EMatsuriBossSkill::FoxRain ? 2 : -1;
		Step.Outcome.Touches = Touches;
		Step.Outcome.BossDamage = Damage;
		Step.Reward = Reward;
		return Step;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossLiveTest, "MatsuriLive.BossModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriBossLiveTest::RunTest(const FString&)
{
	const FMatsuriModelSettings Settings = FMatsuriModelSettings::FromConsole();
	if (!Settings.IsUsable())
	{
		AddError(FString::Printf(TEXT("the boss model is not usable (provider '%s'): set DEEPSEEK_API_KEY or matsuri.BossLLM ollama"), *Settings.Provider));
		return false;
	}
	const TSharedRef<FMatsuriBossModelClient> Client = MakeShared<FMatsuriBossModelClient>(Settings);
	const FMatsuriBossArena Arena = Matsuri::BossArena::Layout();

	FMatsuriBossPromptInput Input;
	Input.Now = Camper(2);
	for (const FMatsuriArenaZone& Zone : Arena.Zones)
	{
		Input.ZoneNames.Add(Zone.Name.ToString());
		Input.ZoneHigh.Add(Zone.bHigh);
	}
	Input.AnchorCount = Arena.Anchors.Num();
	Input.Examples = { Past(EMatsuriBossSkill::FoxRain, 1.8f, 2, 0.02f), Past(EMatsuriBossSkill::Ring, -1.2f, 0, 0.12f), Past(EMatsuriBossSkill::Spiral, -0.6f, 0, 0.06f) };
	Input.ThisFight = { Past(EMatsuriBossSkill::Pursuit, -0.8f, 0, 0.08f) };
	Input.Lessons = { TEXT("when she camps behind a stone lantern, rain on that zone: rings and spirals hit the stone") };
	const FMatsuriBossPrompt Decision = Matsuri::BossPrompt::Decision(Input);

	TSharedRef<FLiveState> State = MakeShared<FLiveState>();
	State->Started = FPlatformTime::Seconds();
	Client->Chat(Decision.System, Decision.User, 300, false, true, [State](const FMatsuriModelReply& Reply) { State->Decision = Reply; });

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, Client, Input, Arena]()
	{
		if (!State->Decision.IsSet())
		{
			if (FPlatformTime::Seconds() - State->Started < DecisionTimeout)
			{
				return false;
			}
			AddError(TEXT("no decision within 30 s"));
			return true;
		}
		const FMatsuriModelReply& Reply = State->Decision.GetValue();
		TestTrue(FString::Printf(TEXT("the model answered (%s)"), *Reply.Error), Reply.bOk);
		FMatsuriBossAction Action;
		FString Error;
		const bool bParsed = Reply.bOk && Matsuri::BossPrompt::ParseDecision(Reply.Content, Input.Now.Phase, Input.ZoneNames, Input.AnchorCount, Action, Error);
		TestTrue(FString::Printf(TEXT("its answer is a legal action (%s)"), *Error), bParsed);
		AddInfo(FString::Printf(TEXT("decision in %.1f s, %d+%d tokens: %s(%.2f, %.2f, %.2f, %.2f) target %d, taunt '%s', why: %s"),
			Reply.Seconds, Reply.PromptTokens, Reply.CompletionTokens, Matsuri::BossSkills::Id(Action.Skill),
			Action.Params[0], Action.Params[1], Action.Params[2], Action.Params[3], Action.Target, *Action.Taunt, *Action.Why));

		// After the fight: the notebook.
		FMatsuriBossFight Fight;
		Fight.FightId = TEXT("fight-live");
		Fight.Hero = Input.Now.Hero;
		Fight.Seconds = 140.0f;
		Fight.Steps = Input.Examples.Num();
		Fight.MeanReward = 0.0f;
		Fight.BossHealthLeft = 0.0f;
		const FMatsuriBossPrompt Reflection = Matsuri::BossPrompt::Reflection(Fight, Input.Examples, Input.Lessons, Input.ZoneNames);
		State->Started = FPlatformTime::Seconds();
		Client->Chat(Reflection.System, Reflection.User, 2000, true, true, [State](const FMatsuriModelReply& Reflected) { State->Reflection = Reflected; });
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, Client]()
	{
		if (!State->Reflection.IsSet())
		{
			if (FPlatformTime::Seconds() - State->Started < ReflectionTimeout)
			{
				return false;
			}
			AddError(TEXT("no reflection within 120 s"));
			return true;
		}
		const FMatsuriModelReply& Reply = State->Reflection.GetValue();
		TArray<FString> Lessons;
		TestTrue(FString::Printf(TEXT("the reflection is a notebook (%s)"), *Reply.Error), Reply.bOk && Matsuri::BossPrompt::ParseLessons(Reply.Content, Lessons));
		AddInfo(FString::Printf(TEXT("reflection in %.1f s, %d+%d tokens, %d lessons"), Reply.Seconds, Reply.PromptTokens, Reply.CompletionTokens, Lessons.Num()));
		for (const FString& Lesson : Lessons)
		{
			AddInfo(TEXT("  lesson: ") + Lesson);
		}
		return true;
	}));
	return true;
}

// Reviews the latest fight in Saved/BossMemory (matsuri.BossMemory) and writes her notebook, exactly as the
// game does after a fight. Useful when a reflection failed in play (for example: out of tokens).
// Run: Automation RunTests MatsuriLive.BossReflectLatest
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossReflectLatestTest, "MatsuriLive.BossReflectLatest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriBossReflectLatestTest::RunTest(const FString&)
{
	const FMatsuriModelSettings Settings = FMatsuriModelSettings::FromConsole();
	TSharedRef<FMatsuriBossStore> Store = MakeShared<FMatsuriBossStore>(FMatsuriBossStore::DefaultFolder());
	Store->Load();
	if (!Settings.IsUsable() || Store->GetFights().Num() == 0)
	{
		AddError(Settings.IsUsable() ? TEXT("no recorded fight to review") : TEXT("the boss model is not usable"));
		return false;
	}
	const FMatsuriBossFight Fight = Store->GetFights().Last();
	TArray<FMatsuriBossStep> Steps;
	for (const FMatsuriBossStep& Step : Store->GetSteps())
	{
		if (Step.FightId == Fight.FightId)
		{
			Steps.Add(Step);
		}
	}
	TArray<FString> ZoneNames;
	for (const FMatsuriArenaZone& Zone : Matsuri::BossArena::Layout().Zones)
	{
		ZoneNames.Add(Zone.Name.ToString());
	}
	const FMatsuriBossPrompt Prompt = Matsuri::BossPrompt::Reflection(Fight, Steps, Store->GetLessons(), ZoneNames);
	AddInfo(FString::Printf(TEXT("reviewing %s: %d skills, prompt %d characters"), *Fight.FightId, Steps.Num(), Prompt.System.Len() + Prompt.User.Len()));

	const TSharedRef<FMatsuriBossModelClient> Client = MakeShared<FMatsuriBossModelClient>(Settings);
	TSharedRef<FLiveState> State = MakeShared<FLiveState>();
	State->Started = FPlatformTime::Seconds();
	Client->Chat(Prompt.System, Prompt.User, 8000, true, true, [State](const FMatsuriModelReply& Reply) { State->Reflection = Reply; });
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, Store, Client]()
	{
		if (!State->Reflection.IsSet())
		{
			if (FPlatformTime::Seconds() - State->Started < ReflectionTimeout + 10.0)
			{
				return false;
			}
			AddError(TEXT("no reflection in time"));
			return true;
		}
		const FMatsuriModelReply& Reply = State->Reflection.GetValue();
		TArray<FString> Lessons;
		const bool bOk = Reply.bOk && Matsuri::BossPrompt::ParseLessons(Reply.Content, Lessons);
		TestTrue(FString::Printf(TEXT("the reflection is a notebook (%s)"), *Reply.Error), bOk);
		AddInfo(FString::Printf(TEXT("reflection in %.1f s, %d+%d tokens, %d lessons"), Reply.Seconds, Reply.PromptTokens, Reply.CompletionTokens, Lessons.Num()));
		if (bOk)
		{
			Store->SetLessons(Lessons);
			for (const FString& Lesson : Lessons)
			{
				AddInfo(TEXT("  lesson: ") + Lesson);
			}
		}
		return true;
	}));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
