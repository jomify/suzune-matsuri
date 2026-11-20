// Suzune's Night Festival - shots: arrows, shuriken, bells, thrown coins, kappa cucumbers.

#include "MatsuriProjectile.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MatsuriCombat.h"
#include "MatsuriFx.h"
#include "MatsuriArt.h"
#include "MatsuriAudio.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "MatsuriCombatComponent.h"
#include "MatsuriDamageNumber.h"
#include "MatsuriYokai.h"
#include "MatsuriGraybox.h"
#include "SuzuneMatsuri.h"

namespace
{
	constexpr float CollisionRadius = 10.0f;
	constexpr float PlayerShotRadius = 20.0f;		// fat like Overwatch projectiles: near misses on a yokai still land
	constexpr float GlintSeconds = 0.035f;
	constexpr float FlinchDistance = 14.0f;	// cm a struck yokai reels back
	constexpr float StuckSeconds = 6.0f;			// arrows and kunai stay in the wall this long
	constexpr float LifeSeconds = 4.0f;
	constexpr float BounceRange = 900.0f;
	constexpr float HomingAcceleration = 25000.0f;
	constexpr float GlowIntensity = 2500.0f;	// enough to light the floor under a passing shot
	constexpr float GlowRadius = 260.0f;
}

AMatsuriProjectile::AMatsuriProjectile()
{
	PrimaryActorTick.bCanEverTick = true;	// the tracer grows with the distance flown
	InitialLifeSpan = LifeSeconds;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(CollisionRadius);
	Collision->SetCollisionProfileName(TEXT("Projectile"));
	Collision->SetCollisionResponseToChannel(Matsuri::Combat::ProjectileChannel, ECR_Ignore);	// a fan of kunai must not knock each other down
	Collision->SetCanEverAffectNavigation(false);
	RootComponent = Collision;

	Spinner = CreateDefaultSubobject<USceneComponent>(TEXT("Spinner"));
	Spinner->SetupAttachment(Collision);
	Body = Matsuri::Graybox::MakePart(*this, Spinner, TEXT("Body"), Matsuri::Graybox::EShape::Sphere, FVector::ZeroVector, FVector(0.12f));
	Body->SetCastShadow(false);

	// The cone's tip (+Z) turned to point back along the flight (-X).
	Trail = Matsuri::Graybox::MakePart(*this, Collision, TEXT("Trail"), Matsuri::Graybox::EShape::Cone, FVector::ZeroVector, FVector(0.01f), FRotator(90.0f, 0.0f, 0.0f));
	Trail->SetCastShadow(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Collision);
	Glow->SetIntensity(GlowIntensity);
	Glow->SetAttenuationRadius(GlowRadius);
	Glow->SetCastShadows(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	// Shots get a world-space direction. The engine default (true) would rotate it again by the
	// spawn rotation, so shots only flew straight when facing +X.
	Movement->bInitialVelocityInLocalSpace = false;
}

void AMatsuriProjectile::Configure(const FMatsuriShot& InShot)
{
	Shot = InShot;
	Ballistics = Matsuri::Shots::BallisticsFor(Shot.Look);
	Movement->InitialSpeed = Shot.Speed;
	Movement->MaxSpeed = Shot.Speed * 1.5f;	// falling may add a little
	Movement->ProjectileGravityScale = Shot.HomingTarget.IsValid() ? 0.0f : Ballistics.GravityScale;
	if (Ballistics.WorldImpact == EMatsuriShotImpact::Bounce)
	{
		Movement->bShouldBounce = true;
		Movement->Bounciness = Ballistics.Bounciness;
		Movement->Friction = 0.25f;
		WorldBouncesLeft = Ballistics.WorldBounces;
	}
	Movement->Velocity = Shot.Direction.GetSafeNormal() * Shot.Speed;

	if (USceneComponent* Target = Shot.HomingTarget.Get())
	{
		Movement->bIsHomingProjectile = true;
		Movement->HomingTargetComponent = Target;
		Movement->HomingAccelerationMagnitude = HomingAcceleration;
	}
	if (!Shot.bHostileToPlayer)
	{
		Collision->SetSphereRadius(PlayerShotRadius);
		// Pass into yokai (they are WorldDynamic) so one shot can pierce or bounce; ignore the player.
		Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Collision->SetGenerateOverlapEvents(true);
	}
	ApplyLook();
}

void AMatsuriProjectile::ApplyLook()
{
	Glow->SetLightColor(Shot.Color);
	bDressed = Matsuri::Art::Dress(Body, Shot.Art);
	if (bDressed)
	{
		if (Ballistics.SpinAxis == EMatsuriSpinAxis::None)
		{
			const float HalfLength = Body->GetStaticMesh()->GetBoundingBox().GetExtent().X * Shot.Art.Scale;
			Spinner->SetRelativeLocation(FVector(-HalfLength, 0.0f, 0.0f));
		}
		return;
	}
	const float S = Shot.Scale;
	switch (Shot.Look)
	{
	case EMatsuriShotLook::Arrow:
		Body->SetStaticMesh(Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Cylinder));
		Body->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
		Body->SetRelativeScale3D(FVector(S * 0.35f, S * 0.35f, S * 7.0f));
		break;
	case EMatsuriShotLook::Star:
		Body->SetStaticMesh(Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Cube));
		Body->SetRelativeRotation(FRotator(0.0f, 45.0f, 0.0f));
		Body->SetRelativeScale3D(FVector(S * 1.6f, S * 1.6f, S * 0.15f));
		break;
	case EMatsuriShotLook::Orb:
		Body->SetRelativeScale3D(FVector(S));
		break;
	}
}

void AMatsuriProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Spin: a kunai tumbles end over end, a bell wobbles on its cord, a coin flips.
	if (Ballistics.SpinDegreesPerSecond > 0.0f)
	{
		SpinAngle = FMath::Fmod(SpinAngle + Ballistics.SpinDegreesPerSecond * DeltaSeconds, 360.0f);
		const FRotator Spin = Ballistics.SpinAxis == EMatsuriSpinAxis::EndOverEnd ? FRotator(-SpinAngle, 0.0f, 0.0f)
			: Ballistics.SpinAxis == EMatsuriSpinAxis::Wobble ? FRotator(0.0f, 0.0f, 25.0f * FMath::Sin(FMath::DegreesToRadians(SpinAngle)))
			: FRotator(0.0f, 0.0f, SpinAngle);
		Spinner->SetRelativeRotation(Spin);
	}
	if (Ballistics.bGlints && (GlintClock += DeltaSeconds) >= GlintSeconds)
	{
		GlintClock = 0.0f;
		SpawnGlint();
	}

	const float Length = FMath::Min(static_cast<float>(FVector::Dist(GetActorLocation(), LaunchLocation)), Shot.Speed * Ballistics.TrailSeconds);
	const float Width = Ballistics.TrailWidth;
	Trail->SetVisibility(Length > 5.0f);
	Trail->SetRelativeLocation(FVector(-Length * 0.5f, 0.0f, 0.0f));
	Trail->SetRelativeScale3D(FVector(Width, Width, Length) / 100.0f);	// engine cone: 1 m tall, 1 m across
}

void AMatsuriProjectile::Deflect()
{
	Matsuri::Fx::Burst(GetWorld(), GetActorLocation(), FLinearColor(1.0f, 0.9f, 0.5f), 10, 500.0f, 10.0f, 0.35f);
	Destroy();
}

void AMatsuriProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (!bDressed)
	{
		Matsuri::Graybox::Paint(Body, Shot.Color, true);
	}
	Matsuri::Graybox::Paint(Trail, Shot.Color, true, 5.0f);
	LaunchLocation = GetActorLocation();
	UE_LOG(LogSuzuneMatsuri, Verbose, TEXT("%s launched at %s, velocity %s, gravity %.2f, bounce %d"), *GetName(), *LaunchLocation.ToCompactString(),
		*Movement->Velocity.ToCompactString(), Movement->ProjectileGravityScale, Movement->bShouldBounce ? 1 : 0);
	Movement->OnProjectileStop.AddDynamic(this, &AMatsuriProjectile::HandleStop);
	if (!Shot.bHostileToPlayer)
	{
		Matsuri::Fx::MuzzleFlash(GetWorld(), LaunchLocation, Shot.Direction, Shot.Color);
	}
	// Never collide with whoever fired it (pawn instigator for the player, owner for yokai).
	for (AActor* Shooter : { static_cast<AActor*>(GetInstigator()), GetOwner() })
	{
		if (Shooter)
		{
			Collision->IgnoreActorWhenMoving(Shooter, true);
		}
	}
	Collision->OnComponentHit.AddDynamic(this, &AMatsuriProjectile::HandleHit);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AMatsuriProjectile::HandleOverlap);

	// Overlaps found while spawning fire before the handler above is bound, so a yokai pressed
	// against the muzzle used to be flown straight through. Strike it now.
	TArray<AActor*> Touching;
	Collision->GetOverlappingActors(Touching);
	for (AActor* Other : Touching)
	{
		if (!IsValid(this) || IsActorBeingDestroyed())
		{
			break;
		}
		HandleOverlap(Collision, Other, nullptr, 0, false, FHitResult());
	}
}

void AMatsuriProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor && OtherActor != GetInstigator() && OtherActor != GetOwner())
	{
		if (Shot.bHostileToPlayer && Matsuri::Combat::IsPlayerPawn(OtherActor))
		{
			Matsuri::Combat::HurtPlayer(this, this);
		}
		else if (!Shot.bHostileToPlayer && OtherActor->ActorHasTag(Matsuri::Combat::YokaiTag))
		{
			StrikeYokai(OtherActor);
		}
	}
	const FVector At = Hit.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(Hit.ImpactPoint);
	const bool bWorld = !OtherActor || !(Matsuri::Combat::IsPlayerPawn(OtherActor) || OtherActor->ActorHasTag(Matsuri::Combat::YokaiTag));
	if (bWorld && Ballistics.WorldImpact == EMatsuriShotImpact::Bounce && WorldBouncesLeft-- > 0)
	{
		// The movement component reflects the velocity; a clink of sparks marks the ricochet.
		UE_LOG(LogSuzuneMatsuri, Verbose, TEXT("%s bounced off %s at %s, velocity %s"), *GetName(), *GetNameSafe(OtherActor), *At.ToCompactString(), *Movement->Velocity.ToCompactString());
		Matsuri::Fx::Burst(GetWorld(), At, Shot.Color, 4, 260.0f, 4.0f, 0.2f, Hit.ImpactNormal);
		Matsuri::Audio::PlayAt(this, Shot.Look == EMatsuriShotLook::Coin ? EMatsuriSfx::CoinBounce : EMatsuriSfx::BellBounce, At);
		return;
	}
	if (bWorld && Ballistics.WorldImpact == EMatsuriShotImpact::Stick && bDressed)
	{
		StickInto(Hit);
		Matsuri::Audio::PlayAt(this, EMatsuriSfx::ArrowThunk, At);
		Matsuri::Fx::Impact(GetWorld(), At, Hit.ImpactNormal, FLinearColor(0.55f, 0.45f, 0.32f), false, false);	// dust and splinters, no scorch
	}
	else
	{
		Matsuri::Fx::Impact(GetWorld(), At, Hit.ImpactNormal, Shot.Color, false);
	}
	UE_LOG(LogSuzuneMatsuri, Verbose, TEXT("%s stopped by %s (%s) at %s"), *GetName(), *GetNameSafe(OtherActor), *GetNameSafe(OtherComp), *GetActorLocation().ToCompactString());
	Destroy();
}

void AMatsuriProjectile::HandleStop(const FHitResult& ImpactResult)
{
	Destroy();	// a bell that has rolled to a stop
}

