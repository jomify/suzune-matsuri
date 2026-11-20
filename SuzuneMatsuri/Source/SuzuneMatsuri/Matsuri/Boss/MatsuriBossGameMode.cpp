// Suzune's Night Festival - game mode for 本殿・月下, the boss floor.

#include "MatsuriBossGameMode.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "MatsuriBoss.h"
#include "MatsuriBossBarWidget.h"
#include "MatsuriDanmakuField.h"
#include "MatsuriFloors.h"
#include "MatsuriGrappleComponent.h"
#include "SuzuneMatsuri.h"

namespace
{
	constexpr int32 BossBarZOrder = 1;	// above the HUD, under menus and the loading card
}

void AMatsuriBossGameMode::BuildFloorContent(UWorld& World, int32 FloorIndex, const FMatsuriFloorSpec& Spec, const FMatsuriFloorTheme& Theme)
{
	Arena = Matsuri::BossArena::Layout();
	Matsuri::BossArena::Build(World, Arena, Theme);
	FloorStart = World.SpawnActor<APlayerStart>(Arena.PlayerStart, Arena.PlayerStartRotation);
	Field = World.SpawnActor<AMatsuriDanmakuField>(Arena.Center, FRotator::ZeroRotator);
	Field->SetArena(Arena);
	Boss = World.SpawnActor<AMatsuriBoss>(Arena.Anchors[0], FRotator::ZeroRotator);
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Boss floor built: %d solids, %d zones, %d hook rings, %d ammo spots"),
		Arena.Solids.Num(), Arena.Zones.Num(), Arena.HookPoints.Num(), Arena.AmmoSpots.Num());
}

void AMatsuriBossGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);
	if (PlayerPawn && !PlayerPawn->FindComponentByClass<UMatsuriGrappleComponent>())
	{
		UMatsuriGrappleComponent* Grapple = NewObject<UMatsuriGrappleComponent>(PlayerPawn, TEXT("MatsuriGrapple"));
		Grapple->RegisterComponent();	// BeginPlay binds F once the pawn is possessed
	}
}

void AMatsuriBossGameMode::BeginPlay()
{
	Super::BeginPlay();	// builds the courtyard, starts the floor, adds the HUD
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		BossBar = CreateWidget<UMatsuriBossBarWidget>(PC, UMatsuriBossBarWidget::StaticClass());
		BossBar->AddToViewport(BossBarZOrder);
	}
	if (Boss)
	{
		Boss->OnDefeated.AddUObject(this, &AMatsuriBossGameMode::HandleBossDefeated);
	}
	const IConsoleVariable* Threats = IConsoleManager::Get().FindConsoleVariable(TEXT("matsuri.Threats"));
	if (!Threats || Threats->GetBool())
	{
		StartBossFight();
	}
}

void AMatsuriBossGameMode::StartBossFight()
{
	if (Boss && Field && !bBossStarted)
	{
		Boss->Setup(Field, Arena);
		bBossStarted = true;
	}
}

void AMatsuriBossGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (BossBar)
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const UMatsuriGrappleComponent* Grapple = PC && PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<UMatsuriGrappleComponent>() : nullptr;
		BossBar->Refresh(Boss, Grapple);
	}
}

int32 AMatsuriBossGameMode::DirectPlayFloor() const
{
	return Matsuri::Floors::BossFloor;
}

void AMatsuriBossGameMode::HandleBossDefeated()
{
	HandleFloorCleared();	// the last floor: the run ends in victory
}

void AMatsuriBossGameMode::OnFloorEnded(bool bPlayerWon)
{
	if (!bPlayerWon && Boss)
	{
		Boss->EndFightAsWinner();
	}
	if (Field)
	{
		Field->ClearAll();
	}
}
