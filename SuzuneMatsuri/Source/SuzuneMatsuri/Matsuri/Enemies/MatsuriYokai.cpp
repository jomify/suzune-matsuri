// Suzune's Night Festival - shared yokai behaviour.

#include "MatsuriYokai.h"
#include "MatsuriYokaiBrain.h"
#include "MatsuriAudio.h"
#include "MatsuriCombatComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MatsuriAmmoPickup.h"
#include "MatsuriCollectible.h"
#include "MatsuriCombat.h"
#include "MatsuriDecoy.h"
#include "MatsuriFx.h"
#include "MatsuriFloorGameMode.h"
#include "MatsuriRunSubsystem.h"
#include "EngineUtils.h"
#include "SuzuneMatsuri.h"

namespace
{
	constexpr float ArenaMargin = 60.0f;
	constexpr float DropScatter = 70.0f;
	constexpr float DropHeight = 90.0f;
	constexpr float EyeHeight = 40.0f;
	constexpr float DecoyLureRange = 1500.0f;
	constexpr float WanderRadius = 500.0f;
}

namespace
{
	constexpr float HitPunchSeconds = 0.12f;
	constexpr float HitPunchScale = 0.25f;	// +25 % at the moment of the hit, easing back
}

AMatsuriYokai::AMatsuriYokai()
{
	PrimaryActorTick.bCanEverTick = true;
	Tags.Add(Matsuri::Combat::YokaiTag);

	Body = CreateDefaultSubobject<USphereComponent>(TEXT("Body"));
	Body->InitSphereRadius(50.0f);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_WorldDynamic);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);	// swept movement stops at walls
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);			// contact hurts, never body-blocks
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);		// crosshair aim converges on it
	Body->SetCollisionResponseToChannel(Matsuri::Combat::ProjectileChannel, ECR_Block);
	Body->SetGenerateOverlapEvents(true);
	Body->SetCanEverAffectNavigation(false);
	RootComponent = Body;

	Visual = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Body);
}

void AMatsuriYokai::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	Body->OnComponentBeginOverlap.AddDynamic(this, &AMatsuriYokai::HandleContact);
}

void AMatsuriYokai::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HitPunchLeft > 0.0f)
	{
		HitPunchLeft = FMath::Max(0.0f, HitPunchLeft - DeltaSeconds);
		Visual->SetRelativeScale3D(FVector(1.0f + HitPunchScale * HitPunchLeft / HitPunchSeconds));
	}
}

float AMatsuriYokai::TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Applied = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	if (bDead || Applied <= 0.0f)
	{
		return 0.0f;
	}

	Health -= Applied;
	HitPunchLeft = HitPunchSeconds;
	if (EventInstigator && EventInstigator->IsPlayerController())
	{
		if (UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this))
		{
			Run->AddDamageDealt(Applied);	// fighting charges the ultimate
		}
	}
	if (Health <= 0.0f)
	{
		bDead = true;
		const APawn* Killer = EventInstigator ? EventInstigator->GetPawn() : nullptr;
		if (UMatsuriCombatComponent* Weapon = Killer ? Killer->FindComponentByClass<UMatsuriCombatComponent>() : nullptr)
		{
			Weapon->ConfirmKill();
		}
		UE_LOG(LogSuzuneMatsuri, Verbose, TEXT("%s defeated"), *GetName());
		Matsuri::Fx::Burst(GetWorld(), GetActorLocation(), GetFxColor(), 16, 550.0f, 14.0f, 0.55f);
		Matsuri::Fx::Smoke(GetWorld(), GetActorLocation(), 110.0f);
		Matsuri::Audio::PlayAt(this, EMatsuriSfx::YokaiDeath, GetActorLocation());
		Matsuri::Fx::Template(GetWorld(), Matsuri::Fx::ETemplate::Explosion, GetActorLocation(), FVector::UpVector, 0.5f);
		const AMatsuriFloorGameMode* GameMode = GetWorld()->GetAuthGameMode<AMatsuriFloorGameMode>();
		const int32 DropRounds = GameMode && GameMode->IsDoubleDropActive() ? 2 : 1;
		for (int32 i = 0; i < DropRounds; ++i)
		{
			SpawnDrops();
		}
		Destroy();
	}
	return Applied;
}

