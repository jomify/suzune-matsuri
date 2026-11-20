// Suzune's Night Festival - Shift / E / Q skills of the three heroines.

#include "MatsuriSkillComponent.h"
#include "MatsuriCinematicWidget.h"
#include "Components/CapsuleComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "MatsuriAmmoPickup.h"
#include "MatsuriCollectible.h"
#include "MatsuriCombatComponent.h"
#include "MatsuriBossGameMode.h"
#include "MatsuriDanmakuField.h"
#include "MatsuriDecoy.h"
#include "MatsuriFx.h"
#include "MatsuriFloorGameMode.h"
#include "MatsuriRunSubsystem.h"
#include "MatsuriYokai.h"
#include "SuzuneMatsuri.h"

namespace
{
	constexpr int32 MappingPriority = 1;

	// Sakura
	constexpr float BarrierSeconds = 2.0f;
	constexpr float BarrierRadius = 450.0f;
	constexpr float BarrierPush = 500.0f;
	constexpr float BarrierStun = 0.6f;
	constexpr float PurifyRange = 700.0f;
	constexpr float PurifyConeDot = 0.82f;	// ~35 degrees either side
	constexpr float PurifyStun = 3.0f;
	constexpr float PurgeRadius = 1500.0f;
	constexpr float PurgeDamage = 99.0f;

	// Suzune
	constexpr float BlinkDistance = 600.0f;
	constexpr float ShadowStitchSeconds = 6.0f;

	// Mio
	constexpr float CatRadius = 600.0f;
	constexpr float CatSeconds = 8.0f;
	constexpr float CatPullSpeed = 1200.0f;
	constexpr float FullHouseRadius = 3000.0f;
	constexpr float FullHouseSeconds = 1.5f;
	constexpr float FullHousePullSpeed = 3500.0f;
	constexpr float DoubleDropSeconds = 8.0f;

	const FLinearColor SakuraGold(1.0f, 0.85f, 0.35f);
	const FLinearColor PurifyWhite(0.95f, 0.95f, 1.0f);
	const FLinearColor SuzuneBlue(0.35f, 0.55f, 1.0f);
	const FLinearColor ShadowViolet(0.65f, 0.30f, 1.0f);
	const FLinearColor MioGold(1.0f, 0.75f, 0.2f);

	FVector Feet(const AActor& Actor)
	{
		return Actor.GetActorLocation() - FVector(0.0f, 0.0f, 85.0f);
	}

	template <typename TActor, typename TFunc>
	void ForEachWithin(UWorld* World, const FVector& Center, float Radius, TFunc&& Func)
	{
		for (TActorIterator<TActor> It(World); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Center) <= Radius)
			{
				Func(**It);
			}
		}
	}
}

UMatsuriSkillComponent::UMatsuriSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMatsuriSkillComponent::BeginPlay()
{
	Super::BeginPlay();
	BindInput();
}

void UMatsuriSkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bUltimateShowcaseLocked = false;
	if (UMatsuriCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UMatsuriCombatComponent>() : nullptr)
	{
		Combat->SetUltimateShowcaseLock(false);
	}
	UnbindInput();
	Super::EndPlay(EndPlayReason);
}

void UMatsuriSkillComponent::BindInput()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UEnhancedInputComponent* Input = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!Input || !Subsystem)
	{
		UE_LOG(LogSuzuneMatsuri, Warning, TEXT("Skill input not bound: owner is not a locally controlled player"));
		return;
	}

	ShiftAction = NewObject<UInputAction>(this, TEXT("IA_MatsuriShift"));
	EAction = NewObject<UInputAction>(this, TEXT("IA_MatsuriE"));
	QAction = NewObject<UInputAction>(this, TEXT("IA_MatsuriQ"));
	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_MatsuriSkills"));
	Mapping->MapKey(ShiftAction, EKeys::LeftShift);
	Mapping->MapKey(ShiftAction, EKeys::Gamepad_LeftShoulder);
	Mapping->MapKey(EAction, EKeys::E);
	Mapping->MapKey(EAction, EKeys::Gamepad_FaceButton_Left);
	Mapping->MapKey(QAction, EKeys::Q);
	Mapping->MapKey(QAction, EKeys::Gamepad_FaceButton_Top);
	Subsystem->AddMappingContext(Mapping, MappingPriority);

	BindingHandles.Add(Input->BindAction(ShiftAction, ETriggerEvent::Started, this, &UMatsuriSkillComponent::HandleShift).GetHandle());
	BindingHandles.Add(Input->BindAction(EAction, ETriggerEvent::Started, this, &UMatsuriSkillComponent::HandleE).GetHandle());
	BindingHandles.Add(Input->BindAction(QAction, ETriggerEvent::Started, this, &UMatsuriSkillComponent::HandleQ).GetHandle());
}

