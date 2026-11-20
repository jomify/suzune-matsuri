// Suzune's Night Festival - 九尾狐「玉藻」: the boss herself.

#include "MatsuriBoss.h"
#include "Components/SkeletalMeshComponent.h"
#include "MatsuriToon.h"
#include "Components/SphereComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MatsuriArt.h"
#include "MatsuriAudio.h"
#include "MatsuriMusic.h"
#include "MatsuriBossBrainComponent.h"
#include "MatsuriCollectible.h"
#include "MatsuriCombat.h"
#include "MatsuriDamageNumber.h"
#include "MatsuriDanmakuField.h"
#include "MatsuriFx.h"
#include "MatsuriGrappleComponent.h"
#include "MatsuriGraybox.h"
#include "MatsuriRunSubsystem.h"
#include "SuzuneMatsuri.h"

#define LOCTEXT_NAMESPACE "MatsuriBoss"

namespace
{
	TAutoConsoleVariable<float> CVarBossHealth(
		TEXT("matsuri.BossHealth"), AMatsuriBoss::DefaultMaxHealth,
		TEXT("Tamamo's health (three equal bars). Tests and demos shorten the fight."));

	constexpr int32 TailCount = 9;
	const TCHAR* const TamamoAsset = TEXT("SK_Boss_Tamamo");
	constexpr float TamamoScale = 3.0f;			// rigged Tripo characters come out 1 m tall: she stands 3 m
	constexpr float TamamoFeet = -160.0f;			// her feet below the body sphere's centre
	constexpr float BodyRadius = 170.0f;
	constexpr float GlideSpeed = 900.0f;
	constexpr float BobHeight = 25.0f;
	constexpr float HitPunchSeconds = 0.12f;
	constexpr float MinPlayerDistance = 1200.0f;	// she keeps some room
	constexpr float MaxPlayerDistance = 3200.0f;
	const FLinearColor RobeColor(0.95f, 0.93f, 0.9f);
	const FLinearColor HakamaColor(0.75f, 0.08f, 0.1f);
	const FLinearColor TailColor(1.0f, 0.78f, 0.35f);
	const FLinearColor MaskColor(1.0f, 1.0f, 1.0f);

	FString HeroId(EMatsuriHero Hero)
	{
		switch (Hero)
		{
		case EMatsuriHero::Sakura:	return TEXT("sakura (bow, charged arrows)");
		case EMatsuriHero::Suzune:	return TEXT("suzune (kunai fans, blink)");
		case EMatsuriHero::Mio:		return TEXT("mio (bouncing bells)");
		case EMatsuriHero::Count:	break;
		}
		return TEXT("unknown");
	}

	const TCHAR* PhaseLine(int32 Phase)
	{
		return Phase >= 3 ? TEXT("……本気で行くわよ。") : TEXT("ふふ、少しは楽しめそう。");
	}
}

float AMatsuriBoss::PhaseThreshold(int32 PhaseIndex)
{
	return PhaseIndex >= 2 ? 1.0f / 3.0f : 2.0f / 3.0f;
}

int32 AMatsuriBoss::PhaseFor(float HealthFraction)
{
	return HealthFraction > PhaseThreshold(1) ? 1 : HealthFraction > PhaseThreshold(2) ? 2 : 3;
}

