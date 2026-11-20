// Suzune's Night Festival - game mode for a playable floor.

#include "MatsuriFloorGameMode.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"
#include "MatsuriArt.h"
#include "MatsuriAudio.h"
#include "MatsuriFx.h"
#include "MatsuriDev.h"
#include "MatsuriDevComponent.h"
#include "MatsuriQuality.h"
#include "MatsuriCombatComponent.h"
#include "MatsuriViewModel.h"
#include "MatsuriSkillComponent.h"
#include "MatsuriFloorBuilder.h"
#include "MatsuriFloorDecor.h"
#include "MatsuriMusic.h"
#include "MatsuriGameOverWidget.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriLoadingWidget.h"
#include "MatsuriRunSubsystem.h"
#include "MatsuriReplay.h"
#include "MatsuriStory.h"
#include "MatsuriCinematicWidget.h"
#include "MatsuriVictoryWidget.h"
#include "SuzuneMatsuri.h"

namespace
{
	TAutoConsoleVariable<bool> CVarThreats(
		TEXT("matsuri.Threats"), true,
		TEXT("Spawn yokai and hazards on floors. Tests of the pure collection loop turn this off."));

	TAutoConsoleVariable<bool> CVarGod(
		TEXT("matsuri.God"), false,
		TEXT("Player ignores hits. For automated playthroughs and demos only."));

	constexpr int32 HUDZOrder = 0;
	constexpr int32 ModalZOrder = 10;
	constexpr int32 FxPiecesToPrewarm = 64;	// a swing is 9, a hit 4-8, a kill ~20: covers a busy moment
}

AMatsuriFloorGameMode::AMatsuriFloorGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;

	static ConstructorHelpers::FClassFinder<APawn> PawnFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	static ConstructorHelpers::FClassFinder<APlayerController> ControllerFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController"));
	if (PawnFinder.Succeeded())
	{
		DefaultPawnClass = PawnFinder.Class;
	}
	if (ControllerFinder.Succeeded())
	{
		PlayerControllerClass = ControllerFinder.Class;
	}

	HUDWidgetClass = UMatsuriHUDWidget::StaticClass();
	VictoryWidgetClass = UMatsuriVictoryWidget::StaticClass();
	GameOverWidgetClass = UMatsuriGameOverWidget::StaticClass();
}

AActor* AMatsuriFloorGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	EnsureFloorBuilt();
	return FloorStart ? FloorStart.Get() : Super::ChoosePlayerStart_Implementation(Player);
}

void AMatsuriFloorGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	if (PlayerPawn && Matsuri::Dev::IsEnabled() && !PlayerPawn->FindComponentByClass<UMatsuriDevComponent>())
	{
		NewObject<UMatsuriDevComponent>(PlayerPawn, TEXT("MatsuriDev"))->RegisterComponent();
	}
	Super::SetPlayerDefaults(PlayerPawn);
	if (!PlayerPawn->FindComponentByClass<UMatsuriCombatComponent>())
	{
		NewObject<UMatsuriCombatComponent>(PlayerPawn, TEXT("MatsuriCombat"))->RegisterComponent();
	}
	if (!PlayerPawn->FindComponentByClass<UMatsuriSkillComponent>())
	{
		NewObject<UMatsuriSkillComponent>(PlayerPawn, TEXT("MatsuriSkills"))->RegisterComponent();
	}

	// Heroine passives and omamori: speed (Suzune, 交通安全守) and double jump (Suzune).
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	ACharacter* Hero = Cast<ACharacter>(PlayerPawn);
	if (Hero && Run)
	{
		const FMatsuriRunState& State = Run->GetRunState();
		UCharacterMovementComponent* Movement = Hero->GetCharacterMovement();
		BaseWalkSpeed = BaseWalkSpeed > 0.0f ? BaseWalkSpeed : Movement->MaxWalkSpeed;
		Movement->MaxWalkSpeed = BaseWalkSpeed * State.Modifiers.MoveSpeedMultiplier * State.GetHero().MoveSpeedMultiplier;
		Hero->JumpMaxCount = State.GetHero().JumpCount;
		AttachViewWeapon(*Hero, State.Hero);
	}
}

