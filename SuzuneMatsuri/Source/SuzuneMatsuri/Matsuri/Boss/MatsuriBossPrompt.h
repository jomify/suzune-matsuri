// Suzune's Night Festival - what the boss tells the language model, and how she reads its answer.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriBossMemory.h"

/** Everything one decision prompt is built from. */
struct FMatsuriBossPromptInput
{
	FMatsuriBossObservation Now;
	TArray<FMatsuriBossStep> Examples;		// retrieved from past fights (in-context learning)
	TArray<FMatsuriBossStep> ThisFight;		// the latest steps of this fight with rewards (in-context RL)
	TArray<FString> Lessons;				// her notebook, rewritten after each fight
	TArray<FString> ZoneNames;
	TArray<bool> ZoneHigh;
	int32 AnchorCount = 0;
	TArray<FString> LearnerHints;			// "fox_rain#1 +0.80": the bandit's estimates here
};

/** A system and a user message. */
struct FMatsuriBossPrompt
{
	FString System;
	FString User;
};

namespace Matsuri::BossPrompt
{
	constexpr int32 ExamplesShown = 8;
	constexpr int32 TrajectoryShown = 10;
	constexpr int32 MaxLessonChars = 140;

	SUZUNEMATSURI_API FMatsuriBossPrompt Decision(const FMatsuriBossPromptInput& Input);

	/**
	 *  Reads the model's reply into an action: tolerates code fences and prose around the JSON, named or
	 *  positional params, a zone by name or index. False (with Error) when there is no valid allowed skill.
	 */
	SUZUNEMATSURI_API bool ParseDecision(const FString& Reply, int32 Phase, const TArray<FString>& ZoneNames, int32 AnchorCount,
		FMatsuriBossAction& OutAction, FString& OutError);

	/** After a fight: the fight's steps and the old notebook in, a better notebook out. */
	SUZUNEMATSURI_API FMatsuriBossPrompt Reflection(const FMatsuriBossFight& Fight, const TArray<FMatsuriBossStep>& Steps,
		const TArray<FString>& Lessons, const TArray<FString>& ZoneNames);

	SUZUNEMATSURI_API bool ParseLessons(const FString& Reply, TArray<FString>& OutLessons);

	/** The first {...} object in Text (models wrap JSON in prose or ``` fences). */
	SUZUNEMATSURI_API FString ExtractJson(const FString& Text);
}