AMatsuriBoss::AMatsuriBoss()
{
	PrimaryActorTick.bCanEverTick = true;
	Tags.Add(Matsuri::Combat::YokaiTag);	// player shots, ults and bells look for this

	Body = CreateDefaultSubobject<USphereComponent>(TEXT("Body"));
	Body->InitSphereRadius(BodyRadius);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_WorldDynamic);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);		// the crosshair converges on her
	Body->SetCollisionResponseToChannel(Matsuri::Combat::ProjectileChannel, ECR_Block);
	Body->SetGenerateOverlapEvents(true);
	Body->SetCanEverAffectNavigation(false);
	RootComponent = Body;

	Visual = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Body);

	using namespace Matsuri::Graybox;
	// A gray-box Tamamo until the Tripo model arrives: robe, head, white fox mask, ears, a halo, nine tails.
	MakePart(*this, Visual, TEXT("Robe"), EShape::Cone, FVector(0, 0, -90), FVector(1.5f, 1.5f, 2.2f));
	MakePart(*this, Visual, TEXT("Hakama"), EShape::Cylinder, FVector(0, 0, -150), FVector(1.25f, 1.25f, 0.6f));
	MakePart(*this, Visual, TEXT("Head"), EShape::Sphere, FVector(0, 0, 70), FVector(0.75f));
	MakePart(*this, Visual, TEXT("Mask"), EShape::Cone, FVector(32, 0, 68), FVector(0.45f, 0.45f, 0.55f), FRotator(-90, 0, 0));
	MakePart(*this, Visual, TEXT("EarL"), EShape::Cone, FVector(0, -24, 120), FVector(0.22f, 0.22f, 0.45f), FRotator(0, 0, -15));
	MakePart(*this, Visual, TEXT("EarR"), EShape::Cone, FVector(0, 24, 120), FVector(0.22f, 0.22f, 0.45f), FRotator(0, 0, 15));
	Halo = MakePart(*this, Visual, TEXT("Halo"), EShape::Cylinder, FVector(-60, 0, 80), FVector(2.4f, 2.4f, 0.05f), FRotator(90, 0, 0));
	for (int32 i = 0; i < TailCount; ++i)
	{
		const float Spread = FMath::Lerp(-70.0f, 70.0f, static_cast<float>(i) / (TailCount - 1));
		UStaticMeshComponent* Tail = MakePart(*this, Visual, *FString::Printf(TEXT("Tail%d"), i), EShape::Sphere,
			FVector(-110, 0, -40), FVector(0.4f, 0.4f, 2.6f), FRotator(-55.0f, 0.0f, Spread));
		Tails.Add(Tail);
	}
	Brain = CreateDefaultSubobject<UMatsuriBossBrainComponent>(TEXT("Brain"));
}

void AMatsuriBoss::BeginPlay()
{
	Super::BeginPlay();
	MaxHealth = FMath::Max(3.0f, CVarBossHealth.GetValueOnGameThread());
	Health = MaxHealth;
	Random.Initialize(static_cast<int32>(FPlatformTime::Cycles()));

	TArray<UStaticMeshComponent*> Parts;
	GetComponents(Parts);
	for (UStaticMeshComponent* Part : Parts)
	{
		const FString Name = Part->GetName();
		if (Name.StartsWith(TEXT("Tail")))
		{
			Matsuri::Graybox::Paint(Part, TailColor, true, 3.0f);
		}
		else if (Name == TEXT("Hakama"))
		{
			Matsuri::Graybox::Paint(Part, HakamaColor);
		}
		else if (Name == TEXT("Mask"))
		{
			Matsuri::Graybox::Paint(Part, MaskColor, true, 1.5f);
		}
		else if (Part != Halo)
		{
			Matsuri::Graybox::Paint(Part, RobeColor);
		}
	}
	Matsuri::Graybox::Paint(Halo, TailColor, true, 2.0f);
	HaloMaterial = Cast<UMaterialInstanceDynamic>(Halo->GetMaterial(0));
	DressInArt();
}

void AMatsuriBoss::DressInArt()
{
	USkeletalMesh* Mesh = Matsuri::Art::CharacterMesh(TamamoAsset, TEXT("Idle"));
	if (!Mesh)
	{
		return;	// no Tripo model imported: the gray-box Tamamo fights
	}
	Figure = NewObject<USkeletalMeshComponent>(this, TEXT("Figure"));
	Figure->SetSkeletalMesh(Mesh);
	Matsuri::Toon::Apply(Figure, true);
	Figure->SetupAttachment(Visual);
	Figure->SetRelativeLocation(FVector(0.0f, 0.0f, TamamoFeet));
	Figure->SetRelativeScale3D(FVector(TamamoScale));
	Figure->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Figure->RegisterComponent();
	if (UAnimSequence* Idle = Matsuri::Art::CharacterAnim(TamamoAsset, TEXT("Idle")))
	{
		Figure->PlayAnimation(Idle, true);
	}
	// The model has her robe, mask and tails: hide the stand-in's, keep the halo (it telegraphs her skills).
	TArray<UStaticMeshComponent*> Parts;
	GetComponents(Parts);
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part != Halo)
		{
			Part->SetVisibility(false);
		}
	}
	Tails.Reset();
}

