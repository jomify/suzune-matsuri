// Suzune's Night Festival - the heroine's weapon: ammo-fed shots plus a free melee swing.

#include "MatsuriCombatComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "MatsuriArtTable.h"
#include "MatsuriAudio.h"
#include "Components/AudioComponent.h"
#include "MatsuriCombat.h"
#include "MatsuriDamageNumber.h"
#include "MatsuriFx.h"
#include "MatsuriYokai.h"
#include "MatsuriProjectile.h"
#include "MatsuriViewModel.h"
#include "MatsuriRunSubsystem.h"
#include "SuzuneMatsuri.h"

namespace
{
	constexpr int32 MappingPriority = 1;	// above the template's IMC_Default
	constexpr float AimTraceDistance = 10000.0f;
	constexpr float MinConvergeDistance = 100.0f;
	const FVector MuzzleOffset(26.0f, 12.0f, -9.0f);	// forward, right, up: the right hand (without a view model)
	constexpr double FireBufferSeconds = 0.18;			// a click this early still fires when the weapon is ready
	constexpr float QuickLooseSpeed = 0.6f;				// share of full speed for an undrawn arrow
	const FLinearColor FullDrawColor(1.0f, 0.78f, 0.25f);
	constexpr float PointBlank = 110.0f;	// this close, a yokai is hit whatever the angle
	constexpr float HomingRange = 2500.0f;
	constexpr float HomingConeDot = 0.6f;	// ~53 degrees either side of the crosshair

	// 投げ銭: a heavy gold coin.
	constexpr float ZeniDamage = 4.0f;
	constexpr float ZeniSpeed = 3500.0f;
	const FLinearColor ZeniColor(1.0f, 0.70f, 0.10f);

	EMatsuriShotLook LookFor(EMatsuriHero Hero)
	{
		switch (Hero)
		{
		case EMatsuriHero::Sakura:	return EMatsuriShotLook::Arrow;
		case EMatsuriHero::Suzune:	return EMatsuriShotLook::Star;
		default:					return EMatsuriShotLook::Orb;
		}
	}
}

UMatsuriCombatComponent::UMatsuriCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;	// only to release a buffered click
}

void UMatsuriCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (BufferedFireUntil < GetWorld()->GetTimeSeconds())
	{
		BufferedFireUntil = -1.0;
		return;
	}
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	if (Run && IsReady(LastFireTime, Run->GetRunState().GetHero().Weapon.FireInterval))
	{
		BufferedFireUntil = -1.0;	// one attempt: an empty weapon dry-fires once, not every frame
		TryFire();
	}
}

void UMatsuriCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	BindInput();
}

void UMatsuriCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindInput();
	Super::EndPlay(EndPlayReason);
}

void UMatsuriCombatComponent::BindInput()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UEnhancedInputComponent* Input = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!Input || !Subsystem)
	{
		UE_LOG(LogSuzuneMatsuri, Warning, TEXT("Combat input not bound: owner is not a locally controlled player"));
		return;
	}

	FireAction = NewObject<UInputAction>(this, TEXT("IA_MatsuriFire"));
	MeleeAction = NewObject<UInputAction>(this, TEXT("IA_MatsuriMelee"));
	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_MatsuriCombat"));
	Mapping->MapKey(FireAction, EKeys::LeftMouseButton);
	Mapping->MapKey(FireAction, EKeys::Gamepad_RightTrigger);
	Mapping->MapKey(MeleeAction, EKeys::V);
	Mapping->MapKey(MeleeAction, EKeys::Gamepad_RightShoulder);
	Subsystem->AddMappingContext(Mapping, MappingPriority);

	BindingHandles.Add(Input->BindAction(FireAction, ETriggerEvent::Started, this, &UMatsuriCombatComponent::HandleFirePressed).GetHandle());
	BindingHandles.Add(Input->BindAction(FireAction, ETriggerEvent::Triggered, this, &UMatsuriCombatComponent::HandleFireHeld).GetHandle());
	BindingHandles.Add(Input->BindAction(FireAction, ETriggerEvent::Completed, this, &UMatsuriCombatComponent::HandleFireReleased).GetHandle());
	BindingHandles.Add(Input->BindAction(MeleeAction, ETriggerEvent::Started, this, &UMatsuriCombatComponent::HandleMeleeInput).GetHandle());
}

