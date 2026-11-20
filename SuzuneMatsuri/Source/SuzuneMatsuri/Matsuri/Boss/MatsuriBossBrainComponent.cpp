// Suzune's Night Festival - Tamamo's brain: chooses each skill, scores it, and learns across fights.

#include "MatsuriBossBrainComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DateTime.h"
#include "MatsuriBossModelClient.h"
#include "MatsuriBossPrompt.h"
#include "SuzuneMatsuri.h"

namespace
{
	TAutoConsoleVariable<bool> CVarShowcase(
		TEXT("matsuri.BossShowcase"), false,
		TEXT("Demo: Tamamo shows her six skills in order from the first bar on, and nothing is written to her memory."));

	constexpr int32 DecisionMaxTokens = 300;
	constexpr int32 ReflectionMaxTokens = 8000;	// with thinking on, the reasoning counts against this too
	constexpr int32 HintsShown = 5;

	const TCHAR* TauntFor(EMatsuriBossSkill Skill)
	{
		switch (Skill)
		{
		case EMatsuriBossSkill::Ring:		return TEXT("狐火の輪で踊りなさい。");
		case EMatsuriBossSkill::Spiral:		return TEXT("目が回るかしら？");
		case EMatsuriBossSkill::Pursuit:	return TEXT("逃げても無駄よ。");
		case EMatsuriBossSkill::TailSweep:	return TEXT("九つの尾、避けられて？");
		case EMatsuriBossSkill::FoxRain:	return TEXT("そこがお気に入りなのね。");
		case EMatsuriBossSkill::Phantom:	return TEXT("どれが本物かしら。");
		case EMatsuriBossSkill::Count:		break;
		}
		return TEXT("");
	}
}

UMatsuriBossBrainComponent::UMatsuriBossBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = false;	// the boss drives it
}

FString UMatsuriBossBrainComponent::GetProvider() const
{
	return Client && Client->GetSettings().IsUsable() ? Client->GetSettings().Provider + TEXT(":") + Client->GetSettings().Model : TEXT("off");
}

void UMatsuriBossBrainComponent::BeginFight(const FString& Hero, const FMatsuriBossArena& InArena)
{
	Arena = InArena;
	HeroName = Hero;
	FightId = FString::Printf(TEXT("fight-%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	Store = MakeShared<FMatsuriBossStore>(FMatsuriBossStore::DefaultFolder());
	Store->Load();
	if (!Learner.Load(Store->LearnerPath()))
	{
		Learner.Rebuild(Store->GetSteps());	// learn again from the records (or start fresh)
	}
	Client = MakeShared<FMatsuriBossModelClient>(FMatsuriModelSettings::FromConsole());
	ThisFight.Reset();
	Habits.Reset();
	HabitClock = 0.0f;
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Boss fight %s against %s: model %s, memory of %d steps / %d fights, %d lessons"),
		*FightId, *Hero, *GetProvider(), Store->GetSteps().Num(), Store->GetFights().Num(), Store->GetLessons().Num());
}

void UMatsuriBossBrainComponent::Observe(float DeltaSeconds, const FMatsuriBossObservation& Raw, int32 GrappleUses)
{
	HabitClock += DeltaSeconds;
	SinceSample += DeltaSeconds;
	if (SinceSample < HabitSampleSeconds)
	{
		return;
	}
	SinceSample = 0.0f;
	FHabitSample Sample;
	Sample.Time = HabitClock;
	Sample.bShielded = Raw.bPlayerShielded;
	Sample.bHigh = Raw.bPlayerHigh;
	Sample.Zone = Raw.Zone;
	Sample.GrappleUses = GrappleUses;
	Sample.Speed = Raw.MoveSpeed;
	Habits.Add(Sample);
	const int32 Stale = Habits.IndexOfByPredicate([this](const FHabitSample& Old) { return HabitClock - Old.Time <= HabitWindowSeconds; });
	if (Stale > 0)
	{
		Habits.RemoveAt(0, Stale, EAllowShrinking::No);
	}
}

FMatsuriBossObservation UMatsuriBossBrainComponent::Complete(const FMatsuriBossObservation& Raw) const
{
	FMatsuriBossObservation Out = Raw;
	if (Habits.Num() == 0)
	{
		return Out;
	}
	int32 Shielded = 0, High = 0;
	float Speed = 0.0f;
	TMap<int32, int32> ZoneCounts;
	for (const FHabitSample& Sample : Habits)
	{
		Shielded += Sample.bShielded ? 1 : 0;
		High += Sample.bHigh ? 1 : 0;
		Speed += Sample.Speed;
		if (Sample.Zone >= 0)
		{
			++ZoneCounts.FindOrAdd(Sample.Zone);
		}
	}
	Out.CoverShare = static_cast<float>(Shielded) / Habits.Num();
	Out.HighShare = static_cast<float>(High) / Habits.Num();
	Out.MoveSpeed = Speed / Habits.Num();
	const float Span = FMath::Max(Habits.Last().Time - Habits[0].Time, 1.0f);
	Out.GrapplesPerMinute = (Habits.Last().GrappleUses - Habits[0].GrappleUses) * 60.0f / Span;
	int32 Most = 0;
	for (const TPair<int32, int32>& Count : ZoneCounts)
	{
		if (Count.Value > Most)
		{
			Most = Count.Value;
			Out.FavouriteZone = Count.Key;
		}
	}
	Out.FavouriteZoneName = Arena.Zones.IsValidIndex(Out.FavouriteZone) ? Arena.Zones[Out.FavouriteZone].Name.ToString() : FString();
	return Out;
}

TArray<FString> UMatsuriBossBrainComponent::ZoneNames() const
{
	TArray<FString> Names;
	for (const FMatsuriArenaZone& Zone : Arena.Zones)
	{
		Names.Add(Zone.Name.ToString());
	}
	return Names;
}

TArray<FString> UMatsuriBossBrainComponent::LearnerHints(const FMatsuriBossObservation& Now) const
{
	TArray<TPair<double, int32>> Ranked;
	for (int32 Arm = 0; Arm < FMatsuriBossLearner::Arms; ++Arm)
	{
		if (Learner.Pulls(Arm) > 0 && Matsuri::BossSkills::AllowedIn(FMatsuriBossLearner::SkillOf(Arm), Now.Phase))
		{
			Ranked.Emplace(Learner.Estimate(Arm, Now), Arm);
		}
	}
	Ranked.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key > B.Key; });
	TArray<FString> Hints;
	for (int32 i = 0; i < FMath::Min(Ranked.Num(), HintsShown); ++i)
	{
		const int32 Arm = Ranked[i].Value;
		Hints.Add(FString::Printf(TEXT("%s#%d %+.2f (%d tries)"), Matsuri::BossSkills::Id(FMatsuriBossLearner::SkillOf(Arm)),
			FMatsuriBossLearner::PresetOf(Arm), Ranked[i].Key, Learner.Pulls(Arm)));
	}
	return Hints;
}

