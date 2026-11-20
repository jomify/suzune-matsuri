// Suzune's Night Festival - what Tamamo sees, what she remembers of past fights, and how she scores herself.

#include "MatsuriBossMemory.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "SuzuneMatsuri.h"

namespace
{
	TAutoConsoleVariable<FString> CVarMemoryFolder(
		TEXT("matsuri.BossMemory"), TEXT(""),
		TEXT("Folder for the boss's fight memory (empty = Saved/BossMemory). Tests point it elsewhere."));

	constexpr int32 MaxTouchesRewarded = 3;
	constexpr float GrazeReward = 0.02f;
	constexpr float MaxGrazeReward = 0.5f;
	constexpr float DamagePenaltyPerTenth = 1.0f;
	constexpr int32 NeighbourPoolFactor = 3;	// look at 3K nearest, keep the best and worst halves
	constexpr float PhaseMismatchDistance = 2.0f;

	const TCHAR* const StepsFile = TEXT("steps.jsonl");
	const TCHAR* const FightsFile = TEXT("fights.jsonl");
	const TCHAR* const NotebookFile = TEXT("notebook.json");
	const TCHAR* const LearnerFile = TEXT("learner.json");

	FString Condensed(const TSharedRef<FJsonObject>& Json)
	{
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Json, Writer);
		return Out;
	}

	TSharedPtr<FJsonObject> Parse(const FString& Line)
	{
		TSharedPtr<FJsonObject> Json;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
		return FJsonSerializer::Deserialize(Reader, Json) ? Json : nullptr;
	}

	const TCHAR* SourceName(EMatsuriBossSource Source)
	{
		switch (Source)
		{
		case EMatsuriBossSource::Rules:		return TEXT("rules");
		case EMatsuriBossSource::Learner:	return TEXT("learner");
		case EMatsuriBossSource::Model:		return TEXT("model");
		}
		return TEXT("rules");
	}

	EMatsuriBossSource SourceFrom(const FString& Name)
	{
		return Name == TEXT("model") ? EMatsuriBossSource::Model : Name == TEXT("learner") ? EMatsuriBossSource::Learner : EMatsuriBossSource::Rules;
	}

	float Distance(const TArray<float>& A, const TArray<float>& B)
	{
		float Sum = 0.0f;
		for (int32 i = 0; i < FMath::Min(A.Num(), B.Num()); ++i)
		{
			Sum += FMath::Square(A[i] - B[i]);
		}
		return FMath::Sqrt(Sum);
	}
}

TArray<float> FMatsuriBossObservation::Features() const
{
	const float Ammo = PlayerMaxAmmo > 0 ? FMath::Clamp(static_cast<float>(PlayerAmmo) / PlayerMaxAmmo, 0.0f, 1.0f) : 0.0f;
	return {
		Phase == 2 ? 1.0f : 0.0f,
		Phase >= 3 ? 1.0f : 0.0f,
		bPlayerShielded ? 1.0f : 0.0f,
		bPlayerHigh ? 1.0f : 0.0f,
		bPlayerAirborne ? 1.0f : 0.0f,
		FMath::Clamp(DistanceMeters / 40.0f, 0.0f, 1.0f),
		Ammo,
		FMath::Clamp(CoverShare, 0.0f, 1.0f),
		FMath::Clamp(HighShare, 0.0f, 1.0f),
	};
}

float Matsuri::BossMemory::Reward(const FMatsuriBossOutcome& Outcome)
{
	return FMath::Min(Outcome.Touches, MaxTouchesRewarded)
		- DamagePenaltyPerTenth * Outcome.BossDamage * 10.0f
		+ FMath::Min(Outcome.Grazes * GrazeReward, MaxGrazeReward);
}