void UMatsuriCombatComponent::UnbindInput()
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

void UMatsuriCombatComponent::HandleFirePressed()
{
	if (bUltimateShowcaseLocked)
	{
		return;
	}
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	if (Run && Run->GetRunState().GetHero().Weapon.MaxChargeSeconds > 0.0f)
	{
		bCharging = true;
		ChargeStartTime = GetWorld()->GetTimeSeconds();
		SlowForDraw(true);
		DrawSound = Matsuri::Audio::Spawn2D(this, EMatsuriSfx::BowDraw, 0.8f);
		return;
	}
	// The hold handler fires on this same frame when the weapon is ready; a click during the cooldown
	// is remembered briefly and fires the moment it is ready, instead of being lost.
	if (Run && !IsReady(LastFireTime, Run->GetRunState().GetHero().Weapon.FireInterval))
	{
		BufferedFireUntil = GetWorld()->GetTimeSeconds() + FireBufferSeconds;
	}
}

void UMatsuriCombatComponent::HandleFireHeld()
{
	if (bUltimateShowcaseLocked)
	{
		return;
	}
	if (!bCharging)
	{
		TryFire();	// automatic weapons fire while held, gated by their interval
	}
}

void UMatsuriCombatComponent::HandleFireReleased()
{
	if (bCharging)
	{
		const float Alpha = GetChargeAlpha();
		bCharging = false;
		SlowForDraw(false);
		if (UAudioComponent* Drawing = DrawSound.Get())
		{
			Drawing->FadeOut(DrawSoundFadeSeconds, 0.0f);	// an early release cuts the rising creak off
		}
		DrawSound.Reset();
		if (!bUltimateShowcaseLocked)
		{
			TryFire(Alpha);
		}
	}
}

void UMatsuriCombatComponent::SetUltimateShowcaseLock(bool bLocked)
{
	bUltimateShowcaseLocked = bLocked;
	BufferedFireUntil = -1.0;
	if (bLocked && bCharging)
	{
		bCharging = false;
		SlowForDraw(false);
		if (UAudioComponent* Drawing = DrawSound.Get())
		{
			Drawing->FadeOut(DrawSoundFadeSeconds, 0.0f);
		}
		DrawSound.Reset();
	}
}

void UMatsuriCombatComponent::SlowForDraw(bool bDrawing)
{
	const ACharacter* Hero = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Hero ? Hero->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}
	if (bDrawing && WalkSpeedBeforeDraw <= 0.0f)
	{
		WalkSpeedBeforeDraw = Movement->MaxWalkSpeed;
		Movement->MaxWalkSpeed = WalkSpeedBeforeDraw * DrawWalkScale;
	}
	else if (!bDrawing && WalkSpeedBeforeDraw > 0.0f)
	{
		Movement->MaxWalkSpeed = WalkSpeedBeforeDraw;
		WalkSpeedBeforeDraw = 0.0f;
	}
}

float UMatsuriCombatComponent::GetChargeAlpha() const
{
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	const float MaxCharge = Run ? Run->GetRunState().GetHero().Weapon.MaxChargeSeconds : 0.0f;
	if (!bCharging || MaxCharge <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - ChargeStartTime) / MaxCharge, 0.0f, 1.0f);
}

void UMatsuriCombatComponent::ActivateShadowStitch(float Seconds)
{
	ShadowStitchUntil = GetWorld()->GetTimeSeconds() + Seconds;
}

bool UMatsuriCombatComponent::IsShadowStitchActive() const
{
	return GetWorld()->GetTimeSeconds() < ShadowStitchUntil;
}

