// Suzune's Night Festival - the three killable yokai.

#include "MatsuriYokaiKinds.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "MatsuriArt.h"
#include "MatsuriGraybox.h"
#include "MatsuriProjectile.h"
#include "MatsuriYokaiBrain.h"
#include "SuzuneMatsuri.h"

using Matsuri::Graybox::EShape;

// ---------------------------------------------------------------- Chochin

namespace ChochinTuning
{
	constexpr float Radius = 45.0f;
	constexpr float HoverHeight = 180.0f;	// body clears waist-high crates (<= 130 cm)
	constexpr float WanderSpeed = 160.0f;
	constexpr float ChaseSpeed = 250.0f;	// the hero walks at ~600: outrun it, do not ignore it
	constexpr int32 HuntCells = 8;
	constexpr float ArenaHuntRange = 1800.0f;
	constexpr float BobAmplitude = 12.0f;
	constexpr float ArriveDistance = 40.0f;
}

AMatsuriChochin::AMatsuriChochin()
{
	MaxHealth = 1.0f;
	Body->SetSphereRadius(ChochinTuning::Radius);
	Lantern = Matsuri::Graybox::MakePart(*this, Visual, TEXT("Lantern"), EShape::Sphere, FVector::ZeroVector, FVector(0.85f, 0.85f, 1.05f));
}

void AMatsuriChochin::BeginPlay()
{
	Super::BeginPlay();
	if (Matsuri::Art::Attach(Visual, Matsuri::ArtTable::Chochin()))
	{
		Lantern->SetVisibility(false);
	}
	else
	{
		Matsuri::Graybox::Paint(Lantern, FLinearColor(1.0f, 0.42f, 0.06f), true, 5.0f);
	}

	SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, ChochinTuning::HoverHeight));
	BobTime = FMath::FRandRange(0.0f, 6.0f);
}

void AMatsuriChochin::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	BobTime += DeltaSeconds;
	Visual->SetRelativeLocation(FVector(0.0f, 0.0f, FMath::Sin(BobTime * 2.0f) * ChochinTuning::BobAmplitude));

	if (IsStunned())
	{
		return;
	}
	const AActor* Target = FindTarget();
	const bool bHunting = ShouldHunt(Target, ChochinTuning::HuntCells, ChochinTuning::ArenaHuntRange);
	if (!bHunting && (!bHasWanderGoal || FVector::Dist2D(WanderGoal, GetActorLocation()) < ChochinTuning::ArriveDistance))
	{
		WanderGoal = PickWanderGoal();
		bHasWanderGoal = true;
	}
	float Speed = ChochinTuning::WanderSpeed;
	FVector Goal = WanderGoal;
	if (bHunting)
	{
		using namespace Matsuri::YokaiBrain;
		const EChochinMove Move = ChochinMove(IsWatchedBy(Target), FVector::Dist2D(Target->GetActorLocation(), GetActorLocation()));
		Speed = ChochinSpeed(Move);	// watched: just a lantern hanging there... edging closer
		Goal = Move == EChochinMove::Lunge && CanSee(Target) ? Target->GetActorLocation() : NextStepToward(Target->GetActorLocation());
	}
	Goal.Z = ChochinTuning::HoverHeight;

	const FVector ToGoal = Goal - GetActorLocation();
	MoveSwept(ToGoal.GetSafeNormal2D() * FMath::Min(Speed * DeltaSeconds, ToGoal.Size2D()));
	FaceTowards(Goal);
}

void AMatsuriChochin::SpawnDrops()
{
	DropCollectible(EMatsuriCollectible::Koban);
}

// ---------------------------------------------------------------- Kasa

namespace KasaTuning
{
	constexpr float Radius = 50.0f;
	constexpr int32 HuntCells = 6;
	constexpr float ArenaHuntRange = 1400.0f;
	constexpr float HopDistance = 300.0f;
	constexpr float IdleHopDistance = 110.0f;
	constexpr float HopSeconds = 0.55f;
	constexpr float HopHeight = 150.0f;
	constexpr float RestSeconds = 0.6f;
	constexpr float DropGinkaChance = 0.6f;
	constexpr float LurkCheckSeconds = 0.1f;	// how often a lurking umbrella looks round its corner
	constexpr float PounceSeconds = 0.45f;
	constexpr float PounceMax = 520.0f;			// a long leap, straight at the player
	constexpr float PounceHeight = 190.0f;
}

