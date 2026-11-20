// Suzune's Night Festival - run state that survives level loads, plus the saved record.

#include "MatsuriRunSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MatsuriFloors.h"
#include "MatsuriSaveGame.h"
#include "SuzuneMatsuri.h"

namespace
{
	constexpr int32 SaveUserIndex = 0;

	TAutoConsoleVariable<int32> CVarHero(
		TEXT("matsuri.Hero"), 0,
		TEXT("Heroine used when a floor map is played directly: 0 Sakura, 1 Suzune, 2 Mio."));

	TAutoConsoleVariable<int32> CVarSeed(
		TEXT("matsuri.Seed"), 0,
		TEXT("Run seed when a floor map is played directly (same layout every time, for comparing). 0 = random."));

	TAutoConsoleVariable<bool> CVarMaze(
		TEXT("matsuri.Maze"), true,
		TEXT("Build floors as mazes. 0 = open test arena (combat / skill tests use it so results do not depend on a random maze)."));

	int32 RandomSeed()
	{
		const int32 Seed = static_cast<int32>(GetTypeHash(FDateTime::Now().GetTicks()));
		return Seed != 0 ? Seed : 1;
	}
}

void UMatsuriRunSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadRecord();
}

EMatsuriHero UMatsuriRunSubsystem::DefaultHero()
{
	return static_cast<EMatsuriHero>(FMath::Clamp(CVarHero.GetValueOnGameThread(), 0, Matsuri::Heroes::Count - 1));
}

int32 UMatsuriRunSubsystem::DefaultSeed()
{
	return CVarSeed.GetValueOnGameThread();
}

void UMatsuriRunSubsystem::StartNewRun(EMatsuriHero Hero, int32 Seed)
{
	Run.Reset(Seed != 0 ? Seed : RandomSeed(), Hero);
	bRunActive = true;
	LastRunEnd = EMatsuriRunEnd::None;
	bLastRunWasNewBest = false;
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Run started as %s, seed %d"), *Run.GetHero().Name.ToString(), Run.Seed);
	Broadcast();
}

void UMatsuriRunSubsystem::BeginFloor()
{
	Run.BeginFloor(GetCurrentFloorSpec().TargetScore);
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Floor %d started, target %d"), Run.FloorIndex + 1, Run.Score.FloorTarget);
	Broadcast();
}

FMatsuriFloorSpec UMatsuriRunSubsystem::GetCurrentFloorSpec() const
{
	FMatsuriFloorSpec Spec = Matsuri::Floors::SpecFor(Run.FloorIndex, Run.Seed);
	Spec.TimeLimitSeconds += Run.Modifiers.ExtraSeconds;
	Spec.bMaze = Spec.bMaze && CVarMaze.GetValueOnGameThread();
	return Spec;
}

void UMatsuriRunSubsystem::Collect(EMatsuriCollectible Kind)
{
	const bool bJustCleared = Run.Collect(Kind);
	Broadcast();
	if (bJustCleared)
	{
		UE_LOG(LogSuzuneMatsuri, Log, TEXT("Floor cleared with %d / %d"), Run.Score.FloorScore, Run.Score.FloorTarget);
		OnFloorCleared.Broadcast();
	}
}

EMatsuriHurtResult UMatsuriRunSubsystem::HurtPlayer(float Damage)
{
	const EMatsuriHurtResult Result = Run.Hurt(Damage);
	if (Result == EMatsuriHurtResult::Ignored)
	{
		return Result;
	}
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Player hurt: %s, health %.0f / %.0f, score %d"), *UEnum::GetValueAsString(Result), Run.Health, Run.GetMaxHealth(), Run.Score.RunScore);
	Broadcast();
	OnPlayerHurt.Broadcast(Result);
	return Result;
}

float UMatsuriRunSubsystem::HealPlayer(float Amount)
{
	const float Healed = Run.Heal(Amount);
	if (Healed > 0.0f)
	{
		Broadcast();
	}
	return Healed;
}

int32 UMatsuriRunSubsystem::AddAmmo(int32 Amount)
{
	const int32 Added = Run.AddAmmo(Amount);
	if (Added > 0)
	{
		Broadcast();
	}
	return Added;
}

bool UMatsuriRunSubsystem::ConsumeAmmo(int32 Amount)
{
	const bool bConsumed = Run.ConsumeAmmo(Amount);
	if (bConsumed)
	{
		Broadcast();
	}
	return bConsumed;
}

void UMatsuriRunSubsystem::AddDamageDealt(float Damage)
{
	Run.AddDamageDealt(Damage);
	Broadcast();
}