TArray<int32> Matsuri::BossMemory::Retrieve(const TArray<FMatsuriBossStep>& Steps, const FMatsuriBossObservation& Now, int32 K)
{
	const TArray<float> Here = Now.Features();
	TArray<TPair<float, int32>> ByDistance;
	ByDistance.Reserve(Steps.Num());
	for (int32 i = 0; i < Steps.Num(); ++i)
	{
		const FMatsuriBossObservation& Then = Steps[i].Observation;
		const float Penalty = Then.Phase == Now.Phase ? 0.0f : PhaseMismatchDistance;	// same phase first: other skills were possible
		ByDistance.Emplace(Distance(Here, Then.Features()) + Penalty, i);
	}
	ByDistance.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	ByDistance.SetNum(FMath::Min(ByDistance.Num(), K * NeighbourPoolFactor));

	TArray<int32> Pool;
	for (const TPair<float, int32>& Entry : ByDistance)
	{
		Pool.Add(Entry.Value);
	}
	Pool.Sort([&Steps](int32 A, int32 B) { return Steps[A].Reward > Steps[B].Reward; });
	if (Pool.Num() <= K)
	{
		return Pool;
	}
	TArray<int32> Out;
	const int32 Best = (K + 1) / 2;
	for (int32 i = 0; i < Best; ++i)
	{
		Out.Add(Pool[i]);
	}
	for (int32 i = 0; i < K - Best; ++i)
	{
		Out.Add(Pool[Pool.Num() - 1 - i]);
	}
	return Out;
}

TSharedRef<FJsonObject> Matsuri::BossMemory::ToJson(const FMatsuriBossStep& Step)
{
	const FMatsuriBossObservation& O = Step.Observation;
	TSharedRef<FJsonObject> Obs = MakeShared<FJsonObject>();
	Obs->SetNumberField(TEXT("phase"), O.Phase);
	Obs->SetNumberField(TEXT("boss_health"), O.BossHealth);
	Obs->SetStringField(TEXT("hero"), O.Hero);
	if (O.bHealthKnown)
	{
		Obs->SetNumberField(TEXT("health"), O.PlayerHealth);
		Obs->SetNumberField(TEXT("max_health"), O.PlayerMaxHealth);
	}
	else
	{
		Obs->SetNumberField(TEXT("lives"), O.PlayerLives);
	}
	Obs->SetNumberField(TEXT("ammo"), O.PlayerAmmo);
	Obs->SetNumberField(TEXT("max_ammo"), O.PlayerMaxAmmo);
	Obs->SetNumberField(TEXT("zone"), O.Zone);
	Obs->SetStringField(TEXT("zone_name"), O.ZoneName);
	Obs->SetBoolField(TEXT("high"), O.bPlayerHigh);
	Obs->SetBoolField(TEXT("shielded"), O.bPlayerShielded);
	Obs->SetBoolField(TEXT("airborne"), O.bPlayerAirborne);
	Obs->SetNumberField(TEXT("distance_m"), O.DistanceMeters);
	Obs->SetNumberField(TEXT("cover_share"), O.CoverShare);
	Obs->SetNumberField(TEXT("high_share"), O.HighShare);
	Obs->SetNumberField(TEXT("grapples_per_min"), O.GrapplesPerMinute);
	Obs->SetNumberField(TEXT("move_speed"), O.MoveSpeed);
	Obs->SetNumberField(TEXT("favourite_zone"), O.FavouriteZone);
	Obs->SetStringField(TEXT("favourite_zone_name"), O.FavouriteZoneName);

	TSharedRef<FJsonObject> Act = MakeShared<FJsonObject>();
	Act->SetStringField(TEXT("skill"), Matsuri::BossSkills::Id(Step.Action.Skill));
	TArray<TSharedPtr<FJsonValue>> Params;
	for (const float Value : Step.Action.Params)
	{
		Params.Add(MakeShared<FJsonValueNumber>(Value));
	}
	Act->SetArrayField(TEXT("params"), Params);
	Act->SetNumberField(TEXT("target"), Step.Action.Target);
	Act->SetStringField(TEXT("source"), SourceName(Step.Action.Source));
	Act->SetStringField(TEXT("taunt"), Step.Action.Taunt);
	Act->SetStringField(TEXT("why"), Step.Action.Why);

	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetNumberField(TEXT("touches"), Step.Outcome.Touches);
	Out->SetNumberField(TEXT("hits"), Step.Outcome.Hits);
	Out->SetNumberField(TEXT("grazes"), Step.Outcome.Grazes);
	Out->SetNumberField(TEXT("boss_damage"), Step.Outcome.BossDamage);

	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("fight"), Step.FightId);
	Json->SetNumberField(TEXT("index"), Step.Index);
	Json->SetNumberField(TEXT("seconds"), Step.Seconds);
	Json->SetObjectField(TEXT("obs"), Obs);
	Json->SetObjectField(TEXT("action"), Act);
	Json->SetObjectField(TEXT("outcome"), Out);
	Json->SetNumberField(TEXT("reward"), Step.Reward);
	return Json;
}

