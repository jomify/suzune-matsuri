// Suzune's Night Festival - developer mode: jump to any floor, demo switches, in-game shortcuts.

#include "MatsuriDev.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "MatsuriBoss.h"
#include "MatsuriFloorGameMode.h"
#include "MatsuriRunSubsystem.h"
#include "SuzuneMatsuri.h"

namespace
{
	TAutoConsoleVariable<bool> CVarDev(
		TEXT("matsuri.Dev"), false,
		TEXT("Developer mode: F5 clear the floor, F6 ammo + ultimate, F7 boss's next health bar, F8 god mode (set by the title's developer panel)."));

	IConsoleVariable* Find(const TCHAR* Name)
	{
		IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name);
		UE_CLOG(!Var, LogSuzuneMatsuri, Warning, TEXT("Developer mode: no console variable %s"), Name);
		return Var;
	}
}

bool Matsuri::Dev::IsEnabled()
{
	return CVarDev.GetValueOnGameThread();
}

void Matsuri::Dev::SetEnabled(bool bEnabled)
{
	SetBool(TEXT("matsuri.Dev"), bEnabled);
}

bool Matsuri::Dev::GetBool(const TCHAR* Name)
{
	const IConsoleVariable* Var = Find(Name);
	return Var && Var->GetBool();
}

void Matsuri::Dev::SetBool(const TCHAR* Name, bool bValue)
{
	if (IConsoleVariable* Var = Find(Name))
	{
		Var->Set(bValue, ECVF_SetByCode);
	}
}

FString Matsuri::Dev::GetString(const TCHAR* Name)
{
	const IConsoleVariable* Var = Find(Name);
	return Var ? Var->GetString() : FString();
}

void Matsuri::Dev::SetString(const TCHAR* Name, const FString& Value)
{
	if (IConsoleVariable* Var = Find(Name))
	{
		Var->Set(*Value, ECVF_SetByCode);
	}
}

FString Matsuri::Dev::Cycle(const TCHAR* Name, const TArray<FString>& Values)
{
	if (Values.Num() == 0)
	{
		return FString();
	}
	const FString Now = GetString(Name);
	int32 Index = INDEX_NONE;
	for (int32 i = 0; i < Values.Num(); ++i)
	{
		// Numbers come back formatted ("150" may read "150.000000"): compare as numbers when both are.
		const bool bSame = Values[i].IsNumeric() && Now.IsNumeric()
			? FMath::IsNearlyEqual(FCString::Atof(*Values[i]), FCString::Atof(*Now))
			: Values[i].Equals(Now, ESearchCase::IgnoreCase);
		if (bSame)
		{
			Index = i;
			break;
		}
	}
	const FString Next = Values[(Index + 1) % Values.Num()];
	SetString(Name, Next);
	return Next;
}

void Matsuri::Dev::ClearFloor(UWorld* World)
{
	AMatsuriFloorGameMode* Mode = World ? World->GetAuthGameMode<AMatsuriFloorGameMode>() : nullptr;
	if (!Mode || !Mode->IsFloorActive())
	{
		return;
	}
	for (TActorIterator<AMatsuriBoss> Boss(World); Boss; ++Boss)
	{
		if (!Boss->IsDefeated())
		{
			Boss->DevDefeat();	// her defeat clears the boss floor
			return;
		}
	}
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Developer mode: floor cleared"));
	Mode->DevClearFloor();
}

void Matsuri::Dev::Restock(UWorld* World)
{
	if (UMatsuriRunSubsystem* Run = World ? UMatsuriRunSubsystem::Get(World) : nullptr)
	{
		Run->AddAmmo(Run->GetRunState().GetAmmoCap());
		Run->AddUltCharge(1.0f);
	}
}

bool Matsuri::Dev::NextBossPhase(UWorld* World)
{
	for (TActorIterator<AMatsuriBoss> Boss(World); Boss; ++Boss)
	{
		if (!Boss->IsDefeated())
		{
			Boss->DevNextBar();
			return true;
		}
	}
	return false;
}

bool Matsuri::Dev::ToggleGod()
{
	const bool bGod = !GetBool(TEXT("matsuri.God"));
	SetBool(TEXT("matsuri.God"), bGod);
	return bGod;
}