void UMatsuriBossBrainComponent::Prepare(const FMatsuriBossObservation& Now)
{
	if (!Client || !Client->GetSettings().IsUsable() || bThinking || IsShowcase())
	{
		return;
	}
	FMatsuriBossPromptInput Input;
	Input.Now = Now;
	for (const int32 Index : Matsuri::BossMemory::Retrieve(Store->GetSteps(), Now, Matsuri::BossPrompt::ExamplesShown))
	{
		Input.Examples.Add(Store->GetSteps()[Index]);
	}
	Input.ThisFight = ThisFight;
	Input.Lessons = Store->GetLessons();
	Input.ZoneNames = ZoneNames();
	for (const FMatsuriArenaZone& Zone : Arena.Zones)
	{
		Input.ZoneHigh.Add(Zone.bHigh);
	}
	Input.AnchorCount = Arena.Anchors.Num();
	Input.LearnerHints = LearnerHints(Now);
	const FMatsuriBossPrompt Prompt = Matsuri::BossPrompt::Decision(Input);

	const int32 Serial = ++RequestSerial;
	bThinking = true;
	Ready.Reset();
	++ModelRequests;
	const TArray<FString> Names = Input.ZoneNames;
	const int32 Anchors = Input.AnchorCount;
	TWeakObjectPtr<UMatsuriBossBrainComponent> WeakThis(this);
	Client->Chat(Prompt.System, Prompt.User, DecisionMaxTokens, false, true, [WeakThis, Serial, Names, Anchors](const FMatsuriModelReply& Reply)
	{
		UMatsuriBossBrainComponent* Brain = WeakThis.Get();
		if (!Brain || Serial != Brain->RequestSerial)
		{
			return;	// the fight is over, or this answer was superseded
		}
		Brain->bThinking = false;
		Brain->LastModelSeconds = Reply.Seconds;
		if (!Reply.bOk)
		{
			Brain->LastModelError = Reply.Error;
			UE_LOG(LogSuzuneMatsuri, Warning, TEXT("Boss model: no decision (%s) after %.1f s"), *Reply.Error, Reply.Seconds);
			return;
		}
		FMatsuriBossAction Action;
		FString Error;
		// Parsed against phase 3 here (everything legal); Decide() re-checks against the phase of the moment.
		if (Matsuri::BossPrompt::ParseDecision(Reply.Content, 3, Names, Anchors, Action, Error))
		{
			Brain->Ready = Action;
			Brain->LastModelError.Reset();
			UE_LOG(LogSuzuneMatsuri, Log, TEXT("Boss model (%.1f s, %d+%d tokens): %s - %s"), Reply.Seconds, Reply.PromptTokens, Reply.CompletionTokens,
				Matsuri::BossSkills::Id(Action.Skill), *Action.Why);
		}
		else
		{
			Brain->LastModelError = Error;
			UE_LOG(LogSuzuneMatsuri, Warning, TEXT("Boss model: unusable answer (%s): %s"), *Error, *Reply.Content.Left(200));
		}
	});
}