void AMatsuriFloorGameMode::BeginPlay()
{
	Super::BeginPlay();
	Matsuri::Quality::ApplyForPlay();
	EnsureFloorBuilt();

	UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	Run->OnFloorCleared.AddDynamic(this, &AMatsuriFloorGameMode::HandleFloorCleared);
	Run->BeginFloor();
	RemainingSeconds = Run->GetCurrentFloorSpec().TimeLimitSeconds;
	bFloorActive = true;
	ReplayDirector = GetWorld()->SpawnActor<AMatsuriReplayDirector>();

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		// Menus switch the game viewport to UI-only, and the viewport outlives level loads.
		// Without this the floor after any menu button ignores every key and click.
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
		CreateWidget<UMatsuriHUDWidget>(PC, HUDWidgetClass)->AddToViewport(HUDZOrder);
		// The floor opens on its title card and fades into play (it never holds input or the clock).
		UMatsuriLoadingWidget::ShowArriving(*PC, Matsuri::Floors::ThemeFor(Run->GetRunState().FloorIndex).Title, true);
		const int32 FloorIndex = Run->GetRunState().FloorIndex;
		const EMatsuriMusic FloorMusic = Matsuri::Music::ForFloor(FloorIndex);
		if (Matsuri::Story::IsEnabled() && FloorIndex >= 0 && FloorIndex < 3)
		{
			// The story's theme under the (silent) cutscene, then the floor's own track.
			Matsuri::Music::Play(this, EMatsuriMusic::Story);
			const FSimpleDelegate ToFloorMusic = FSimpleDelegate::CreateWeakLambda(this, [this, FloorMusic]() { Matsuri::Music::Play(this, FloorMusic); });
			if (!UMatsuriCinematicWidget::TryPlay(*PC, static_cast<EMatsuriCinematic>(FloorIndex), ToFloorMusic))
			{
				Matsuri::Music::Play(this, FloorMusic);
			}
		}
		else
		{
			Matsuri::Music::Play(this, FloorMusic);
		}
	}
}

void AMatsuriFloorGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The subsystem outlives this world, so drop the binding explicitly.
	if (UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this))
	{
		Run->OnFloorCleared.RemoveDynamic(this, &AMatsuriFloorGameMode::HandleFloorCleared);
	}
	Super::EndPlay(EndPlayReason);
}

void AMatsuriFloorGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bFloorActive)
	{
		return;
	}
	RemainingSeconds = FMath::Max(0.0f, RemainingSeconds - DeltaSeconds);
	if (RemainingSeconds <= 0.0f)
	{
		EndFloorWithGameOver(EMatsuriRunEnd::TimeUp);
	}
}

void AMatsuriFloorGameMode::EnsureFloorBuilt()
{
	if (bFloorBuilt)
	{
		return;
	}
	bFloorBuilt = true;
	Matsuri::Art::Preload();	// pay for loading the art now, not on the first shot
	Matsuri::Audio::Preload();
	Matsuri::Fx::PreloadTemplates();
	Matsuri::Fx::Prewarm(GetWorld(), FxPiecesToPrewarm);

	UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	if (!Run->IsRunActive())
	{
		Run->StartNewRun(UMatsuriRunSubsystem::DefaultHero(), UMatsuriRunSubsystem::DefaultSeed());	// launched straight into a floor (PIE), not via the menu
		Run->StartOnFloor(DirectPlayFloor());
	}

	const int32 FloorIndex = Run->GetRunState().FloorIndex;
	BuildFloorContent(*GetWorld(), FloorIndex, Run->GetCurrentFloorSpec(), Matsuri::Floors::ThemeFor(FloorIndex));
}

void AMatsuriFloorGameMode::BuildFloorContent(UWorld& World, int32 FloorIndex, const FMatsuriFloorSpec& Spec, const FMatsuriFloorTheme& Theme)
{
	const FMatsuriFloorLayout Layout = Matsuri::Layout::BuildFloorLayout(Spec);
	Maze = Layout.Maze;
	Matsuri::Builder::SpawnEnvironment(World, Theme);
	Matsuri::Builder::SpawnBlocks(World, Layout, Theme);
	Matsuri::Decor::DressFloor(World, Layout, Spec, Theme, FloorIndex);
	Matsuri::Builder::SpawnCollectibles(World, Layout);
	Matsuri::Builder::SpawnSpawns(World, Layout, Spec.HalfExtent, CVarThreats.GetValueOnGameThread());

	FloorStart = World.SpawnActor<APlayerStart>(Layout.PlayerStart, Layout.PlayerStartRotation);
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Floor %d built: seed %d, %d blocks, %d collectibles worth %d (target %d), %d spawns, %.0f s"),
		FloorIndex + 1, Spec.Seed, Layout.Blocks.Num(), Layout.Coins.Num(), Layout.TotalPoints(), Spec.TargetScore,
		Layout.Spawns.Num(), Spec.TimeLimitSeconds);
}

bool AMatsuriFloorGameMode::IsPlayerInvulnerable() const
{
	return GetWorld()->GetTimeSeconds() < InvulnerableUntil;
}

FVector AMatsuriFloorGameMode::GetCheckpoint() const
{
	return FloorStart ? FloorStart->GetActorLocation() : FVector::ZeroVector;
}

void AMatsuriFloorGameMode::GrantInvulnerability(float Seconds)
{
	InvulnerableUntil = FMath::Max(InvulnerableUntil, GetWorld()->GetTimeSeconds() + Seconds);
}

void AMatsuriFloorGameMode::SetDoubleDrops(float Seconds)
{
	DoubleDropsUntil = GetWorld()->GetTimeSeconds() + Seconds;
}

bool AMatsuriFloorGameMode::IsDoubleDropActive() const
{
	return GetWorld()->GetTimeSeconds() < DoubleDropsUntil;
}

void AMatsuriFloorGameMode::SetRemainingSeconds(float Seconds)
{
	RemainingSeconds = FMath::Max(0.0f, Seconds);
}

