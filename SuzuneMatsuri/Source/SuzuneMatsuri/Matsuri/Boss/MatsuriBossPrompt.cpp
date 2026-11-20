// Suzune's Night Festival - what the boss tells the language model, and how she reads its answer.

#include "MatsuriBossPrompt.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const TCHAR* const Role =
		TEXT("You are the tactical mind of Tamamo-no-Mae, a nine-tailed fox who is the final boss of a first-person, ")
		TEXT("Touhou-style bullet-hell fight in a moonlit shrine courtyard. Each turn you choose ONE skill for her next attack. ")
		TEXT("You get better by learning from experience: you are shown similar moments from past fights with the reward each ")
		TEXT("choice earned, this fight's choices so far with their rewards, and notes you wrote after earlier fights. ")
		TEXT("Choose what will earn the highest reward now. Repeat what worked against this player; stop doing what keeps failing.\n\n")
		TEXT("Reward of one skill: +1 per bullet that reaches the player (at most 3), -1 for every 10% of her health she loses ")
		TEXT("while it runs, +0.02 per graze (a bullet passing within 90 cm; at most 0.5).\n\n")
		TEXT("The courtyard is 60 x 60 m. The player can hide behind stone lanterns, statues, the hall and wooden fences ")
		TEXT("(fences break after 25 bullets and grow back in 20 s), stand on 4 m platforms (ramps), and grapple up to 7 m ")
		TEXT("towers, the torii beam (15 m) and the hall roof (9 m). The player must pick up ammo talismans to shoot back.\n");

	FString CatalogFor(int32 Phase)
	{
		FString Out = TEXT("Skills (params are numbers 0..1):\n");
		for (int32 i = 0; i < Matsuri::BossSkills::Count; ++i)
		{
			const EMatsuriBossSkill Skill = static_cast<EMatsuriBossSkill>(i);
			Out += FString::Printf(TEXT("- %s%s: %s\n"), Matsuri::BossSkills::Id(Skill),
				Matsuri::BossSkills::AllowedIn(Skill, Phase) ? TEXT("") : TEXT(" (NOT allowed in this phase)"),
				Matsuri::BossSkills::Description(Skill));
			for (int32 p = 0; p < MatsuriBossParamCount; ++p)
			{
				Out += FString::Printf(TEXT("    %s: %s\n"), Matsuri::BossSkills::ParamName(Skill, p), Matsuri::BossSkills::ParamMeaning(Skill, p));
			}
		}
		return Out;
	}

	FString ZonesText(const TArray<FString>& Names, const TArray<bool>& High)
	{
		FString Out = TEXT("Zones (index name):");
		for (int32 i = 0; i < Names.Num(); ++i)
		{
			Out += FString::Printf(TEXT(" %d %s%s;"), i, *Names[i], High.IsValidIndex(i) && High[i] ? TEXT(" (high)") : TEXT(""));
		}
		return Out + TEXT("\n");
	}

	TSharedPtr<FJsonObject> ParseObject(const FString& Text)
	{
		TSharedPtr<FJsonObject> Json;
		const FString Body = Matsuri::BossPrompt::ExtractJson(Text);
		return !Body.IsEmpty() && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Json) ? Json : nullptr;
	}
}

FString Matsuri::BossPrompt::ExtractJson(const FString& Text)
{
	int32 Start = INDEX_NONE;
	int32 Depth = 0;
	bool bInString = false;
	for (int32 i = 0; i < Text.Len(); ++i)
	{
		const TCHAR C = Text[i];
		if (Start != INDEX_NONE && bInString)
		{
			if (C == TEXT('\\'))
			{
				++i;
			}
			else if (C == TEXT('"'))
			{
				bInString = false;
			}
			continue;
		}
		if (C == TEXT('"') && Start != INDEX_NONE)
		{
			bInString = true;
		}
		else if (C == TEXT('{'))
		{
			Start = Start == INDEX_NONE ? i : Start;
			++Depth;
		}
		else if (C == TEXT('}') && Start != INDEX_NONE && --Depth == 0)
		{
			return Text.Mid(Start, i - Start + 1);
		}
	}
	return FString();
}