AMatsuriKasa::AMatsuriKasa()
{
	MaxHealth = 3.0f;
	Body->SetSphereRadius(KasaTuning::Radius);
	Canopy = Matsuri::Graybox::MakePart(*this, Visual, TEXT("Canopy"), EShape::Cone, FVector(0.0f, 0.0f, 25.0f), FVector(1.1f, 1.1f, 0.7f));
	Handle = Matsuri::Graybox::MakePart(*this, Visual, TEXT("Handle"), EShape::Cylinder, FVector(0.0f, 0.0f, -25.0f), FVector(0.12f, 0.12f, 0.6f));
}

void AMatsuriKasa::BeginPlay()
{
	Super::BeginPlay();
	if (Matsuri::Art::Attach(Visual, Matsuri::ArtTable::Kasa()))
	{
		Canopy->SetVisibility(false);
		Handle->SetVisibility(false);
	}
	else
	{
		Matsuri::Graybox::Paint(Canopy, FLinearColor(0.45f, 0.10f, 0.55f));
		Matsuri::Graybox::Paint(Handle, FLinearColor(0.30f, 0.18f, 0.08f));
	}
	SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, KasaTuning::Radius + 5.0f));
}

void AMatsuriKasa::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bHopping)
	{
		HopElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(HopElapsed / HopSeconds, 0.0f, 1.0f);
		Visual->SetRelativeLocation(FVector(0.0f, 0.0f, FMath::Sin(Alpha * PI) * HopHeight));
		MoveSwept(HopVelocity * DeltaSeconds);
		if (Alpha >= 1.0f)
		{
			bHopping = false;
			RestLeft = KasaTuning::RestSeconds;
		}
		return;
	}

	RestLeft -= DeltaSeconds;
	if (RestLeft > 0.0f || IsStunned())
	{
		return;
	}

	const AActor* Target = FindTarget();
	const bool bHunting = ShouldHunt(Target, KasaTuning::HuntCells, KasaTuning::ArenaHuntRange);
	if (bHunting)
	{
		using namespace Matsuri::YokaiBrain;
		int32 Steps = INDEX_NONE;
		const FVector Waypoint = NextStepToward(Target->GetActorLocation(), &Steps);
		const float Distance = FVector::Dist2D(Target->GetActorLocation(), GetActorLocation());
		const EKasaMove Move = KasaMove(CanSee(Target), Distance, Steps);
		UE_LOG(LogSuzuneMatsuri, Verbose, TEXT("%s: sight %d, %.0f cm, steps %d -> move %d"), *GetName(), CanSee(Target) ? 1 : 0, Distance, Steps, static_cast<int32>(Move));
		switch (Move)
		{
		case EKasaMove::Lurk:
			FaceTowards(Waypoint);	// still, facing the corner it will come round
			RestLeft = KasaTuning::LurkCheckSeconds;
			return;
		case EKasaMove::Pounce:
		{
			const FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			HopVelocity = Direction * (FMath::Min(Distance, KasaTuning::PounceMax) / KasaTuning::PounceSeconds);
			HopSeconds = KasaTuning::PounceSeconds;
			HopHeight = KasaTuning::PounceHeight;
			HopElapsed = 0.0f;
			bHopping = true;
			FaceTowards(Target->GetActorLocation());
			return;
		}
		case EKasaMove::Path:
			break;
		}
	}
	HopSeconds = KasaTuning::HopSeconds;
	HopHeight = KasaTuning::HopHeight;
	const FVector Goal = bHunting ? NextStepToward(Target->GetActorLocation()) : PickWanderGoal();
	const FVector Direction = (Goal - GetActorLocation()).GetSafeNormal2D();
	// Never hop past the waypoint: overshooting a doorway would land the umbrella against the wall.
	const float Distance = FMath::Min(bHunting ? KasaTuning::HopDistance : KasaTuning::IdleHopDistance,
		FMath::Max(60.0f, static_cast<float>(FVector::Dist2D(Goal, GetActorLocation()))));

	HopVelocity = Direction * (Distance / HopSeconds);
	HopElapsed = 0.0f;
	bHopping = true;
	FaceTowards(GetActorLocation() + Direction);
}