bool AMatsuriFloorGameMode::HurtPlayer(const AActor* Source, float Damage, const FText& Cause, int32 Skill, const FSimpleDelegate& OnDamageAccepted)
{
	if (!bFloorActive || IsPlayerInvulnerable() || CVarGod.GetValueOnGameThread() || !FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
		return false;
	}
	UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	if (!Run)
	{
		return false;
	}
	const EMatsuriHurtResult Result = Run->HurtPlayer(Damage);
	if (Result == EMatsuriHurtResult::Ignored)
	{
		return false;
	}
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Player hit by %s: %.0f damage, %.0f / %.0f HP"),
		*GetNameSafe(Source), Damage, Run->GetRunState().Health, Run->GetRunState().GetMaxHealth());
	Matsuri::Audio::Play2D(this, EMatsuriSfx::PlayerHurt);
	if (Result == EMatsuriHurtResult::Blessed)
	{
		GrantInvulnerability(InvulnerableAfterBlessing);
		return false;
	}
	// Accepted-hit accounting must be visible to a synchronous fatal settlement.
	OnDamageAccepted.ExecuteIfBound();
	if (Result == EMatsuriHurtResult::Hurt)
	{
		GrantInvulnerability(InvulnerableAfterHit);
		return true;
	}

	// Capture and pause before ending the fight clears its projectiles. Replay owns
	// a visual history; the normal result is committed once, when playback finishes.
	bFloorActive = false;
	const FSimpleDelegate Finished = FSimpleDelegate::CreateWeakLambda(this, [this]()
	{
		EndFloorWithGameOver(EMatsuriRunEnd::OutOfHealth);
	});
	const FText DeathCause = Cause.IsEmpty()
		? NSLOCTEXT("MatsuriDeath", "Attack", "妖怪或陷阱的攻击") : Cause;
	if (!ReplayDirector || !ReplayDirector->Start(Source, DeathCause, Skill, Finished))
	{
		EndFloorWithGameOver(EMatsuriRunEnd::OutOfHealth);
	}
	return true;
}

void AMatsuriFloorGameMode::EndFloorWithGameOver(EMatsuriRunEnd Reason)
{
	if (bGameOverFinalized || GetWorld()->bIsTearingDown)
	{
		return;
	}
	bGameOverFinalized = true;
	bFloorActive = false;
	OnFloorEnded(false);
	Matsuri::Music::PlaySting(this, EMatsuriSting::Defeat);
	UMatsuriRunSubsystem::Get(this)->EndRun(Reason);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		const FSimpleDelegate Continue = FSimpleDelegate::CreateWeakLambda(this, [this]() { ShowGameOverScreen(); });
		if (Matsuri::Story::IsEnabled() && UMatsuriCinematicWidget::TryPlay(*PC, EMatsuriCinematic::Defeat, Continue))
		{
			return;
		}
		Continue.ExecuteIfBound();
	}
}

void AMatsuriFloorGameMode::ShowGameOverScreen()
{
	if (GetWorld() && !GetWorld()->bIsTearingDown)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			ShowModal(CreateWidget<UMatsuriGameOverWidget>(PC, GameOverWidgetClass));
		}
	}
}

void AMatsuriFloorGameMode::HandleFloorCleared()
{
	if (!bFloorActive)
	{
		return;
	}
	bFloorActive = false;
	OnFloorEnded(true);
	Matsuri::Audio::Play2D(this, EMatsuriSfx::FloorClear);

	UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	const bool bLastFloor = Matsuri::Floors::IsLastFloor(Run->GetRunState().FloorIndex);
	// The last floor's clear is the ending (its theme under the cutscene and the results); others get the sting.
	if (bLastFloor)
	{
		Matsuri::Music::Play(this, EMatsuriMusic::Ending);
	}
	else
	{
		Matsuri::Music::PlaySting(this, EMatsuriSting::Clear);
	}
	if (bLastFloor)
	{
		Run->EndRun(EMatsuriRunEnd::Victory);
		if (Matsuri::Story::IsEnabled())
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				const FSimpleDelegate Continue = FSimpleDelegate::CreateWeakLambda(this, [this]() { ShowVictoryScreen(); });
				if (!UMatsuriCinematicWidget::TryPlay(*PC, EMatsuriCinematic::Victory, Continue))
				{
					Continue.ExecuteIfBound();
				}
				return;
			}
		}
	}
	ShowVictoryScreen();
}

void AMatsuriFloorGameMode::ShowVictoryScreen()
{
	if (!GetWorld()->bIsTearingDown)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			ShowModal(CreateWidget<UMatsuriVictoryWidget>(PC, VictoryWidgetClass));
		}
	}
}

void AMatsuriFloorGameMode::ShowModal(UUserWidget* Widget)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !Widget)
	{
		UE_LOG(LogSuzuneMatsuri, Error, TEXT("Cannot show end-of-floor screen: missing player or widget"));
		return;
	}
	Widget->AddToViewport(ModalZOrder);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	PC->SetPause(true);
}

void AMatsuriFloorGameMode::AttachViewWeapon(ACharacter& Hero, EMatsuriHero Heroine)
{
	UMatsuriViewModelComponent::Equip(Hero, Heroine);	// no-op without the art
}
