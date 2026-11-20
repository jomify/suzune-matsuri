// Suzune's Night Festival - small shared combat helpers.

#include "MatsuriCombat.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "MatsuriFloorGameMode.h"
#include "SuzuneMatsuri.h"

bool Matsuri::Combat::IsPlayerPawn(const AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn && Pawn->IsPlayerControlled();
}

void Matsuri::Combat::HurtPlayer(const UObject* WorldContext, const AActor* Source)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (AMatsuriFloorGameMode* GameMode = World ? World->GetAuthGameMode<AMatsuriFloorGameMode>() : nullptr)
	{
		GameMode->HurtPlayer(Source);
		return;
	}
	UE_LOG(LogSuzuneMatsuri, Warning, TEXT("%s hit the player outside a floor game mode; ignored"), *GetNameSafe(Source));
}