void AMatsuriKasa::SpawnDrops()
{
	if (FMath::FRand() < KasaTuning::DropGinkaChance)
	{
		DropCollectible(EMatsuriCollectible::Ginka);
	}
	else
	{
		DropAmmo(5);
	}
}

// ---------------------------------------------------------------- Kappa

namespace KappaTuning
{
	constexpr float Radius = 50.0f;
	constexpr float TooClose = 700.0f;
	constexpr float TooFar = 1300.0f;
	constexpr float ThrowRange = AMatsuriKappa::ThrowRange;
	constexpr int32 HuntCells = 10;
	constexpr float ArenaHuntRange = 2600.0f;
	constexpr float MoveSpeed = 170.0f;
	constexpr float ThrowInterval = 2.2f;
	constexpr float CucumberSpeed = 1600.0f;
	constexpr float MagatamaChance = 0.15f;
	constexpr int32 AmmoDrop = 6;
	constexpr float DashSpeed = 360.0f;			// scrambling for cover is quicker than stalking
	constexpr float CoverReplanSeconds = 0.4f;
	constexpr int32 CoverDirections = 16;
	const FLinearColor CucumberColor(0.25f, 0.75f, 0.15f);
}

AMatsuriKappa::AMatsuriKappa()
{
	MaxHealth = 4.0f;
	Body->SetSphereRadius(KappaTuning::Radius);
	Shell = Matsuri::Graybox::MakePart(*this, Visual, TEXT("Shell"), EShape::Sphere, FVector::ZeroVector, FVector(0.9f, 0.9f, 1.15f));
	Dish = Matsuri::Graybox::MakePart(*this, Visual, TEXT("Dish"), EShape::Cylinder, FVector(0.0f, 0.0f, 58.0f), FVector(0.55f, 0.55f, 0.06f));
}

void AMatsuriKappa::BeginPlay()
{
	Super::BeginPlay();
	if (Matsuri::Art::Attach(Visual, Matsuri::ArtTable::Kappa()))
	{
		Shell->SetVisibility(false);
		Dish->SetVisibility(false);
	}
	else
	{
		Matsuri::Graybox::Paint(Shell, FLinearColor(0.10f, 0.45f, 0.20f));
		Matsuri::Graybox::Paint(Dish, FLinearColor(0.85f, 0.85f, 0.75f));
	}
	SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, KappaTuning::Radius + 10.0f));
}

void AMatsuriKappa::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ThrowCooldown -= DeltaSeconds;

	const AActor* Player = FindTarget();
	if (!Player || IsStunned() || !ShouldHunt(Player, KappaTuning::HuntCells, KappaTuning::ArenaHuntRange))
	{
		return;
	}
	using namespace Matsuri::YokaiBrain;
	const FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
	const float Distance = ToPlayer.Size2D();
	const bool bInSight = CanSee(Player);
	const bool bInRange = Distance < KappaTuning::ThrowRange;
	const bool bReady = ThrowCooldown <= 0.0f && bInRange;
	FaceTowards(Player->GetActorLocation());

	auto MoveTo = [this, DeltaSeconds](const FVector& Goal, float Speed)
	{
		const FVector Step = Goal - GetActorLocation();
		MoveSwept(Step.GetSafeNormal2D() * FMath::Min(Speed * DeltaSeconds, static_cast<float>(Step.Size2D())));
	};
	switch (KappaMove(bInSight, bReady, bInRange))
	{
	case EKappaMove::Approach:
		MoveTo(NextStepToward(Player->GetActorLocation()), KappaTuning::MoveSpeed);
		break;
	case EKappaMove::Throw:
		Throw(*Player);
		ThrowCooldown = KappaTuning::ThrowInterval;
		bHasCover = false;	// find fresh cover after every throw
		bHasPeek = false;
		break;
	case EKappaMove::Peek:
	{
		// Step out from behind cover to the nearest place with a clear throw (not straight at the
		// player: from cover that line runs through the cover).
		if ((CoverClock -= DeltaSeconds) <= 0.0f || !bHasPeek)
		{
			CoverClock = KappaTuning::CoverReplanSeconds;
			bHasPeek = FindSpot(*Player, false, PeekSpot);
		}
		MoveTo(bHasPeek ? PeekSpot : NextStepToward(Player->GetActorLocation()), KappaTuning::DashSpeed);
		break;
	}
	case EKappaMove::Hide:
		if ((CoverClock -= DeltaSeconds) <= 0.0f || !bHasCover)
		{
			CoverClock = KappaTuning::CoverReplanSeconds;
			bHasCover = FindSpot(*Player, true, CoverSpot);
		}
		if (bHasCover)
		{
			MoveTo(CoverSpot, KappaTuning::DashSpeed);
		}
		else if (Distance < KappaTuning::TooClose)
		{
			MoveSwept(-ToPlayer.GetSafeNormal2D() * KappaTuning::MoveSpeed * DeltaSeconds);	// no cover: back away
		}
		break;
	case EKappaMove::Hold:
		break;
	}
}