bool UMatsuriCombatComponent::IsReady(double LastTime, float BaseInterval) const
{
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	const float Scale = Run ? Run->GetRunState().Modifiers.AttackIntervalMultiplier : 1.0f;
	return GetWorld()->GetTimeSeconds() - LastTime >= BaseInterval * Scale;
}

float UMatsuriCombatComponent::ScaledDamage(float Base) const
{
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	return Base * (Run ? Run->GetRunState().Modifiers.DamageMultiplier : 1.0f);
}

bool UMatsuriCombatComponent::GetView(FVector& OutLocation, FVector& OutDirection) const
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

FVector UMatsuriCombatComponent::GetMuzzle(const FVector& ViewLocation, const FVector& ViewDirection) const
{
	// Shots leave from the tip of the weapon in view, so what you see is where they come from.
	if (const UMatsuriViewModelComponent* ViewModel = GetOwner()->FindComponentByClass<UMatsuriViewModelComponent>())
	{
		return ViewModel->GetMuzzleLocation();
	}
	const FRotator View = ViewDirection.Rotation();
	return ViewLocation + View.RotateVector(MuzzleOffset);
}

FVector UMatsuriCombatComponent::GetAimPoint(const FVector& ViewLocation, const FVector& ViewDirection) const
{
	// Aim where the crosshair lands so shots converge on the target, not parallel to the view.
	FHitResult AimHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MatsuriAim), false, GetOwner());
	const FVector AimEnd = ViewLocation + ViewDirection * AimTraceDistance;
	return GetWorld()->LineTraceSingleByChannel(AimHit, ViewLocation, AimEnd, ECC_Visibility, Params) ? AimHit.ImpactPoint : AimEnd;
}

USceneComponent* UMatsuriCombatComponent::FindHomingTarget(const FVector& ViewLocation, const FVector& ViewDirection) const
{
	USceneComponent* Best = nullptr;
	float BestDistanceSq = FMath::Square(HomingRange);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(Matsuri::Combat::YokaiTag))
		{
			continue;
		}
		const FVector ToYokai = It->GetActorLocation() - ViewLocation;
		const float DistanceSq = ToYokai.SizeSquared();
		if (DistanceSq < BestDistanceSq && FVector::DotProduct(ToYokai.GetSafeNormal(), ViewDirection) > HomingConeDot)
		{
			Best = It->GetRootComponent();
			BestDistanceSq = DistanceSq;
		}
	}
	return Best;
}