void UMatsuriSkillComponent::UnbindInput()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PC->InputComponent))
	{
		for (const uint32 Handle : BindingHandles)
		{
			Input->RemoveBindingByHandle(Handle);
		}
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->RemoveMappingContext(Mapping);
	}
	BindingHandles.Reset();
}

float UMatsuriSkillComponent::GetCooldownRemaining(EMatsuriSkillSlot Slot) const
{
	return FMath::Max(0.0f, static_cast<float>(ReadyAt[static_cast<int32>(Slot)] - GetWorld()->GetTimeSeconds()));
}

bool UMatsuriSkillComponent::TryUseSkill(EMatsuriSkillSlot Slot)
{
	UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	const AMatsuriFloorGameMode* GameMode = GetWorld()->GetAuthGameMode<AMatsuriFloorGameMode>();
	if (!Run || !GameMode || !GameMode->IsFloorActive())
	{
		return false;
	}
	const bool bUltimate = Slot == EMatsuriSkillSlot::Q;
	if (bUltimateShowcaseLocked)
	{
		return false;
	}
	if (bUltimate ? !Run->GetRunState().IsUltReady() : GetCooldownRemaining(Slot) > 0.0f)
	{
		return false;
	}

	const EMatsuriHero Hero = Run->GetRunState().Hero;
	if (!Perform(Hero, Slot))
	{
		return false;
	}
	if (bUltimate)
	{
		Run->ConsumeUlt();
		if (UMatsuriCinematicWidget::IsUltimateEnabled())
		{
			AMatsuriFloorGameMode* Floor = GetWorld()->GetAuthGameMode<AMatsuriFloorGameMode>();
			APlayerController* PC = GetWorld()->GetFirstPlayerController();
			if (Floor && Floor->IsFloorActive() && PC)
			{
				if (UMatsuriCombatComponent* Combat = GetOwner()->FindComponentByClass<UMatsuriCombatComponent>())
				{
					Combat->SetUltimateShowcaseLock(true);
				}
				SetUltimateShowcaseLock(true);
				const EMatsuriCinematic Cinematic = Hero == EMatsuriHero::Sakura ? EMatsuriCinematic::UltimateSakura
					: Hero == EMatsuriHero::Suzune ? EMatsuriCinematic::UltimateSuzune : EMatsuriCinematic::UltimateMio;
				const FSimpleDelegate Unlock = FSimpleDelegate::CreateWeakLambda(this, [this]()
				{
					bUltimateShowcaseLocked = false;
					if (UMatsuriCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UMatsuriCombatComponent>() : nullptr)
					{
						Combat->SetUltimateShowcaseLock(false);
					}
				});
				if (!UMatsuriCinematicWidget::TryPlay(*PC, Cinematic, Unlock, 2.5f))
				{
					Unlock.ExecuteIfBound();
				}
			}
		}
	}
	else
	{
		ReadyAt[static_cast<int32>(Slot)] = GetWorld()->GetTimeSeconds() + Matsuri::Heroes::Get(Hero).Skill(Slot).Cooldown;
	}
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("%s used %s"), *Matsuri::Heroes::Get(Hero).Name.ToString(), *Matsuri::Heroes::Get(Hero).Skill(Slot).Name.ToString());
	OnSkillUsed.Broadcast(Slot);
	return true;
}

bool UMatsuriSkillComponent::Perform(EMatsuriHero Hero, EMatsuriSkillSlot Slot)
{
	switch (Hero)
	{
	case EMatsuriHero::Sakura:
		return Slot == EMatsuriSkillSlot::Shift ? SakuraBarrier() : Slot == EMatsuriSkillSlot::E ? SakuraPurifyCone() : SakuraGreatPurge();
	case EMatsuriHero::Suzune:
		return Slot == EMatsuriSkillSlot::Shift ? SuzuneBlink() : Slot == EMatsuriSkillSlot::E ? SuzuneDecoy() : SuzuneShadowStitch();
	case EMatsuriHero::Mio:
		return Slot == EMatsuriSkillSlot::Shift ? MioBeckoningCat() : Slot == EMatsuriSkillSlot::E ? MioZeni() : MioFullHouse();
	case EMatsuriHero::Count:
		break;
	}
	return false;
}