FMatsuriBossPrompt Matsuri::BossPrompt::Decision(const FMatsuriBossPromptInput& Input)
{
	const int32 Phase = Input.Now.Phase;
	FMatsuriBossPrompt Prompt;
	Prompt.System = FString(Role) + TEXT("\n") + ZonesText(Input.ZoneNames, Input.ZoneHigh)
		+ FString::Printf(TEXT("Anchors (where phantom_step can take her): 0..%d, or -1 for the one farthest from the player.\n\n"), Input.AnchorCount - 1)
		+ CatalogFor(Phase)
		+ TEXT("\nReply with ONE JSON object and nothing else:\n")
		+ TEXT("{\"skill\":\"<id>\",\"params\":{\"<param name>\":<0..1>, ...},\"target\":<fox_rain: the zone NAME in quotes, e.g. \"lantern_sw\", or \"on_player\"; phantom_step: anchor index; else -1>,")
		+ TEXT("\"taunt\":\"<at most 20 characters of Japanese, in character>\",\"why\":\"<one short sentence citing the evidence>\"}");

	FString User;
	User += TEXT("Your notes from earlier fights:\n");
	if (Input.Lessons.Num() == 0)
	{
		User += TEXT("- (none yet: this may be your first fight)\n");
	}
	for (const FString& Lesson : Input.Lessons)
	{
		User += TEXT("- ") + Lesson.Left(MaxLessonChars) + TEXT("\n");
	}
	User += TEXT("\nSimilar moments from past fights (situation -> choice -> result):\n");
	if (Input.Examples.Num() == 0)
	{
		User += TEXT("- (no records yet)\n");
	}
	for (int32 i = 0; i < FMath::Min(Input.Examples.Num(), ExamplesShown); ++i)
	{
		User += TEXT("- ") + Matsuri::BossMemory::Describe(Input.Examples[i], Input.ZoneNames) + TEXT("\n");
	}
	User += TEXT("\nThis fight so far, oldest first:\n");
	if (Input.ThisFight.Num() == 0)
	{
		User += TEXT("- (the fight has just begun)\n");
	}
	const int32 First = FMath::Max(0, Input.ThisFight.Num() - TrajectoryShown);
	for (int32 i = First; i < Input.ThisFight.Num(); ++i)
	{
		User += TEXT("- ") + Matsuri::BossMemory::Describe(Input.ThisFight[i], Input.ZoneNames) + TEXT("\n");
	}
	if (Input.LearnerHints.Num() > 0)
	{
		User += TEXT("\nA statistical learner's reward estimates for this situation: ") + FString::Join(Input.LearnerHints, TEXT(", ")) + TEXT("\n");
	}
	FString Allowed;
	for (int32 i = 0; i < Matsuri::BossSkills::Count; ++i)
	{
		if (Matsuri::BossSkills::AllowedIn(static_cast<EMatsuriBossSkill>(i), Phase))
		{
			Allowed += FString(Allowed.IsEmpty() ? TEXT("") : TEXT(", ")) + Matsuri::BossSkills::Id(static_cast<EMatsuriBossSkill>(i));
		}
	}
	User += FString::Printf(TEXT("\nNow: %s.\nAllowed skills: %s. Choose her next skill."), *Matsuri::BossMemory::Describe(Input.Now), *Allowed);
	Prompt.User = User;
	return Prompt;
}