bool AMatsuriKappa::FindSpot(const AActor& Player, bool bWantHidden, FVector& OutSpot) const
{
	// Sense the ground around it: could it get there, and would the player see it there?
	TArray<Matsuri::YokaiBrain::FCoverSpot> Spots;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MatsuriKappaCover), false, this);
	for (const float Radius : { 160.0f, 300.0f, 440.0f })
	{
		for (int32 i = 0; i < KappaTuning::CoverDirections; ++i)
		{
			const FVector Spot = GetActorLocation() + FRotator(0.0f, 360.0f * i / KappaTuning::CoverDirections, 0.0f).Vector() * Radius;
			Matsuri::YokaiBrain::FCoverSpot Sensed;
			Sensed.Location = Spot;
			Sensed.bReachable = !GetWorld()->SweepTestByChannel(GetActorLocation(), Spot, FQuat::Identity, ECC_WorldStatic,
				FCollisionShape::MakeSphere(KappaTuning::Radius), Params);
			Sensed.bHiddenFromPlayer = !IsVisibleFrom(&Player, Spot + FVector(0.0f, 0.0f, 30.0f));
			Spots.Add(Sensed);
		}
	}
	const int32 Best = bWantHidden ? Matsuri::YokaiBrain::PickCover(Spots, GetActorLocation()) : Matsuri::YokaiBrain::PickPeek(Spots, GetActorLocation());
	if (Best == INDEX_NONE)
	{
		return false;
	}
	OutSpot = Spots[Best].Location;
	return true;
}

void AMatsuriKappa::Throw(const AActor& Target)
{
	// Lead the target: aim where it will be when the cucumber arrives.
	const FVector Hand = GetActorLocation() + FVector(0.0f, 0.0f, 40.0f) + GetActorForwardVector() * (KappaTuning::Radius + 20.0f);
	const float FlightTime = FVector::Dist(Hand, Target.GetActorLocation()) / KappaTuning::CucumberSpeed;
	const FVector Aim = Target.GetActorLocation() + Target.GetVelocity() * FlightTime;

	// Lobbed: aim high enough that gravity brings it down on the target.
	const FVector Launch = Matsuri::Shots::LobDirection(Hand, Aim, KappaTuning::CucumberSpeed,
		Matsuri::Shots::BallisticsFor(EMatsuriShotLook::Cucumber).GravityScale);
	const FTransform Transform(Launch.Rotation(), Hand);
	AMatsuriProjectile* Cucumber = GetWorld()->SpawnActorDeferred<AMatsuriProjectile>(AMatsuriProjectile::StaticClass(), Transform,
		this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	FMatsuriShot Shot;
	Shot.Look = EMatsuriShotLook::Cucumber;
	Shot.Direction = Launch;
	Shot.Speed = KappaTuning::CucumberSpeed;
	Shot.bHostileToPlayer = true;
	Shot.Color = KappaTuning::CucumberColor;
	Shot.Scale = 0.16f;
	Cucumber->Configure(Shot);
	Cucumber->FinishSpawning(Transform);
}

void AMatsuriKappa::SpawnDrops()
{
	DropAmmo(KappaTuning::AmmoDrop);
	if (FMath::FRand() < KappaTuning::MagatamaChance)
	{
		DropCollectible(EMatsuriCollectible::Magatama);
	}
}