bool UMatsuriSkillComponent::GetView(FVector& OutLocation, FVector& OutDirection) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return false;
	}
	FRotator Rotation;
	Controller->GetPlayerViewPoint(OutLocation, Rotation);
	OutDirection = Rotation.Vector();
	return true;
}

// ---------------------------------------------------------------- Sakura

bool UMatsuriSkillComponent::SakuraBarrier()
{
	AMatsuriFloorGameMode* GameMode = GetWorld()->GetAuthGameMode<AMatsuriFloorGameMode>();
	GameMode->GrantInvulnerability(BarrierSeconds);
	const FVector Center = GetOwner()->GetActorLocation();
	Matsuri::Fx::Dome(GetWorld(), Center, SakuraGold, 160.0f, BarrierSeconds, GetOwner());
	Matsuri::Fx::Ring(GetWorld(), Feet(*GetOwner()), SakuraGold, 60.0f, BarrierRadius, 0.35f);
	Matsuri::Fx::Ring(GetWorld(), Feet(*GetOwner()), SakuraGold, 120.0f, 120.0f, BarrierSeconds, GetOwner(), 120.0f);
	ForEachWithin<AMatsuriYokai>(GetWorld(), Center, BarrierRadius, [&Center](AMatsuriYokai& Yokai)
	{
		Yokai.Knockback((Yokai.GetActorLocation() - Center).GetSafeNormal2D() * BarrierPush);
		Yokai.Stun(BarrierStun);
	});
	return true;
}

bool UMatsuriSkillComponent::SakuraPurifyCone()
{
	FVector Eye, Facing;
	if (!GetView(Eye, Facing))
	{
		return false;
	}
	const FVector Facing2D = Facing.GetSafeNormal2D();
	// A flurry of paper charms thrown in a cone.
	Matsuri::Fx::Burst(GetWorld(), Eye + Facing2D * 60.0f - FVector(0, 0, 25), PurifyWhite, 18, 1400.0f, 12.0f, 0.45f, Facing2D);
	ForEachWithin<AMatsuriYokai>(GetWorld(), Eye, PurifyRange, [&Eye, &Facing2D](AMatsuriYokai& Yokai)
	{
		if (FVector::DotProduct((Yokai.GetActorLocation() - Eye).GetSafeNormal2D(), Facing2D) > PurifyConeDot)
		{
			Yokai.Stun(PurifyStun);
		}
	});
	return true;
}

bool UMatsuriSkillComponent::SakuraGreatPurge()
{
	APawn* Pawn = CastChecked<APawn>(GetOwner());
	TArray<AMatsuriYokai*> Targets;
	ForEachWithin<AMatsuriYokai>(GetWorld(), Pawn->GetActorLocation(), PurgeRadius, [&Targets](AMatsuriYokai& Yokai) { Targets.Add(&Yokai); });
	Matsuri::Fx::Ring(GetWorld(), Feet(*Pawn), PurifyWhite, 100.0f, PurgeRadius, 0.8f);
	Matsuri::Fx::Pillar(GetWorld(), Feet(*Pawn), SakuraGold, 900.0f, 0.9f);
	for (AMatsuriYokai* Yokai : Targets)
	{
		Matsuri::Fx::Pillar(GetWorld(), Feet(*Yokai), PurifyWhite, 600.0f, 0.7f);
		UGameplayStatics::ApplyDamage(Yokai, PurgeDamage, Pawn->GetController(), Pawn, nullptr);
	}
	return true;
}

// ---------------------------------------------------------------- Suzune

bool UMatsuriSkillComponent::SuzuneBlink()
{
	ACharacter* Hero = Cast<ACharacter>(GetOwner());
	FVector Eye, Facing;
	if (!Hero || !GetView(Eye, Facing))
	{
		return false;
	}
	// Sweep the capsule along the floor: the blink stops at the first wall instead of passing it.
	const UCapsuleComponent* Capsule = Hero->GetCapsuleComponent();
	const FVector Start = Hero->GetActorLocation();
	const FVector Direction = Facing.GetSafeNormal2D();
	const FVector End = Start + Direction * BlinkDistance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MatsuriBlink), false, Hero);
	const bool bBlocked = GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params);
	const FVector Destination = bBlocked ? Hit.Location - Direction * 2.0f : End;
	Matsuri::Fx::AfterImage(GetWorld(), Start - FVector(0, 0, 40.0f), SuzuneBlue, 0.6f);
	Matsuri::Fx::Streak(GetWorld(), Start, Destination, SuzuneBlue, 0.3f);
	Hero->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
	if (const AMatsuriBossGameMode* BossMode = GetWorld()->GetAuthGameMode<AMatsuriBossGameMode>())
	{
		if (AMatsuriDanmakuField* Field = BossMode->GetField())
		{
			Field->ResetPlayerSweep();
		}
	}
	return true;
}

