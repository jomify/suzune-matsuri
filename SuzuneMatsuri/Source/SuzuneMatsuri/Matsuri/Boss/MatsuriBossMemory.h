// Suzune's Night Festival - what Tamamo sees, what she remembers of past fights, and how she scores herself.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriBossSkills.h"

/** The situation at the moment she chooses a skill. */
struct FMatsuriBossObservation
{
	int32 Phase = 1;
	float BossHealth = 1.0f;		// 0..1 of her whole life
	FString Hero;					// sakura / suzune / mio
	float PlayerHealth = 200.0f;
	float PlayerMaxHealth = 200.0f;
	bool bHealthKnown = true;
	int32 PlayerLives = 3; // read-only interpretation of pre-HP historical records
	int32 PlayerAmmo = 0;
	int32 PlayerMaxAmmo = 12;
	int32 Zone = -1;				// arena zone the player is in (-1 = the open)
	FString ZoneName;
	bool bPlayerHigh = false;		// on a platform or perch
	bool bPlayerShielded = false;	// solid cover between her and the player
	bool bPlayerAirborne = false;	// jumping, falling or on the grapple
	float DistanceMeters = 0.0f;
	// Habits over the last 20 seconds.
	float CoverShare = 0.0f;		// of the time shielded
	float HighShare = 0.0f;			// of the time up high
	float GrapplesPerMinute = 0.0f;
	float MoveSpeed = 0.0f;			// m/s on average
	int32 FavouriteZone = -1;		// where the player spent most of it
	FString FavouriteZoneName;

	/** The numbers retrieval and the learner compare (all roughly 0..1). */
	TArray<float> Features() const;
};

/** What happened while one skill ran (the field's counters and her health, before and after). */
struct FMatsuriBossOutcome
{
	int32 Touches = 0;			// her bullets that reached the player's hit sphere
	int32 Hits = 0;				// of those, the ones that actually reduced player HP
	int32 Grazes = 0;
	float BossDamage = 0.0f;	// 0..1 of her whole life lost meanwhile
};

/** One decision and its result: a line of the fight record. */
struct FMatsuriBossStep
{
	FString FightId;
	int32 Index = 0;
	float Seconds = 0.0f;		// into the fight
	FMatsuriBossObservation Observation;
	FMatsuriBossAction Action;
	FMatsuriBossOutcome Outcome;
	float Reward = 0.0f;
};

/** A finished fight. */
struct FMatsuriBossFight
{
	FString FightId;
	FString Hero;
	bool bBossWon = false;
	float Seconds = 0.0f;
	int32 Steps = 0;
	float MeanReward = 0.0f;
	float PlayerHealthLost = 0.0f;
	bool bHealthKnown = true;
	int32 PlayerLivesLost = 0; // legacy historical summaries only
	float BossHealthLeft = 0.0f;
	FString When;				// ISO time
};

namespace Matsuri::BossMemory
{
	constexpr int32 MaxLessons = 10;

	/**
	 *  The reward she learns from (per skill):
	 *    +1 per bullet that reached the player (at most 3),
	 *    -1 per tenth of her life she lost meanwhile,
	 *    +0.02 per graze (at most 0.5): pressure that nearly worked.
	 */
	SUZUNEMATSURI_API float Reward(const FMatsuriBossOutcome& Outcome);

	/**
	 *  The K past steps most like Now: half the best-rewarded and half the worst of its closest
	 *  neighbours (so the model sees both what worked and what did not). Indices into Steps.
	 */
	SUZUNEMATSURI_API TArray<int32> Retrieve(const TArray<FMatsuriBossStep>& Steps, const FMatsuriBossObservation& Now, int32 K);

	SUZUNEMATSURI_API TSharedRef<class FJsonObject> ToJson(const FMatsuriBossStep& Step);
	SUZUNEMATSURI_API bool FromJson(const class FJsonObject& Json, FMatsuriBossStep& OutStep);
	SUZUNEMATSURI_API TSharedRef<class FJsonObject> ToJson(const FMatsuriBossFight& Fight);
	SUZUNEMATSURI_API bool FromJson(const class FJsonObject& Json, FMatsuriBossFight& OutFight);

	/** One compact line for a prompt ("phase 2, player at lantern_sw (shielded) ... -> fox_rain ... reward +1.2"). */
	SUZUNEMATSURI_API FString Describe(const FMatsuriBossStep& Step, const TArray<FString>& ZoneNames);
	SUZUNEMATSURI_API FString Describe(const FMatsuriBossObservation& Observation);
}

/**
 *  Her memory on disk (Saved/BossMemory by default; tests use their own folder): every step of every
 *  fight (steps.jsonl), every fight (fights.jsonl), the lessons she wrote after fights (notebook.json)
 *  and the local learner (learner.json). Safe to share between the fight and a late reflection reply.
 */
class SUZUNEMATSURI_API FMatsuriBossStore
{
public:
	explicit FMatsuriBossStore(const FString& InFolder);

	/** The default folder, Saved/BossMemory (matsuri.BossMemory overrides it, for tests). */
	static FString DefaultFolder();

	void Load();
	void AppendStep(const FMatsuriBossStep& Step);
	void AppendFight(const FMatsuriBossFight& Fight);
	void SetLessons(const TArray<FString>& InLessons);

	const TArray<FMatsuriBossStep>& GetSteps() const { return Steps; }
	const TArray<FMatsuriBossFight>& GetFights() const { return Fights; }
	const TArray<FString>& GetLessons() const { return Lessons; }
	const FString& GetFolder() const { return Folder; }
	FString LearnerPath() const;

	/** Deletes everything in the folder (tests). */
	void Wipe();

private:
	FString Folder;
	TArray<FMatsuriBossStep> Steps;
	TArray<FMatsuriBossFight> Fights;
	TArray<FString> Lessons;
	FCriticalSection Lock;
};