void AMatsuriBoss::Setup(AMatsuriDanmakuField* InField, const FMatsuriBossArena& InArena)
{
	Field = InField;
	Arena = InArena;
	Anchor = 0;
	SetActorLocation(Arena.Anchors.IsValidIndex(Anchor) ? Arena.Anchors[Anchor] : GetActorLocation());
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	HealthAtStart = Run ? Run->GetRunState().Health : 0;
	Brain->BeginFight(Run ? HeroId(Run->GetRunState().Hero) : FString(TEXT("unknown")), Arena);
	bFightOpen = true;
	SetTaunt(TEXT("ようこそ、私の祭りへ。"));
}

void AMatsuriBoss::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bFightOpen && Field)
	{
		Brain->StepFinished(Field->GetStats(), GetHealthFraction());	// keep the last step; an abandoned fight has no verdict
	}
	Super::EndPlay(EndPlayReason);
}

FMatsuriBossView AMatsuriBoss::MakeView() const
{
	FMatsuriBossView View;
	View.Boss = GetActorLocation();
	View.Center = Arena.Center;
	View.GroundZ = Arena.GroundZ;
	View.HalfExtent = Arena.HalfExtent;
	View.Phase = Phase;
	View.Anchors = Arena.Anchors;
	for (const FMatsuriArenaZone& Zone : Arena.Zones)
	{
		View.Zones.Add(Zone.Point);
	}
	if (!Field || !Field->GetPlayerHitSphere(View.Player))
	{
		View.Player = Arena.PlayerStart;
	}
	return View;
}

bool AMatsuriBoss::IsShielded() const
{
	FVector Player;
	return Field && Field->GetPlayerHitSphere(Player) && Matsuri::BossArena::IsShielded(Arena, GetActorLocation(), Player);
}

FMatsuriBossObservation AMatsuriBoss::Observe() const
{
	FMatsuriBossObservation Raw;
	Raw.Phase = Phase;
	Raw.BossHealth = GetHealthFraction();
	if (const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this))
	{
		const FMatsuriRunState& State = Run->GetRunState();
		Raw.Hero = HeroId(State.Hero);
		Raw.PlayerHealth = State.Health;
		Raw.PlayerMaxHealth = State.GetMaxHealth();
		Raw.PlayerAmmo = State.Ammo;
		Raw.PlayerMaxAmmo = State.GetAmmoCap();
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const ACharacter* Player = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Player)
	{
		return Raw;
	}
	const FVector Feet = Player->GetActorLocation() - FVector(0.0f, 0.0f, Player->GetSimpleCollisionHalfHeight());
	Raw.Zone = Matsuri::BossArena::ZoneAt(Arena, Feet);
	Raw.ZoneName = Arena.Zones.IsValidIndex(Raw.Zone) ? Arena.Zones[Raw.Zone].Name.ToString() : FString();
	Raw.bPlayerHigh = (Arena.Zones.IsValidIndex(Raw.Zone) && Arena.Zones[Raw.Zone].bHigh) || Feet.Z > Arena.GroundZ + 300.0f;
	Raw.bPlayerShielded = IsShielded();
	const UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Raw.bPlayerAirborne = Movement && (Movement->IsFalling() || Movement->IsFlying());
	Raw.DistanceMeters = FVector::Dist(GetActorLocation(), Player->GetActorLocation()) / 100.0f;
	Raw.MoveSpeed = Player->GetVelocity().Size2D() / 100.0f;
	return Raw;
}