bool Matsuri::BossPrompt::ParseDecision(const FString& Reply, int32 Phase, const TArray<FString>& ZoneNames, int32 AnchorCount,
	FMatsuriBossAction& OutAction, FString& OutError)
{
	const TSharedPtr<FJsonObject> Json = ParseObject(Reply);
	if (!Json)
	{
		OutError = TEXT("no JSON object in the reply");
		return false;
	}
	FMatsuriBossAction Action;
	FString SkillId;
	if (!Json->TryGetStringField(TEXT("skill"), SkillId) || !Matsuri::BossSkills::FromId(SkillId.TrimStartAndEnd(), Action.Skill))
	{
		OutError = FString::Printf(TEXT("unknown skill '%s'"), *SkillId);
		return false;
	}
	const TSharedPtr<FJsonObject>* Named = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Positional = nullptr;
	if (Json->TryGetObjectField(TEXT("params"), Named))
	{
		for (int32 p = 0; p < MatsuriBossParamCount; ++p)
		{
			double Value = 0.5;
			if ((*Named)->TryGetNumberField(Matsuri::BossSkills::ParamName(Action.Skill, p), Value))
			{
				Action.Params[p] = static_cast<float>(Value);
			}
		}
	}
	else if (Json->TryGetArrayField(TEXT("params"), Positional))
	{
		for (int32 p = 0; p < FMath::Min(Positional->Num(), MatsuriBossParamCount); ++p)
		{
			Action.Params[p] = static_cast<float>((*Positional)[p]->AsNumber());
		}
	}
	double TargetNumber = -1.0;
	FString TargetName;
	if (Json->TryGetNumberField(TEXT("target"), TargetNumber))
	{
		Action.Target = FMath::RoundToInt32(TargetNumber);
	}
	else if (Json->TryGetStringField(TEXT("target"), TargetName))
	{
		Action.Target = ZoneNames.IndexOfByKey(TargetName.TrimStartAndEnd());	// -1 when unknown: "on the player"
	}
	Json->TryGetStringField(TEXT("taunt"), Action.Taunt);
	Json->TryGetStringField(TEXT("why"), Action.Why);
	Action.Source = EMatsuriBossSource::Model;
	if (!Matsuri::BossSkills::Clamp(Action, Phase, ZoneNames.Num(), AnchorCount))
	{
		OutError = FString::Printf(TEXT("%s is not allowed in phase %d"), *SkillId, Phase);
		return false;
	}
	OutAction = Action;
	return true;
}

FMatsuriBossPrompt Matsuri::BossPrompt::Reflection(const FMatsuriBossFight& Fight, const TArray<FMatsuriBossStep>& Steps,
	const TArray<FString>& Lessons, const TArray<FString>& ZoneNames)
{
	FMatsuriBossPrompt Prompt;
	Prompt.System = FString(Role)
		+ TEXT("\nA fight has just ended. Review it and rewrite your notebook: at most 10 short, concrete lessons of the form ")
		+ TEXT("'when <situation>, <do this> (because <evidence>)'. Keep old lessons that still hold, correct the ones the fight ")
		+ TEXT("disproved, add what you learned about this player. Each lesson under 140 characters.\n")
		+ TEXT("Reply with ONE JSON object and nothing else: {\"lessons\":[\"...\", ...]}");
	FString User = FString::Printf(TEXT("Fight %s against %s: %s after %.0f s, %d skills, mean reward %+.2f, the player lost %s, she ended with %.0f%% health.\n\nOld notebook:\n"),
		*Fight.FightId, *Fight.Hero, Fight.bBossWon ? TEXT("SHE WON") : TEXT("SHE WAS DEFEATED"), Fight.Seconds, Fight.Steps,
		Fight.MeanReward,
		*(Fight.bHealthKnown ? FString::Printf(TEXT("%.0f HP"), Fight.PlayerHealthLost)
			: FString::Printf(TEXT("%d lives (legacy health system)"), Fight.PlayerLivesLost)), Fight.BossHealthLeft * 100.0f);
	if (Lessons.Num() == 0)
	{
		User += TEXT("- (empty)\n");
	}
	for (const FString& Lesson : Lessons)
	{
		User += TEXT("- ") + Lesson + TEXT("\n");
	}
	User += TEXT("\nEvery skill of the fight:\n");
	for (const FMatsuriBossStep& Step : Steps)
	{
		User += TEXT("- ") + Matsuri::BossMemory::Describe(Step, ZoneNames) + TEXT("\n");
	}
	Prompt.User = User;
	return Prompt;
}

bool Matsuri::BossPrompt::ParseLessons(const FString& Reply, TArray<FString>& OutLessons)
{
	const TSharedPtr<FJsonObject> Json = ParseObject(Reply);
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Json || !Json->TryGetArrayField(TEXT("lessons"), Items))
	{
		return false;
	}
	TArray<FString> Lessons;
	for (const TSharedPtr<FJsonValue>& Item : *Items)
	{
		FString Lesson;
		if (Item->TryGetString(Lesson) && !Lesson.TrimStartAndEnd().IsEmpty())
		{
			Lessons.Add(Lesson.TrimStartAndEnd().Left(MaxLessonChars));
		}
		if (Lessons.Num() == Matsuri::BossMemory::MaxLessons)
		{
			break;
		}
	}
	if (Lessons.Num() == 0)
	{
		return false;
	}
	OutLessons = Lessons;
	return true;
}