void AMatsuriYokai::HandleContact(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bDead && Matsuri::Combat::IsPlayerPawn(OtherActor))
	{
		Matsuri::Combat::HurtPlayer(this, this);
	}
}

void AMatsuriYokai::Stun(float Seconds)
{
	const double Until = GetWorld()->GetTimeSeconds() + Seconds;
	if (Until > StunnedUntil + 0.1)
	{
		Matsuri::Fx::StunHalo(GetWorld(), this, Seconds);
	}
	StunnedUntil = FMath::Max(StunnedUntil, Until);
}

bool AMatsuriYokai::IsStunned() const
{
	return GetWorld()->GetTimeSeconds() < StunnedUntil;
}

void AMatsuriYokai::Knockback(const FVector& Offset)
{
	MoveSwept(FVector(Offset.X, Offset.Y, 0.0f));
}

AActor* AMatsuriYokai::FindTarget() const
{
	for (TActorIterator<AMatsuriDecoy> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < DecoyLureRange)
		{
			return *It;
		}
	}
	return FindPlayer();
}

APawn* AMatsuriYokai::FindPlayer() const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	return PC ? PC->GetPawn() : nullptr;
}

bool AMatsuriYokai::CanSee(const AActor* Target) const
{
	if (!Target)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MatsuriYokaiSight), false, this);
	const FVector Eye = GetActorLocation() + FVector(0.0f, 0.0f, EyeHeight);
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target->GetActorLocation(), ECC_Visibility, Params);
	return !bBlocked || Hit.GetActor() == Target;
}

bool AMatsuriYokai::IsWatchedBy(const AActor* Target) const
{
	const APawn* Pawn = Cast<APawn>(Target);
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller || !Controller->IsPlayerController())
	{
		return false;	// a decoy has no eyes
	}
	FVector Eye;
	FRotator View;
	Controller->GetPlayerViewPoint(Eye, View);
	return Matsuri::YokaiBrain::IsInViewCone(Eye, View.Vector(), GetActorLocation()) && CanSee(Target);
}

bool AMatsuriYokai::IsVisibleFrom(const AActor* Target, const FVector& Spot) const
{
	if (!Target)
	{
		return false;
	}
	FVector Eye;
	FRotator Unused;
	Target->GetActorEyesViewPoint(Eye, Unused);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MatsuriYokaiCover), false, this);
	Params.AddIgnoredActor(Target);
	return !GetWorld()->LineTraceTestByChannel(Eye, Spot, ECC_Visibility, Params);
}

FVector AMatsuriYokai::NextStepToward(const FVector& Target, int32* OutSteps) const
{
	const AMatsuriFloorGameMode* GameMode = GetWorld()->GetAuthGameMode<AMatsuriFloorGameMode>();
	const FMatsuriMaze* Maze = GameMode ? GameMode->GetMaze() : nullptr;
	if (!Maze)
	{
		if (OutSteps)
		{
			*OutSteps = INDEX_NONE;
		}
		return Target;
	}
	const FVector2D Waypoint = Maze->NextWaypoint(FVector2D(GetActorLocation()), FVector2D(Target), OutSteps);
	return FVector(Waypoint, Target.Z);
}

bool AMatsuriYokai::ShouldHunt(const AActor* Target, int32 HuntCells, float ArenaRange) const
{
	if (!Target)
	{
		return false;
	}
	int32 Steps = INDEX_NONE;
	NextStepToward(Target->GetActorLocation(), &Steps);
	return Steps == INDEX_NONE
		? FVector::Dist2D(Target->GetActorLocation(), GetActorLocation()) < ArenaRange
		: Steps <= HuntCells;
}