bool Matsuri::BossMemory::FromJson(const FJsonObject& Json, FMatsuriBossStep& OutStep)
{
	const TSharedPtr<FJsonObject>* Obs = nullptr;
	const TSharedPtr<FJsonObject>* Act = nullptr;
	if (!Json.TryGetObjectField(TEXT("obs"), Obs) || !Json.TryGetObjectField(TEXT("action"), Act))
	{
		return false;
	}
	FMatsuriBossStep Step;
	Step.FightId = Json.GetStringField(TEXT("fight"));
	Step.Index = static_cast<int32>(Json.GetNumberField(TEXT("index")));
	Step.Seconds = static_cast<float>(Json.GetNumberField(TEXT("seconds")));
	Step.Reward = static_cast<float>(Json.GetNumberField(TEXT("reward")));

	FMatsuriBossObservation& O = Step.Observation;
	const FJsonObject& Ob = **Obs;
	O.Phase = static_cast<int32>(Ob.GetNumberField(TEXT("phase")));
	O.BossHealth = static_cast<float>(Ob.GetNumberField(TEXT("boss_health")));
	Ob.TryGetStringField(TEXT("hero"), O.Hero);
	double PlayerHealth = 0.0;
	double PlayerMaxHealth = 200.0;
	O.bHealthKnown = Ob.TryGetNumberField(TEXT("health"), PlayerHealth) && Ob.TryGetNumberField(TEXT("max_health"), PlayerMaxHealth);
	if (O.bHealthKnown)
	{
		O.PlayerMaxHealth = FMath::Max(1.0f, static_cast<float>(PlayerMaxHealth));
		O.PlayerHealth = FMath::Clamp(static_cast<float>(PlayerHealth), 0.0f, O.PlayerMaxHealth);
	}
	else
	{
		double LegacyLives = 0.0;
		Ob.TryGetNumberField(TEXT("lives"), LegacyLives);
		O.PlayerLives = static_cast<int32>(LegacyLives);
	}
	O.PlayerAmmo = static_cast<int32>(Ob.GetNumberField(TEXT("ammo")));
	O.PlayerMaxAmmo = static_cast<int32>(Ob.GetNumberField(TEXT("max_ammo")));
	O.Zone = static_cast<int32>(Ob.GetNumberField(TEXT("zone")));
	Ob.TryGetStringField(TEXT("zone_name"), O.ZoneName);
	O.bPlayerHigh = Ob.GetBoolField(TEXT("high"));
	O.bPlayerShielded = Ob.GetBoolField(TEXT("shielded"));
	O.bPlayerAirborne = Ob.GetBoolField(TEXT("airborne"));
	O.DistanceMeters = static_cast<float>(Ob.GetNumberField(TEXT("distance_m")));
	O.CoverShare = static_cast<float>(Ob.GetNumberField(TEXT("cover_share")));
	O.HighShare = static_cast<float>(Ob.GetNumberField(TEXT("high_share")));
	O.GrapplesPerMinute = static_cast<float>(Ob.GetNumberField(TEXT("grapples_per_min")));
	O.MoveSpeed = static_cast<float>(Ob.GetNumberField(TEXT("move_speed")));
	O.FavouriteZone = static_cast<int32>(Ob.GetNumberField(TEXT("favourite_zone")));
	Ob.TryGetStringField(TEXT("favourite_zone_name"), O.FavouriteZoneName);

	const FJsonObject& Ac = **Act;
	if (!Matsuri::BossSkills::FromId(Ac.GetStringField(TEXT("skill")), Step.Action.Skill))
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Params = nullptr;
	if (Ac.TryGetArrayField(TEXT("params"), Params))
	{
		for (int32 i = 0; i < FMath::Min(Params->Num(), MatsuriBossParamCount); ++i)
		{
			Step.Action.Params[i] = static_cast<float>((*Params)[i]->AsNumber());
		}
	}
	Step.Action.Target = static_cast<int32>(Ac.GetNumberField(TEXT("target")));
	Step.Action.Source = SourceFrom(Ac.GetStringField(TEXT("source")));
	Ac.TryGetStringField(TEXT("taunt"), Step.Action.Taunt);
	Ac.TryGetStringField(TEXT("why"), Step.Action.Why);

	const TSharedPtr<FJsonObject>* Out = nullptr;
	if (Json.TryGetObjectField(TEXT("outcome"), Out))
	{
		Step.Outcome.Touches = static_cast<int32>((*Out)->GetNumberField(TEXT("touches")));
		Step.Outcome.Hits = static_cast<int32>((*Out)->GetNumberField(TEXT("hits")));
		Step.Outcome.Grazes = static_cast<int32>((*Out)->GetNumberField(TEXT("grazes")));
		Step.Outcome.BossDamage = static_cast<float>((*Out)->GetNumberField(TEXT("boss_damage")));
	}
	OutStep = Step;
	return true;
}