void AMatsuriProjectile::StickInto(const FHitResult& Hit)
{
	// The head sinks EmbedDepth past the point of contact, the shaft along the line of flight.
	const FVector Flight = GetActorForwardVector();
	const float HalfLength = Body->GetStaticMesh()->GetBoundingBox().GetExtent().X * Shot.Art.Scale;
	const FVector Contact = Hit.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(Hit.ImpactPoint);
	const FQuat Rotation = Flight.ToOrientationQuat() * Shot.Art.Rotation.Quaternion();
	AStaticMeshActor* Stuck = GetWorld()->SpawnActor<AStaticMeshActor>(Contact + Flight * (Ballistics.EmbedDepth - HalfLength), Rotation.Rotator());
	if (!Stuck)
	{
		return;
	}
	Stuck->Tags.Add(TEXT("MatsuriStuck"));
	UStaticMeshComponent* Mesh = Stuck->GetStaticMeshComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetStaticMesh(Body->GetStaticMesh());
	Stuck->SetActorScale3D(FVector(Shot.Art.Scale));
	Stuck->SetLifeSpan(StuckSeconds);
}

void AMatsuriProjectile::SpawnGlint()
{
	FMatsuriFxSpec Glint;
	Glint.Color = Shot.Color;
	Glint.Glow = 12.0f;
	Glint.StartSize = FVector(4.0f);
	Glint.EndSize = FVector(0.5f);
	Glint.Velocity = FMath::VRand() * 40.0f;
	Glint.Gravity = 150.0f;
	Glint.Lifetime = 0.3f;
	Matsuri::Fx::Spawn(GetWorld(), GetActorLocation(), Glint);
}

void AMatsuriProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Shot.bHostileToPlayer || !OtherActor || !OtherActor->ActorHasTag(Matsuri::Combat::YokaiTag) || Struck.Contains(OtherActor))
	{
		return;
	}
	StrikeYokai(OtherActor);

	if (Shot.Bounces > 0)
	{
		if (const AActor* Next = FindBounceTarget())
		{
			--Shot.Bounces;
			Movement->Velocity = (Next->GetActorLocation() - GetActorLocation()).GetSafeNormal() * Shot.Speed;
			return;
		}
	}
	if (Shot.Pierce > 0)
	{
		--Shot.Pierce;
		return;
	}
	Destroy();
}

void AMatsuriProjectile::StrikeYokai(AActor* Yokai)
{
	Struck.Add(Yokai);
	Matsuri::Fx::Impact(GetWorld(), GetActorLocation(), (GetActorLocation() - Yokai->GetActorLocation()).GetSafeNormal(), Shot.Color, true);
	UE_LOG(LogSuzuneMatsuri, Verbose, TEXT("%s struck %s for %.1f"), *GetName(), *Yokai->GetName(), Shot.Damage);
	AController* ShooterController = GetInstigator() ? GetInstigator()->GetController() : nullptr;
	const FVector HitAt = Yokai->GetActorLocation();
	UGameplayStatics::ApplyDamage(Yokai, Shot.Damage, ShooterController, this, nullptr);
	AMatsuriYokai* StruckYokai = Cast<AMatsuriYokai>(Yokai);
	const bool bKilled = !IsValid(Yokai) || Yokai->IsActorBeingDestroyed() || (StruckYokai && StruckYokai->GetHealth() <= 0.0f);
	if (!Shot.bHostileToPlayer)
	{
		AMatsuriDamageNumber::Pop(GetWorld(), HitAt, Shot.Damage, false, bKilled);
		Matsuri::Audio::Play2D(this, bKilled ? EMatsuriSfx::Kill : EMatsuriSfx::HitTick, bKilled ? 0.8f : 0.6f);	// the hit confirm, heard as hers
		if (StruckYokai && !bKilled)
		{
			StruckYokai->Knockback(GetVelocity().GetSafeNormal2D() * FlinchDistance);	// it reels from the hit
		}
	}
	if (UMatsuriCombatComponent* Weapon = GetInstigator() ? GetInstigator()->FindComponentByClass<UMatsuriCombatComponent>() : nullptr)
	{
		Weapon->ConfirmHit();	// hit marker on the same frame as the damage
	}
}