bool UMatsuriSkillComponent::SuzuneDecoy()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Matsuri::Fx::Smoke(GetWorld(), Feet(*GetOwner()) + FVector(0, 0, 60.0f), 150.0f);
	return GetWorld()->SpawnActor<AMatsuriDecoy>(GetOwner()->GetActorLocation(), GetOwner()->GetActorRotation(), Params) != nullptr;
}

bool UMatsuriSkillComponent::SuzuneShadowStitch()
{
	UMatsuriCombatComponent* Combat = GetOwner()->FindComponentByClass<UMatsuriCombatComponent>();
	if (!Combat)
	{
		return false;
	}
	Combat->ActivateShadowStitch(ShadowStitchSeconds);
	Matsuri::Fx::Ring(GetWorld(), Feet(*GetOwner()), ShadowViolet, 60.0f, 600.0f, 0.4f);
	Matsuri::Fx::Ring(GetWorld(), Feet(*GetOwner()), ShadowViolet, 90.0f, 90.0f, ShadowStitchSeconds, GetOwner(), -200.0f);
	return true;
}

// ---------------------------------------------------------------- Mio

bool UMatsuriSkillComponent::MioBeckoningCat()
{
	StartMagnet(CatRadius, CatSeconds, CatPullSpeed);
	Matsuri::Fx::Ring(GetWorld(), Feet(*GetOwner()), MioGold, CatRadius, CatRadius, CatSeconds, GetOwner(), 60.0f);
	Matsuri::Fx::Burst(GetWorld(), GetOwner()->GetActorLocation(), MioGold, 14, 500.0f, 10.0f, 0.5f);
	return true;
}

bool UMatsuriSkillComponent::MioZeni()
{
	UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	UMatsuriCombatComponent* Combat = GetOwner()->FindComponentByClass<UMatsuriCombatComponent>();
	if (!Combat || !Run->SpendScore(Matsuri::Run::ZeniCost))
	{
		return false;	// not enough score: nothing thrown, no cooldown
	}
	Combat->ThrowZeni();
	Matsuri::Fx::Burst(GetWorld(), GetOwner()->GetActorLocation() + FVector(0, 0, 40.0f), MioGold, 12, 600.0f, 10.0f, 0.4f);
	return true;
}

bool UMatsuriSkillComponent::MioFullHouse()
{
	StartMagnet(FullHouseRadius, FullHouseSeconds, FullHousePullSpeed);
	GetWorld()->GetAuthGameMode<AMatsuriFloorGameMode>()->SetDoubleDrops(DoubleDropSeconds);
	Matsuri::Fx::Ring(GetWorld(), Feet(*GetOwner()), MioGold, 100.0f, FullHouseRadius, 1.0f);
	Matsuri::Fx::Pillar(GetWorld(), Feet(*GetOwner()), MioGold, 900.0f, 1.0f);
	Matsuri::Fx::Burst(GetWorld(), GetOwner()->GetActorLocation(), MioGold, 24, 700.0f, 12.0f, 0.7f);
	return true;
}

void UMatsuriSkillComponent::StartMagnet(float Radius, float Seconds, float PullSpeed)
{
	MagnetRadius = Radius;
	MagnetSpeed = PullSpeed;
	MagnetUntil = GetWorld()->GetTimeSeconds() + Seconds;
}

bool UMatsuriSkillComponent::IsMagnetActive() const
{
	return GetWorld()->GetTimeSeconds() < MagnetUntil;
}

void UMatsuriSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (IsMagnetActive())
	{
		TickMagnet(DeltaTime);
	}
}

void UMatsuriSkillComponent::TickMagnet(float DeltaTime)
{
	const FVector Target = GetOwner()->GetActorLocation();
	const float Step = MagnetSpeed * DeltaTime;
	auto Pull = [&Target, Step](AActor& Loot)
	{
		const FVector ToHero = Target - Loot.GetActorLocation();
		Loot.SetActorLocation(Loot.GetActorLocation() + ToHero.GetClampedToMaxSize(Step));	// overlap on arrival collects it
	};
	ForEachWithin<AMatsuriCollectible>(GetWorld(), Target, MagnetRadius, Pull);
	ForEachWithin<AMatsuriAmmoPickup>(GetWorld(), Target, MagnetRadius, [&Pull](AMatsuriAmmoPickup& Ammo)
	{
		if (Ammo.IsAvailable())
		{
			Pull(Ammo);
		}
	});
}