TSharedRef<FJsonObject> Matsuri::BossMemory::ToJson(const FMatsuriBossFight& Fight)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("fight"), Fight.FightId);
	Json->SetStringField(TEXT("hero"), Fight.Hero);
	Json->SetBoolField(TEXT("boss_won"), Fight.bBossWon);
	Json->SetNumberField(TEXT("seconds"), Fight.Seconds);
	Json->SetNumberField(TEXT("steps"), Fight.Steps);
	Json->SetNumberField(TEXT("mean_reward"), Fight.MeanReward);
	if (Fight.bHealthKnown)
	{
		Json->SetNumberField(TEXT("player_health_lost"), Fight.PlayerHealthLost);
	}
	else
	{
		Json->SetNumberField(TEXT("player_lives_lost"), Fight.PlayerLivesLost);
	}
	Json->SetNumberField(TEXT("boss_health_left"), Fight.BossHealthLeft);
	Json->SetStringField(TEXT("when"), Fight.When);
	return Json;
}

bool Matsuri::BossMemory::FromJson(const FJsonObject& Json, FMatsuriBossFight& OutFight)
{
	if (!Json.HasField(TEXT("fight")))
	{
		return false;
	}
	OutFight.FightId = Json.GetStringField(TEXT("fight"));
	Json.TryGetStringField(TEXT("hero"), OutFight.Hero);
	OutFight.bBossWon = Json.GetBoolField(TEXT("boss_won"));
	OutFight.Seconds = static_cast<float>(Json.GetNumberField(TEXT("seconds")));
	OutFight.Steps = static_cast<int32>(Json.GetNumberField(TEXT("steps")));
	OutFight.MeanReward = static_cast<float>(Json.GetNumberField(TEXT("mean_reward")));
	double PlayerHealthLost = 0.0;
	OutFight.bHealthKnown = Json.TryGetNumberField(TEXT("player_health_lost"), PlayerHealthLost);
	OutFight.PlayerHealthLost = static_cast<float>(PlayerHealthLost);
	if (!OutFight.bHealthKnown)
	{
		double LegacyLivesLost = 0.0;
		Json.TryGetNumberField(TEXT("player_lives_lost"), LegacyLivesLost);
		OutFight.PlayerLivesLost = static_cast<int32>(LegacyLivesLost);
	}
	OutFight.BossHealthLeft = static_cast<float>(Json.GetNumberField(TEXT("boss_health_left")));
	Json.TryGetStringField(TEXT("when"), OutFight.When);
	return true;
}

FString Matsuri::BossMemory::Describe(const FMatsuriBossObservation& O)
{
	return FString::Printf(TEXT("phase %d, her health %.0f%%, player (%s) %s%s%s%s, %.0f m away, ammo %d/%d, %s; last 20 s: shielded %.0f%%, up high %.0f%%, %.1f grapples/min, mostly at %s"),
		O.Phase, O.BossHealth * 100.0f, *O.Hero,
		O.ZoneName.IsEmpty() ? TEXT("in the open") : *FString::Printf(TEXT("at %s"), *O.ZoneName),
		O.bPlayerShielded ? TEXT(" (behind cover)") : TEXT(""), O.bPlayerHigh ? TEXT(" (up high)") : TEXT(""),
		O.bPlayerAirborne ? TEXT(" (airborne)") : TEXT(""), O.DistanceMeters, O.PlayerAmmo, O.PlayerMaxAmmo,
		*(O.bHealthKnown ? FString::Printf(TEXT("HP %.0f/%.0f"), O.PlayerHealth, O.PlayerMaxHealth)
			: FString::Printf(TEXT("legacy record: %d lives (HP unknown)"), O.PlayerLives)),
		O.CoverShare * 100.0f, O.HighShare * 100.0f, O.GrapplesPerMinute,
		O.FavouriteZoneName.IsEmpty() ? TEXT("the open") : *O.FavouriteZoneName);
}