FMatsuriBossAction UMatsuriBossBrainComponent::FromLearner(const FMatsuriBossObservation& Now) const
{
	const int32 Arm = Learner.Choose(Now);
	if (Arm == INDEX_NONE)
	{
		return FromRules(Now);
	}
	FMatsuriBossAction Action = Matsuri::BossSkills::Preset(FMatsuriBossLearner::SkillOf(Arm), FMatsuriBossLearner::PresetOf(Arm));
	if (Action.Skill == EMatsuriBossSkill::FoxRain)
	{
		Action.Target = Now.FavouriteZone >= 0 ? Now.FavouriteZone : Now.Zone;	// rain where the player keeps going
	}
	Action.Taunt = TauntFor(Action.Skill);
	Action.Why = FString::Printf(TEXT("learner: %s#%d estimated %+.2f here after %d tries"), Matsuri::BossSkills::Id(Action.Skill),
		FMatsuriBossLearner::PresetOf(Arm), Learner.Estimate(Arm, Now), Learner.Pulls(Arm));
	Action.Source = EMatsuriBossSource::Learner;
	return Action;
}

FMatsuriBossAction UMatsuriBossBrainComponent::FromRules(const FMatsuriBossObservation& Now) const
{
	// The fixed rotation through what the phase allows.
	for (int32 Tries = 0; Tries < Matsuri::BossSkills::Count; ++Tries)
	{
		const EMatsuriBossSkill Skill = static_cast<EMatsuriBossSkill>(RuleTurn++ % Matsuri::BossSkills::Count);
		if (Matsuri::BossSkills::AllowedIn(Skill, Now.Phase))
		{
			FMatsuriBossAction Action = Matsuri::BossSkills::Preset(Skill, 1);
			Action.Taunt = TauntFor(Skill);
			Action.Why = TEXT("rules: the next skill in the rotation");
			Action.Source = EMatsuriBossSource::Rules;
			return Action;
		}
	}
	return Matsuri::BossSkills::Preset(EMatsuriBossSkill::Ring, 0);
}

bool UMatsuriBossBrainComponent::IsShowcase()
{
	return CVarShowcase.GetValueOnGameThread();
}

FMatsuriBossAction UMatsuriBossBrainComponent::Decide(const FMatsuriBossObservation& Now)
{
	if (IsShowcase())
	{
		// Every skill in turn, whatever the bar: a demo of the moves, not a fight to learn from.
		const EMatsuriBossSkill Skill = static_cast<EMatsuriBossSkill>(ShowcaseTurn++ % Matsuri::BossSkills::Count);
		FMatsuriBossAction Show = Matsuri::BossSkills::Preset(Skill, 1);
		Show.Target = Skill == EMatsuriBossSkill::FoxRain ? Now.Zone : -1;
		Show.Taunt = Matsuri::BossSkills::DisplayName(Skill).ToString();
		Show.Why = TEXT("showcase: the next of her six skills");
		Show.Source = EMatsuriBossSource::Rules;
		Ready.Reset();
		bThinking = false;
		++RequestSerial;
		LastAction = Show;
		return Show;
	}
	FMatsuriBossAction Action;
	if (Ready.IsSet() && Matsuri::BossSkills::AllowedIn(Ready->Skill, Now.Phase))
	{
		Action = Ready.GetValue();
	}
	else
	{
		if (bThinking)
		{
			UE_LOG(LogSuzuneMatsuri, Log, TEXT("Boss model still thinking: the learner decides this one"));
		}
		Action = FromLearner(Now);
	}
	Ready.Reset();
	bThinking = false;
	++RequestSerial;	// a late answer to the old prompt is ignored
	Matsuri::BossSkills::Clamp(Action, Now.Phase, Arena.Zones.Num(), Arena.Anchors.Num());
	++Decisions[static_cast<int32>(Action.Source)];
	LastAction = Action;
	return Action;
}