FVector AMatsuriYokai::PickWanderGoal() const
{
	const AMatsuriFloorGameMode* GameMode = GetWorld()->GetAuthGameMode<AMatsuriFloorGameMode>();
	const FMatsuriMaze* Maze = GameMode ? GameMode->GetMaze() : nullptr;
	if (!Maze)
	{
		return ClampToArena(GetActorLocation() + FVector(FMath::RandPointInCircle(WanderRadius), 0.0f));
	}
	const TArray<FIntPoint> Exits = Maze->OpenNeighbours(Maze->CellAt(FVector2D(GetActorLocation())));
	const FIntPoint Next = Exits.Num() > 0 ? Exits[FMath::RandRange(0, Exits.Num() - 1)] : Maze->CellAt(FVector2D(GetActorLocation()));
	return FVector(Maze->CellCenter(Next), GetActorLocation().Z);
}

void AMatsuriYokai::MoveSwept(const FVector& Delta)
{
	FHitResult Hit;
	SetActorLocation(ClampToArena(GetActorLocation() + Delta), true, &Hit);
	if (Hit.IsValidBlockingHit())
	{
		// Slide along the obstacle (or round it, when walking straight into it) with what is left.
		const FVector FromCentre = GetActorLocation() - (Hit.GetComponent() ? Hit.GetComponent()->Bounds.Origin : Hit.ImpactPoint);
		const FVector Remaining = Matsuri::Yokai::AroundObstacle(Delta * (1.0f - Hit.Time), Hit.Normal, FromCentre);
		SetActorLocation(ClampToArena(GetActorLocation() + Remaining), true);
	}
}

FVector Matsuri::Yokai::AroundObstacle(const FVector& Step, const FVector& HitNormal, const FVector& FromObstacleCentre)
{
	constexpr float HeadOnFraction = 0.35f;	// sliding keeps less than this share of the step: treat as head-on
	const FVector Flat(Step.X, Step.Y, 0.0f);
	const FVector Normal = FVector(HitNormal.X, HitNormal.Y, 0.0f).GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		return Flat;
	}
	const FVector Slide = FVector::VectorPlaneProject(Flat, Normal);
	if (Slide.Size() >= Flat.Size() * HeadOnFraction)
	{
		return Slide;
	}
	FVector Along = FVector::CrossProduct(Normal, FVector::UpVector);	// a tangent of the face
	if (FVector::DotProduct(Along, FromObstacleCentre) < 0.0f)
	{
		Along = -Along;	// toward the side the yokai already leans to: the nearer edge
	}
	return Along * Flat.Size();
}

void AMatsuriYokai::FaceTowards(const FVector& Target)
{
	const FVector ToTarget = (Target - GetActorLocation()).GetSafeNormal2D();
	if (!ToTarget.IsNearlyZero())
	{
		SetActorRotation(ToTarget.Rotation());
	}
}

FVector AMatsuriYokai::ClampToArena(const FVector& Location) const
{
	const float Limit = ArenaHalfExtent - ArenaMargin;
	return FVector(FMath::Clamp(Location.X, -Limit, Limit), FMath::Clamp(Location.Y, -Limit, Limit), Location.Z);
}

void AMatsuriYokai::DropCollectible(EMatsuriCollectible Kind)
{
	const FVector Spot = ClampToArena(GetActorLocation() + FVector(FMath::RandPointInCircle(DropScatter), 0.0f));
	const FTransform Transform(FVector(Spot.X, Spot.Y, DropHeight));
	AMatsuriCollectible* Drop = GetWorld()->SpawnActorDeferred<AMatsuriCollectible>(AMatsuriCollectible::StaticClass(), Transform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Drop->SetKind(Kind);
	Drop->FinishSpawning(Transform);
}

void AMatsuriYokai::DropAmmo(int32 Amount)
{
	const FVector Spot = ClampToArena(GetActorLocation() + FVector(FMath::RandPointInCircle(DropScatter), 0.0f));
	const FTransform Transform(FVector(Spot.X, Spot.Y, DropHeight));
	AMatsuriAmmoPickup* Drop = GetWorld()->SpawnActorDeferred<AMatsuriAmmoPickup>(AMatsuriAmmoPickup::StaticClass(), Transform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Drop->Configure(Amount, false);
	Drop->FinishSpawning(Transform);
}