FString Matsuri::BossMemory::Describe(const FMatsuriBossStep& Step, const TArray<FString>& ZoneNames)
{
	const FMatsuriBossAction& A = Step.Action;
	const FString Target = A.Skill == EMatsuriBossSkill::FoxRain
		? (ZoneNames.IsValidIndex(A.Target) ? FString::Printf(TEXT(" zone=%s"), *ZoneNames[A.Target]) : FString(TEXT(" zone=on_player")))
		: A.Skill == EMatsuriBossSkill::Phantom ? FString::Printf(TEXT(" anchor=%d"), A.Target) : FString();
	return FString::Printf(TEXT("[%s] %s -> %s(%.2f,%.2f,%.2f,%.2f)%s -> touches %d, grazes %d, she lost %.0f%% => reward %+.2f"),
		*Step.FightId.Right(6), *Describe(Step.Observation), Matsuri::BossSkills::Id(A.Skill),
		A.Params[0], A.Params[1], A.Params[2], A.Params[3], *Target,
		Step.Outcome.Touches, Step.Outcome.Grazes, Step.Outcome.BossDamage * 100.0f, Step.Reward);
}

// ---------------------------------------------------------------- store

FMatsuriBossStore::FMatsuriBossStore(const FString& InFolder)
	: Folder(InFolder)
{
}

FString FMatsuriBossStore::DefaultFolder()
{
	const FString Override = CVarMemoryFolder.GetValueOnGameThread();
	return Override.IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("BossMemory") : Override;
}

FString FMatsuriBossStore::LearnerPath() const
{
	return Folder / LearnerFile;
}

void FMatsuriBossStore::Load()
{
	FScopeLock Guard(&Lock);
	Steps.Reset();
	Fights.Reset();
	Lessons.Reset();
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToStringArray(Lines, *(Folder / StepsFile)))
	{
		for (const FString& Line : Lines)
		{
			FMatsuriBossStep Step;
			const TSharedPtr<FJsonObject> Json = Parse(Line);
			if (Json && Matsuri::BossMemory::FromJson(*Json, Step))
			{
				Steps.Add(Step);
			}
		}
	}
	Lines.Reset();
	if (FFileHelper::LoadFileToStringArray(Lines, *(Folder / FightsFile)))
	{
		for (const FString& Line : Lines)
		{
			FMatsuriBossFight Fight;
			const TSharedPtr<FJsonObject> Json = Parse(Line);
			if (Json && Matsuri::BossMemory::FromJson(*Json, Fight))
			{
				Fights.Add(Fight);
			}
		}
	}
	FString Notebook;
	if (FFileHelper::LoadFileToString(Notebook, *(Folder / NotebookFile)))
	{
		const TSharedPtr<FJsonObject> Json = Parse(Notebook);
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (Json && Json->TryGetArrayField(TEXT("lessons"), Items))
		{
			for (const TSharedPtr<FJsonValue>& Item : *Items)
			{
				Lessons.Add(Item->AsString());
			}
		}
	}
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Boss memory: %d steps from %d fights, %d lessons (%s)"), Steps.Num(), Fights.Num(), Lessons.Num(), *Folder);
}

void FMatsuriBossStore::AppendStep(const FMatsuriBossStep& Step)
{
	FScopeLock Guard(&Lock);
	Steps.Add(Step);
	FFileHelper::SaveStringToFile(Condensed(Matsuri::BossMemory::ToJson(Step)) + LINE_TERMINATOR, *(Folder / StepsFile),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

void FMatsuriBossStore::AppendFight(const FMatsuriBossFight& Fight)
{
	FScopeLock Guard(&Lock);
	Fights.Add(Fight);
	FFileHelper::SaveStringToFile(Condensed(Matsuri::BossMemory::ToJson(Fight)) + LINE_TERMINATOR, *(Folder / FightsFile),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

void FMatsuriBossStore::SetLessons(const TArray<FString>& InLessons)
{
	FScopeLock Guard(&Lock);
	Lessons = InLessons;
	if (Lessons.Num() > Matsuri::BossMemory::MaxLessons)
	{
		Lessons.SetNum(Matsuri::BossMemory::MaxLessons);
	}
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FString& Lesson : Lessons)
	{
		Items.Add(MakeShared<FJsonValueString>(Lesson));
	}
	Json->SetArrayField(TEXT("lessons"), Items);
	FFileHelper::SaveStringToFile(Condensed(Json), *(Folder / NotebookFile), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void FMatsuriBossStore::Wipe()
{
	FScopeLock Guard(&Lock);
	IFileManager::Get().DeleteDirectory(*Folder, false, true);
	Steps.Reset();
	Fights.Reset();
	Lessons.Reset();
}