void UMatsuriRunSubsystem::AddUltCharge(float Amount)
{
	Run.UltCharge = FMath::Clamp(Run.UltCharge + Amount, 0.0f, 1.0f);
	Broadcast();
}

bool UMatsuriRunSubsystem::ConsumeUlt()
{
	const bool bUsed = Run.ConsumeUlt();
	Broadcast();
	return bUsed;
}

bool UMatsuriRunSubsystem::SpendScore(int32 Amount)
{
	const bool bSpent = Run.SpendScore(Amount);
	Broadcast();
	return bSpent;
}

void UMatsuriRunSubsystem::StartOnFloor(int32 FloorIndex)
{
	Run.FloorIndex = FMath::Clamp(FloorIndex, 0, Matsuri::Floors::Total - 1);
	Broadcast();
}

void UMatsuriRunSubsystem::SetFloorTargetForTesting(int32 Target)
{
	Run.Score.FloorTarget = FMath::Max(1, Target);
	Broadcast();
}

TArray<EMatsuriOmamori> UMatsuriRunSubsystem::GetOmamoriOffer() const
{
	return Matsuri::Omamori::RollOffer(Run.Seed, Run.FloorIndex);
}

void UMatsuriRunSubsystem::ChooseOmamoriAndAdvance(EMatsuriOmamori Charm)
{
	Run.TakeCharm(Charm);
	Run.AdvanceFloor();
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Took %s, advancing to floor %d"), *UEnum::GetValueAsString(Charm), Run.FloorIndex + 1);
	Broadcast();
}

void UMatsuriRunSubsystem::EndRun(EMatsuriRunEnd Reason)
{
	if (!bRunActive)
	{
		return;
	}
	bRunActive = false;
	LastRunEnd = Reason;

	const bool bVictory = Reason == EMatsuriRunEnd::Victory;
	bLastRunWasNewBest = Matsuri::Record::IsNewBest(Record, Run.Score.RunScore);
	Record = Matsuri::Record::WithRun(Record, Run.Score.RunScore, Run.FloorIndex + 1, bVictory);
	SaveRecord();

	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Run ended (%s): score %d, best %d"), *UEnum::GetValueAsString(Reason), Run.Score.RunScore, Record.BestScore);
	OnRunEnded.Broadcast(Reason);
}

void UMatsuriRunSubsystem::UseRecordSlot(const FString& SlotName)
{
	RecordSlot = SlotName;
	LoadRecord();
}

void UMatsuriRunSubsystem::Broadcast()
{
	OnRunChanged.Broadcast(Run);
}

void UMatsuriRunSubsystem::LoadRecord()
{
	Record = FMatsuriRecord();
	if (!UGameplayStatics::DoesSaveGameExist(RecordSlot, SaveUserIndex))
	{
		UE_LOG(LogSuzuneMatsuri, Log, TEXT("No saved record in slot %s yet"), *RecordSlot);
		return;
	}
	if (const UMatsuriSaveGame* Save = Cast<UMatsuriSaveGame>(UGameplayStatics::LoadGameFromSlot(RecordSlot, SaveUserIndex)))
	{
		Record = Save->Record;
		UE_LOG(LogSuzuneMatsuri, Log, TEXT("Loaded record: best %d, deepest floor %d"), Record.BestScore, Record.DeepestFloor);
		return;
	}
	UE_LOG(LogSuzuneMatsuri, Warning, TEXT("Save slot %s exists but could not be read; starting a new record"), *RecordSlot);
}

void UMatsuriRunSubsystem::SaveRecord() const
{
	UMatsuriSaveGame* Save = Cast<UMatsuriSaveGame>(UGameplayStatics::CreateSaveGameObject(UMatsuriSaveGame::StaticClass()));
	Save->Record = Record;
	if (!UGameplayStatics::SaveGameToSlot(Save, RecordSlot, SaveUserIndex))
	{
		UE_LOG(LogSuzuneMatsuri, Error, TEXT("Could not save record to slot %s"), *RecordSlot);
		return;
	}

	// Human-readable copy for the logbook / demo; the .sav stays the source of truth.
	FString Json;
	const FString JsonPath = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (RecordSlot + TEXT(".json"));
	if (!FJsonObjectConverter::UStructToJsonObjectString(Record, Json) || !FFileHelper::SaveStringToFile(Json, *JsonPath))
	{
		UE_LOG(LogSuzuneMatsuri, Warning, TEXT("Could not write %s"), *JsonPath);
	}
}

UMatsuriRunSubsystem* UMatsuriRunSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UMatsuriRunSubsystem>() : nullptr;
}