void UMatsuriBossBrainComponent::StepStarted(const FMatsuriBossAction& Action, const FMatsuriBossObservation& Now, const FMatsuriDanmakuStats& Stats,
	float BossHealth, float FightSeconds)
{
	if (bStepOpen)
	{
		StepFinished(Stats, BossHealth);
	}
	OpenStep = FMatsuriBossStep();
	OpenStep.FightId = FightId;
	OpenStep.Index = ThisFight.Num();
	OpenStep.Seconds = FightSeconds;
	OpenStep.Observation = Now;
	OpenStep.Action = Action;
	StatsAtStart = Stats;
	HealthAtStart = BossHealth;
	bStepOpen = true;
}

void UMatsuriBossBrainComponent::StepFinished(const FMatsuriDanmakuStats& Stats, float BossHealth)
{
	if (!bStepOpen)
	{
		return;
	}
	bStepOpen = false;
	OpenStep.Outcome.Touches = Stats.Touches - StatsAtStart.Touches;
	OpenStep.Outcome.Hits = Stats.Hits - StatsAtStart.Hits;
	OpenStep.Outcome.Grazes = Stats.Grazes - StatsAtStart.Grazes;
	OpenStep.Outcome.BossDamage = FMath::Max(0.0f, HealthAtStart - BossHealth);
	OpenStep.Reward = Matsuri::BossMemory::Reward(OpenStep.Outcome);
	ThisFight.Add(OpenStep);
	if (Store && !IsShowcase())	// a demo teaches her nothing
	{
		Store->AppendStep(OpenStep);
		Learner.Update(OpenStep.Observation, FMatsuriBossLearner::NearestArm(OpenStep.Action), OpenStep.Reward);
		Learner.Save(Store->LearnerPath());
	}
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Boss step %d: %s by %s -> touches %d, grazes %d, she lost %.0f%%: reward %+.2f"),
		OpenStep.Index, Matsuri::BossSkills::Id(OpenStep.Action.Skill),
		OpenStep.Action.Source == EMatsuriBossSource::Model ? TEXT("model") : OpenStep.Action.Source == EMatsuriBossSource::Learner ? TEXT("learner") : TEXT("rules"),
		OpenStep.Outcome.Touches, OpenStep.Outcome.Grazes, OpenStep.Outcome.BossDamage * 100.0f, OpenStep.Reward);
}

void UMatsuriBossBrainComponent::EndFight(bool bBossWon, float PlayerHealthLost, float BossHealthLeft, float Seconds)
{
	if (!Store || FightId.IsEmpty() || IsShowcase())
	{
		return;
	}
	FMatsuriBossFight Fight;
	Fight.FightId = FightId;
	Fight.Hero = HeroName;
	Fight.bBossWon = bBossWon;
	Fight.Seconds = Seconds;
	Fight.Steps = ThisFight.Num();
	float Sum = 0.0f;
	for (const FMatsuriBossStep& Step : ThisFight)
	{
		Sum += Step.Reward;
	}
	Fight.MeanReward = ThisFight.Num() > 0 ? Sum / ThisFight.Num() : 0.0f;
	Fight.PlayerHealthLost = PlayerHealthLost;
	Fight.BossHealthLeft = BossHealthLeft;
	Fight.When = FDateTime::Now().ToIso8601();
	Store->AppendFight(Fight);
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Boss fight %s over: %s, %d skills, mean reward %+.2f"), *FightId, bBossWon ? TEXT("she won") : TEXT("she lost"), Fight.Steps, Fight.MeanReward);
	Reflect(Fight);
	FightId.Reset();
}

void UMatsuriBossBrainComponent::Reflect(const FMatsuriBossFight& Fight)
{
	if (!Client || !Client->GetSettings().IsUsable() || ThisFight.Num() == 0)
	{
		return;
	}
	const FMatsuriBossPrompt Prompt = Matsuri::BossPrompt::Reflection(Fight, ThisFight, Store->GetLessons(), ZoneNames());
	// Captures the store, not this component: the reply may arrive after the level has gone.
	TSharedPtr<FMatsuriBossStore> Memory = Store;
	TSharedPtr<FMatsuriBossModelClient> KeepAlive = Client;
	Client->Chat(Prompt.System, Prompt.User, ReflectionMaxTokens, true, true, [Memory, KeepAlive](const FMatsuriModelReply& Reply)
	{
		TArray<FString> Lessons;
		if (Reply.bOk && Matsuri::BossPrompt::ParseLessons(Reply.Content, Lessons))
		{
			Memory->SetLessons(Lessons);
			UE_LOG(LogSuzuneMatsuri, Log, TEXT("Boss reflection (%.1f s): %d lessons, first: %s"), Reply.Seconds, Lessons.Num(), *Lessons[0]);
		}
		else
		{
			UE_LOG(LogSuzuneMatsuri, Warning, TEXT("Boss reflection failed: %s"), Reply.bOk ? TEXT("no lessons in the answer") : *Reply.Error);
		}
	});
}