AActor* AMatsuriProjectile::FindBounceTarget() const
{
	AActor* Best = nullptr;
	float BestDistanceSq = FMath::Square(BounceRange);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!IsValid(Candidate) || !Candidate->ActorHasTag(Matsuri::Combat::YokaiTag) || Struck.Contains(Candidate))
		{
			continue;
		}
		const float DistanceSq = FVector::DistSquared(Candidate->GetActorLocation(), GetActorLocation());
		if (DistanceSq < BestDistanceSq)
		{
			Best = Candidate;
			BestDistanceSq = DistanceSq;
		}
	}
	return Best;
}

FMatsuriBallistics Matsuri::Shots::BallisticsFor(EMatsuriShotLook Look)
{
	FMatsuriBallistics B;
	switch (Look)
	{
	case EMatsuriShotLook::Arrow:		// a light arc, point-first; sinks in where it lands
		B.GravityScale = 0.35f;
		B.WorldImpact = EMatsuriShotImpact::Stick;
		B.EmbedDepth = 14.0f;
		B.TrailWidth = 2.5f;
		B.TrailSeconds = 0.06f;	// long and thin: the streak of a fletched shaft
		break;
	case EMatsuriShotLook::Star:		// a kunai: tumbles end over end, bites into wood
		B.GravityScale = 0.25f;
		B.SpinDegreesPerSecond = 1440.0f;
		B.SpinAxis = EMatsuriSpinAxis::EndOverEnd;
		B.WorldImpact = EMatsuriShotImpact::Stick;
		B.EmbedDepth = 5.0f;
		B.TrailWidth = 4.0f;
		B.TrailSeconds = 0.022f;
		break;
	case EMatsuriShotLook::Orb:			// a bell: heavier, rings off walls twice
		B.GravityScale = 0.6f;
		B.SpinDegreesPerSecond = 540.0f;
		B.SpinAxis = EMatsuriSpinAxis::Wobble;
		B.WorldImpact = EMatsuriShotImpact::Bounce;
		B.WorldBounces = 2;
		B.Bounciness = 0.35f;	// heavy brass: a short hop, not a rocket
		B.TrailWidth = 6.0f;
		B.TrailSeconds = 0.03f;
		B.bGlints = true;
		break;
	case EMatsuriShotLook::Coin:		// a heavy coin: flips, bounces once
		B.GravityScale = 0.8f;
		B.SpinDegreesPerSecond = 1080.0f;
		B.SpinAxis = EMatsuriSpinAxis::Flip;
		B.WorldImpact = EMatsuriShotImpact::Bounce;
		B.WorldBounces = 1;
		B.Bounciness = 0.4f;
		B.TrailWidth = 7.0f;
		B.TrailSeconds = 0.03f;
		B.bGlints = true;
		break;
	case EMatsuriShotLook::Cucumber:	// lobbed by the kappa, tumbling, splats
		B.GravityScale = 0.5f;
		B.SpinDegreesPerSecond = 720.0f;
		B.SpinAxis = EMatsuriSpinAxis::EndOverEnd;
		B.TrailWidth = 5.0f;
		B.TrailSeconds = 0.02f;
		break;
	}
	return B;
}

FVector Matsuri::Shots::LobDirection(const FVector& From, const FVector& To, float Speed, float GravityScale)
{
	const FVector Delta = To - From;
	const FVector Flat(Delta.X, Delta.Y, 0.0f);
	const float Distance = Flat.Size();
	const float G = 980.0f * GravityScale;
	if (G <= 0.0f || Distance < 1.0f)
	{
		return Delta.GetSafeNormal();
	}
	// Low solution of the launch angle: tan(a) = (v^2 - sqrt(v^4 - g (g d^2 + 2 h v^2))) / (g d)
	const float V2 = Speed * Speed;
	const float Root = V2 * V2 - G * (G * Distance * Distance + 2.0f * Delta.Z * V2);
	const float Angle = Root < 0.0f ? PI / 4.0f : FMath::Atan((V2 - FMath::Sqrt(Root)) / (G * Distance));	// out of reach: throw as far as possible
	return (Flat / Distance * FMath::Cos(Angle) + FVector::UpVector * FMath::Sin(Angle)).GetSafeNormal();
}