void AMatsuriBoss::SetTaunt(const FString& Line)
{
	if (!Line.IsEmpty())
	{
		Taunt = Line;
		TauntAge = 0.0f;
	}
}

float AMatsuriBoss::GapSeconds() const
{
	return Phase >= 3 ? 0.45f : Phase == 2 ? 0.7f : 1.0f;
}

int32 AMatsuriBoss::PickAnchor(int32 Preferred) const
{
	if (Arena.Anchors.IsValidIndex(Preferred))
	{
		return Preferred;
	}
	FVector Player;
	const bool bHasPlayer = Field && Field->GetPlayerHitSphere(Player);
	TArray<int32> Good;
	int32 Farthest = 0;
	float FarthestDistance = -1.0f;
	for (int32 i = 0; i < Arena.Anchors.Num(); ++i)
	{
		const float Distance = bHasPlayer ? FVector::Dist(Arena.Anchors[i], Player) : MinPlayerDistance;
		if (Distance > FarthestDistance)
		{
			FarthestDistance = Distance;
			Farthest = i;
		}
		if (i != Anchor && Distance >= MinPlayerDistance && Distance <= MaxPlayerDistance)
		{
			Good.Add(i);
		}
	}
	return Good.Num() > 0 ? Good[Random.RandRange(0, Good.Num() - 1)] : Farthest;
}

void AMatsuriBoss::StartNextSkill()
{
	const FMatsuriBossObservation Now = Brain->Complete(Observe());
	const FMatsuriBossAction Action = Brain->Decide(Now);
	Brain->StepStarted(Action, Now, Field->GetStats(), GetHealthFraction(), FightSeconds);

	if (Action.Skill == EMatsuriBossSkill::Phantom)
	{
		// She blinks: an after-image where she was, and she is simply there.
		Matsuri::Fx::AfterImage(GetWorld(), GetActorLocation(), Matsuri::BossSkills::Color(Action.Skill), 0.6f);
		Anchor = PickAnchor(Action.Target);
		SetActorLocation(Arena.Anchors[Anchor]);
		Matsuri::Audio::PlayAt(this, EMatsuriSfx::BossTeleport, GetActorLocation());
	}
	else
	{
		Anchor = PickAnchor(-1);
	}
	Program = Matsuri::BossSkills::Begin(Action, MakeView());
	bRunningSkill = true;
	SetTaunt(Action.Taunt);
	// Think about the next skill for the whole of this one (the live model took 3.7 s to answer).
	Brain->Prepare(Now);
	Matsuri::Audio::PlayAt(this, EMatsuriSfx::BossShot, GetActorLocation(), 0.9f);
	Matsuri::Fx::Ring(GetWorld(), GetActorLocation(), Matsuri::BossSkills::Color(Action.Skill), 100.0f, 420.0f, Matsuri::BossSkills::TelegraphSeconds);
	if (Action.Skill == EMatsuriBossSkill::FoxRain)
	{
		// The warning on the ground where the rain will fall.
		Matsuri::Fx::Ring(GetWorld(), Program.Center + FVector(0, 0, 8), Matsuri::BossSkills::Color(Action.Skill), 650.0f, 120.0f,
			Matsuri::BossSkills::TelegraphSeconds + 0.6f);
	}
	for (const FVector& Phantom : Program.Phantoms)
	{
		Matsuri::Fx::AfterImage(GetWorld(), Phantom, Matsuri::BossSkills::Color(Action.Skill), Program.Duration + 0.5f);
	}
}

void AMatsuriBoss::BeginPhase(int32 NewPhase)
{
	Phase = NewPhase;
	bShifting = true;
	bRunningSkill = false;
	StateTime = 0.0f;
	if (Field)
	{
		Field->ClearAll();
	}
	SetTaunt(PhaseLine(Phase));
	Matsuri::Audio::Play2D(this, EMatsuriSfx::BossPhase);
	Matsuri::Music::Play(this, Matsuri::Music::ForBossPhase(Phase));	// her last bar gets its own track
	Matsuri::Fx::Ring(GetWorld(), GetActorLocation(), TailColor, 150.0f, 2400.0f, 0.9f);
	Matsuri::Fx::Pillar(GetWorld(), GetActorLocation() - FVector(0, 0, 600), TailColor, 2000.0f, 1.0f);
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Tamamo enters phase %d at %.0f%%"), Phase, GetHealthFraction() * 100.0f);
}

