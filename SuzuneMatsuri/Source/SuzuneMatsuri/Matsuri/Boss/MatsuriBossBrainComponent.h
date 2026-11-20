// Suzune's Night Festival - Tamamo's brain: chooses each skill, scores it, and learns across fights.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MatsuriBossLearner.h"
#include "MatsuriBossMemory.h"
#include "MatsuriBossArena.h"
#include "MatsuriDanmakuField.h"
#include "MatsuriBossBrainComponent.generated.h"

class FMatsuriBossModelClient;

/**
 *  How she decides (each skill):
 *   1. While the current skill runs, Prepare() sends the language model a prompt built from her memory:
 *      similar moments from past fights with their rewards (in-context learning), this fight's choices
 *      and rewards so far (in-context reinforcement learning), and her notebook of lessons.
 *   2. Decide() takes the model's answer if it came back in time and is a legal skill; otherwise the
 *      local learner (LinUCB over the same records) decides, and if even that has nothing, the rules.
 *   3. StepFinished() scores the skill (Matsuri::BossMemory::Reward), writes it to her memory and
 *      teaches the learner. EndFight() asks the model to rewrite the notebook from the whole fight.
 */
UCLASS(ClassGroup=(Matsuri))
class SUZUNEMATSURI_API UMatsuriBossBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMatsuriBossBrainComponent();

	void BeginFight(const FString& Hero, const FMatsuriBossArena& InArena);

	/** Feeds the habit tracker (call every frame with what she sees right now). */
	void Observe(float DeltaSeconds, const FMatsuriBossObservation& Raw, int32 GrappleUses);

	/** Raw plus the habits of the last 20 seconds. */
	FMatsuriBossObservation Complete(const FMatsuriBossObservation& Raw) const;

	/** Starts thinking about the next skill (no-op if the model is off or already thinking). */
	void Prepare(const FMatsuriBossObservation& Now);

	FMatsuriBossAction Decide(const FMatsuriBossObservation& Now);

	void StepStarted(const FMatsuriBossAction& Action, const FMatsuriBossObservation& Now, const FMatsuriDanmakuStats& Stats, float BossHealth, float FightSeconds);
	void StepFinished(const FMatsuriDanmakuStats& Stats, float BossHealth);

	void EndFight(bool bBossWon, float PlayerHealthLost, float BossHealthLeft, float Seconds);

	// What the debug panel and tests read.
	const FMatsuriBossAction& GetLastAction() const { return LastAction; }
	/** matsuri.BossShowcase: her six skills in order, nothing recorded. */
	static bool IsShowcase();

	/** A model answer is on its way (the boss may hold her next skill a moment for it). */
	bool IsThinking() const { return bThinking; }
	double GetLastModelSeconds() const { return LastModelSeconds; }
	const FString& GetLastModelError() const { return LastModelError; }
	FString GetProvider() const;
	int32 GetDecisions(EMatsuriBossSource Source) const { return Decisions[static_cast<int32>(Source)]; }
	int32 GetModelRequests() const { return ModelRequests; }
	const TArray<FMatsuriBossStep>& GetThisFight() const { return ThisFight; }
	TSharedPtr<FMatsuriBossStore> GetStore() const { return Store; }
	const FMatsuriBossLearner& GetLearner() const { return Learner; }

	static constexpr float HabitWindowSeconds = 20.0f;
	static constexpr float HabitSampleSeconds = 0.25f;

private:
	struct FHabitSample
	{
		float Time = 0.0f;
		bool bShielded = false;
		bool bHigh = false;
		int32 Zone = -1;
		int32 GrappleUses = 0;
		float Speed = 0.0f;
	};

	FMatsuriBossAction FromLearner(const FMatsuriBossObservation& Now) const;
	FMatsuriBossAction FromRules(const FMatsuriBossObservation& Now) const;
	TArray<FString> ZoneNames() const;
	TArray<FString> LearnerHints(const FMatsuriBossObservation& Now) const;
	void Reflect(const FMatsuriBossFight& Fight);

	FMatsuriBossArena Arena;
	TSharedPtr<FMatsuriBossStore> Store;
	TSharedPtr<FMatsuriBossModelClient> Client;
	FMatsuriBossLearner Learner;
	FString FightId;
	FString HeroName;
	TArray<FMatsuriBossStep> ThisFight;
	TArray<FHabitSample> Habits;
	float HabitClock = 0.0f;
	float SinceSample = 0.0f;

	// The step in progress (scored when the next one starts).
	bool bStepOpen = false;
	FMatsuriBossStep OpenStep;
	FMatsuriDanmakuStats StatsAtStart;
	float HealthAtStart = 1.0f;

	// The model's answer for the next decision.
	int32 RequestSerial = 0;
	bool bThinking = false;
	TOptional<FMatsuriBossAction> Ready;
	double LastModelSeconds = 0.0;
	FString LastModelError;
	int32 ModelRequests = 0;

	FMatsuriBossAction LastAction;
	int32 Decisions[3] = {};
	mutable int32 RuleTurn = 0;
	int32 ShowcaseTurn = 0;
};