void UMatsuriCombatComponent::SpawnShot(const FMatsuriShot& Shot, const FVector& Muzzle)
{
	APawn* Pawn = CastChecked<APawn>(GetOwner());
	const FTransform SpawnTransform(Shot.Direction.Rotation(), Muzzle);
	AMatsuriProjectile* Projectile = GetWorld()->SpawnActorDeferred<AMatsuriProjectile>(AMatsuriProjectile::StaticClass(), SpawnTransform,
		Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Projectile->Configure(Shot);
	Projectile->FinishSpawning(SpawnTransform);
}

bool UMatsuriCombatComponent::TryFire(float ChargeAlpha)
{
	if (bUltimateShowcaseLocked)
	{
		return false;
	}
	UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	FVector ViewLocation, ViewDirection;
	if (!Run || !GetView(ViewLocation, ViewDirection))
	{
		return false;
	}
	const FMatsuriWeaponDef& Weapon = Run->GetRunState().GetHero().Weapon;
	if (!IsReady(LastFireTime, Weapon.FireInterval))
	{
		return false;
	}
	LastFireTime = GetWorld()->GetTimeSeconds();

	const bool bStitched = IsShadowStitchActive();
	if (!bStitched && !Run->ConsumeAmmo(1))
	{
		OnDryFire.Broadcast();
		Matsuri::Audio::Play2D(this, EMatsuriSfx::DryFire, 0.6f);
		return false;
	}

	const FVector Muzzle = GetMuzzle(ViewLocation, ViewDirection);
	const FVector AimPoint = GetAimPoint(ViewLocation, ViewDirection);
	// Point-blank (wall in the face): converging on the hit point would send the shot sideways or back.
	const FVector Launch = FVector::DotProduct(AimPoint - Muzzle, ViewDirection) > MinConvergeDistance ? (AimPoint - Muzzle).GetSafeNormal() : ViewDirection;

	FMatsuriShot Shot;
	// A fuller draw flies faster (a quick loose drops sooner) and hits harder.
	const bool bChargeWeapon = Weapon.MaxChargeSeconds > 0.0f;
	const float Draw = FMath::Clamp(ChargeAlpha, 0.0f, 1.0f);
	Shot.Speed = bChargeWeapon ? Weapon.Speed * FMath::Lerp(QuickLooseSpeed, 1.0f, Draw) : Weapon.Speed;
	Shot.Damage = ScaledDamage(FMath::Lerp(Weapon.Damage, Weapon.ChargedDamage, FMath::Clamp(ChargeAlpha, 0.0f, 1.0f)));
	Shot.Pierce = Weapon.Pierce;
	Shot.Bounces = Weapon.Bounces;
	Shot.Color = Weapon.Color;
	Shot.Scale = Weapon.ShotScale * (1.0f + ChargeAlpha * 0.6f);
	Shot.Look = LookFor(Run->GetRunState().Hero);
	Shot.Art = Matsuri::ArtTable::Shot(Run->GetRunState().Hero);
	Shot.HomingTarget = bStitched ? FindHomingTarget(ViewLocation, ViewDirection) : nullptr;
	if (bStitched)
	{
		Shot.Color = FLinearColor(0.65f, 0.30f, 1.0f);	// 影縫い: shadow-violet, homing
	}
	else if (bChargeWeapon && Draw >= 1.0f)
	{
		Shot.Color = FullDrawColor;	// a full draw: the arrow burns gold
	}

	// A fan spreads the shots evenly around the aim, e.g. -5, 0, +5 degrees for three shuriken.
	for (int32 i = 0; i < Weapon.ShotsPerTrigger; ++i)
	{
		const float Yaw = (i - (Weapon.ShotsPerTrigger - 1) * 0.5f) * Weapon.FanDegrees;
		Shot.Direction = Launch.RotateAngleAxis(Yaw, FVector::UpVector);
		SpawnShot(Shot, Muzzle);
	}
	BufferedFireUntil = -1.0;
	switch (Run->GetRunState().Hero)
	{
	case EMatsuriHero::Sakura:
		Matsuri::Audio::Play2D(this, EMatsuriSfx::BowRelease);
		Matsuri::Audio::Play2D(this, EMatsuriSfx::ArrowWhoosh, 0.35f + 0.4f * FMath::Clamp(ChargeAlpha, 0.0f, 1.0f));
		break;
	case EMatsuriHero::Suzune:
		Matsuri::Audio::Play2D(this, EMatsuriSfx::KunaiThrow, 0.8f);
		break;
	default:
		Matsuri::Audio::Play2D(this, EMatsuriSfx::BellShot, 0.7f);
		break;
	}
	OnShot.Broadcast();
	OnAttack.Broadcast();
	return true;
}

void UMatsuriCombatComponent::ThrowZeni()
{
	FVector ViewLocation, ViewDirection;
	if (!GetView(ViewLocation, ViewDirection))
	{
		return;
	}
	const FVector Muzzle = GetMuzzle(ViewLocation, ViewDirection);
	FMatsuriShot Shot;
	Shot.Direction = (GetAimPoint(ViewLocation, ViewDirection) - Muzzle).GetSafeNormal();
	Shot.Speed = ZeniSpeed;
	Shot.Damage = ScaledDamage(ZeniDamage);
	Shot.Pierce = 1;
	Shot.Color = ZeniColor;
	Shot.Scale = 0.28f;
	Shot.Art = Matsuri::ArtTable::ZeniCoin();
	Shot.Look = EMatsuriShotLook::Coin;
	Matsuri::Audio::Play2D(this, EMatsuriSfx::ZeniThrow);
	SpawnShot(Shot, Muzzle);
	OnShot.Broadcast();
	OnAttack.Broadcast();
}

bool UMatsuriCombatComponent::InSwing(const FVector& Eye, const FVector& Facing2D, const FVector& Point, float Slack) const
{
	const FVector To = Point - Eye;
	const float Distance = To.Size2D();
	if (Distance > MeleeReach + Slack || FMath::Abs(To.Z) > MeleeReach)
	{
		return false;
	}
	return Distance < PointBlank || FVector::DotProduct(To.GetSafeNormal2D(), Facing2D) >= FMath::Cos(FMath::DegreesToRadians(MeleeHalfAngle));
}

bool UMatsuriCombatComponent::TryMelee()
{
	if (bUltimateShowcaseLocked)
	{
		return false;
	}
	FVector Eye, ViewDirection;
	if (!IsReady(LastMeleeTime, MeleeInterval) || !GetView(Eye, ViewDirection))
	{
		return false;
	}
	LastMeleeTime = GetWorld()->GetTimeSeconds();

	APawn* Pawn = CastChecked<APawn>(GetOwner());
	const FVector Facing2D = ViewDirection.GetSafeNormal2D();
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	const FLinearColor SwingColor = Run ? Run->GetRunState().GetHero().Color : FLinearColor::White;
	Matsuri::Fx::SwingArc(GetWorld(), Eye, Facing2D, SwingColor, 140.0f, MeleeHalfAngle);
	OnSwing.Broadcast();
	Matsuri::Audio::Play2D(this, EMatsuriSfx::MeleeSwing, 0.8f);

	// Yokai in the arc: damage, then shove and stagger the survivors.
	TArray<AMatsuriYokai*> Struck;
	for (TActorIterator<AMatsuriYokai> It(GetWorld()); It; ++It)
	{
		if (InSwing(Eye, Facing2D, It->GetActorLocation(), It->GetSimpleCollisionRadius()))
		{
			Struck.Add(*It);
		}
	}
	for (AMatsuriYokai* Yokai : Struck)
	{
		const FVector Away = (Yokai->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D();
		Matsuri::Fx::Burst(GetWorld(), Yokai->GetActorLocation(), SwingColor, 8, 450.0f, 11.0f, 0.3f, Away);
		const FVector HitAt = Yokai->GetActorLocation();
		UGameplayStatics::ApplyDamage(Yokai, ScaledDamage(MeleeDamage), Pawn->GetController(), Pawn, nullptr);
		AMatsuriDamageNumber::Pop(GetWorld(), HitAt, ScaledDamage(MeleeDamage), false, !IsValid(Yokai) || Yokai->GetHealth() <= 0.0f);
		Matsuri::Audio::PlayAt(this, EMatsuriSfx::MeleeHit, HitAt);
		ConfirmHit();
		if (IsValid(Yokai))
		{
			Yokai->Knockback(Away * MeleeKnockback);
			Yokai->Stun(MeleeStun);
		}
	}

	// Swat incoming cucumbers out of the air.
	TArray<AMatsuriProjectile*> Swatted;
	for (TActorIterator<AMatsuriProjectile> It(GetWorld()); It; ++It)
	{
		if (It->IsHostileToPlayer() && InSwing(Eye, Facing2D, It->GetActorLocation(), 60.0f))
		{
			Swatted.Add(*It);
		}
	}
	for (AMatsuriProjectile* Shot : Swatted)
	{
		Shot->Deflect();
	}
	if (Swatted.Num() > 0)
	{
		OnDeflect.Broadcast();
	}
	OnAttack.Broadcast();
	return true;
}