float AMatsuriBoss::TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDefeated || bShifting || bIntro || Damage <= 0.0f)
	{
		return 0.0f;	// between bars and before the fight she cannot be hurt
	}
	const float Applied = FMath::Min(Damage, Health);
	Health -= Applied;
	HitPunch = HitPunchSeconds;
	if (UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this))
	{
		Run->AddDamageDealt(Applied);
	}
	if (Health <= 0.0f)
	{
		Defeat();
		return Applied;
	}
	const int32 NewPhase = PhaseFor(GetHealthFraction());
	if (NewPhase > Phase)
	{
		BeginPhase(NewPhase);
	}
	return Applied;
}

void AMatsuriBoss::Defeat()
{
	bDefeated = true;
	bRunningSkill = false;
	if (Field)
	{
		Field->ClearAll();
		Brain->StepFinished(Field->GetStats(), 0.0f);
	}
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	const float HealthLost = FMath::Max(0.0f, HealthAtStart - (Run ? Run->GetRunState().Health : HealthAtStart));
	Brain->EndFight(false, HealthLost, 0.0f, FightSeconds);
	bFightOpen = false;

	SetTaunt(TEXT("……見事、ね。"));
	Matsuri::Audio::Play2D(this, EMatsuriSfx::BossDefeat);
	Matsuri::Fx::Template(GetWorld(), Matsuri::Fx::ETemplate::Explosion, GetActorLocation(), FVector::UpVector, 2.5f);
	Matsuri::Fx::Burst(GetWorld(), GetActorLocation(), TailColor, 40, 900.0f, 22.0f, 1.2f);
	Matsuri::Fx::Pillar(GetWorld(), GetActorLocation() - FVector(0, 0, 800), FLinearColor::White, 3000.0f, 1.5f);
	// Returned wishes scatter into magatama; the guardian survives her trial.
	for (int32 i = 0; i < MagatamaDrops; ++i)
	{
		const FVector Drop = FVector(GetActorLocation().X, GetActorLocation().Y, Arena.GroundZ + 90.0f)
			+ FRotator(0.0f, 360.0f * i / MagatamaDrops, 0.0f).Vector() * 250.0f;
		const FTransform Transform(Drop);
		AMatsuriCollectible* Gem = GetWorld()->SpawnActorDeferred<AMatsuriCollectible>(AMatsuriCollectible::StaticClass(), Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Gem->SetKind(EMatsuriCollectible::Magatama);
		Gem->FinishSpawning(Transform);
	}
	// Keep Tamamo present for the reconciliation; her trial has ended.
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Tamamo defeated after %.0f s"), FightSeconds);
	OnDefeated.Broadcast();
}

void AMatsuriBoss::DevNextBar()
{
	if (bDefeated)
	{
		return;
	}
	bIntro = false;
	if (Phase >= 3)
	{
		DevDefeat();
		return;
	}
	Health = FMath::Min(Health, MaxHealth * PhaseThreshold(Phase) - KINDA_SMALL_NUMBER);
	BeginPhase(Phase + 1);
}

void AMatsuriBoss::DevDefeat()
{
	if (!bDefeated)
	{
		Health = 0.0f;
		Defeat();
	}
}

void AMatsuriBoss::EndFightAsWinner()
{
	if (!bFightOpen || bDefeated)
	{
		return;
	}
	bFightOpen = false;
	bRunningSkill = false;
	if (Field)
	{
		Brain->StepFinished(Field->GetStats(), GetHealthFraction());
	}
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	const float HealthLost = FMath::Max(0.0f, HealthAtStart - (Run ? Run->GetRunState().Health : HealthAtStart));
	Brain->EndFight(true, HealthLost, GetHealthFraction(), FightSeconds);
	SetTaunt(TEXT("また遊びましょう、ね？"));
}

void AMatsuriBoss::MoveAndPose(float DeltaSeconds)
{
	Clock += DeltaSeconds;
	if (Arena.Anchors.IsValidIndex(Anchor))
	{
		const FVector Goal = Arena.Anchors[Anchor] + FVector(0.0f, 0.0f, FMath::Sin(Clock * 1.6f) * BobHeight);
		SetActorLocation(FMath::VInterpConstantTo(GetActorLocation(), Goal, DeltaSeconds, GlideSpeed));
	}
	FVector Player;
	if (Field && Field->GetPlayerHitSphere(Player))
	{
		SetActorRotation(FRotator(0.0f, (Player - GetActorLocation()).Rotation().Yaw, 0.0f));
	}
	for (int32 i = 0; i < Tails.Num(); ++i)
	{
		const float Spread = FMath::Lerp(-70.0f, 70.0f, static_cast<float>(i) / FMath::Max(Tails.Num() - 1, 1));
		Tails[i]->SetRelativeRotation(FRotator(-55.0f + 8.0f * FMath::Sin(Clock * 2.2f + i), 0.0f, Spread + 6.0f * FMath::Sin(Clock * 1.7f + i * 0.7f)));
	}
	HitPunch = FMath::Max(0.0f, HitPunch - DeltaSeconds);
	Visual->SetRelativeScale3D(FVector(1.0f + 0.15f * HitPunch / HitPunchSeconds));
	if (HaloMaterial)
	{
		const bool bTelegraph = bRunningSkill && Program.Time < Matsuri::BossSkills::TelegraphSeconds;
		const FLinearColor Color = bRunningSkill ? Matsuri::BossSkills::Color(Program.Action.Skill) : TailColor;
		HaloMaterial->SetVectorParameterValue(TEXT("Color"), Color);
		HaloMaterial->SetScalarParameterValue(TEXT("Glow"), bTelegraph ? 18.0f : bShifting ? 12.0f : 3.0f);
	}
}

void AMatsuriBoss::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	MoveAndPose(DeltaSeconds);
	TauntAge += DeltaSeconds;
	if (bDefeated || !bFightOpen || !Field)
	{
		return;
	}
	FightSeconds += DeltaSeconds;
	StateTime += DeltaSeconds;

	const FMatsuriBossObservation Raw = Observe();
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const UMatsuriGrappleComponent* Grapple = PC && PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<UMatsuriGrappleComponent>() : nullptr;
	Brain->Observe(DeltaSeconds, Raw, Grapple ? Grapple->GetUses() : 0);

	if (bIntro)
	{
		if (StateTime < DeltaSeconds * 1.5f)
		{
			Brain->Prepare(Brain->Complete(Raw));	// think about the first skill during the greeting
		}
		if (StateTime >= IntroSeconds)
		{
			bIntro = false;
			StateTime = 0.0f;
			StartNextSkill();
		}
		return;
	}
	if (bShifting)
	{
		if (StateTime >= PhaseShiftSeconds)
		{
			bShifting = false;
			StateTime = 0.0f;
			StartNextSkill();
		}
		else if (StateTime < DeltaSeconds * 1.5f)
		{
			Brain->Prepare(Brain->Complete(Raw));
		}
		return;
	}
	if (bRunningSkill)
	{
		const bool bGoing = Matsuri::BossSkills::Advance(Program, DeltaSeconds, MakeView(), Random, Fired);
		Field->Spawn(Fired);
		if (!bGoing)
		{
			bRunningSkill = false;
			StateTime = 0.0f;
		}
		return;
	}
	const bool bAnswerComing = Brain->IsThinking() && StateTime < GapSeconds() + ModelGraceSeconds;
	if (StateTime >= GapSeconds() && !bAnswerComing)
	{
		StartNextSkill();
	}
}

#undef LOCTEXT_NAMESPACE
