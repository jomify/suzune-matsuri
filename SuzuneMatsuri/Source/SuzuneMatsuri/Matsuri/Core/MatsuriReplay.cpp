// Suzune's Night Festival - bounded death-replay capture and playback.

#include "MatsuriReplay.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/SkeletalMeshComponent.h"
#include "MatsuriToon.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "MatsuriArt.h"
#include "MatsuriBoss.h"
#include "MatsuriCombat.h"
#include "MatsuriDanmakuField.h"
#include "MatsuriFloorGameMode.h"
#include "MatsuriGraybox.h"
#include "MatsuriProjectile.h"
#include "MatsuriYokai.h"
#include "MatsuriReplayWidget.h"
#include "MatsuriRunSubsystem.h"
#include "MatsuriUIStyle.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<bool> CVarReplay(
		TEXT("matsuri.Replay"), true,
		TEXT("Play a short death replay before game over. 0 = disable the replay."));
	constexpr float SamplePeriod = 1.0f / 15.0f;
	constexpr float ReplayZOrder = 200.0f;
}

void FMatsuriReplayBuffer::Reset()
{
	Frames.Reset();
}

void FMatsuriReplayBuffer::SetMaxFrames(int32 InCapacity)
{
	Capacity = FMath::Max(1, InCapacity);
	while (Frames.Num() > Capacity || (Frames.Num() > 1 && Frames.Last().Time - Frames[0].Time > 6.0f))
	{
		Frames.RemoveAt(0, 1, EAllowShrinking::No);
	}
}

void FMatsuriReplayBuffer::Add(FMatsuriReplayFrame Frame)
{
	if (Frames.Num() > 0 && Frame.Time < Frames.Last().Time)
	{
		return;
	}
	Frames.Add(MoveTemp(Frame));
	while (Frames.Num() > Capacity || (Frames.Num() > 1 && Frames.Last().Time - Frames[0].Time > 6.0f))
	{
		Frames.RemoveAt(0, 1, EAllowShrinking::No);
	}
}

float FMatsuriReplayBuffer::Duration() const
{
	return Frames.Num() > 1 ? FMath::Max(0.0f, Frames.Last().Time - Frames[0].Time) : 0.0f;
}

const FMatsuriReplayFrame* FMatsuriReplayBuffer::At(int32 Index) const
{
	return Frames.IsValidIndex(Index) ? &Frames[Index] : nullptr;
}

const FMatsuriReplayFrame* FMatsuriReplayBuffer::Sample(float Time) const
{
	if (Frames.Num() == 0)
	{
		return nullptr;
	}
	if (Time <= Frames[0].Time)
	{
		return &Frames[0];
	}
	for (int32 Index = 1; Index < Frames.Num(); ++Index)
	{
		if (Time <= Frames[Index].Time)
		{
			const float Before = FMath::Abs(Time - Frames[Index - 1].Time);
			const float After = FMath::Abs(Frames[Index].Time - Time);
			return Before <= After ? &Frames[Index - 1] : &Frames[Index];
		}
	}
	return &Frames.Last();
}

AMatsuriReplayDirector::AMatsuriReplayDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
	SetActorEnableCollision(false);
}

bool AMatsuriReplayDirector::IsEnabled()
{
	return CVarReplay.GetValueOnGameThread();
}

void AMatsuriReplayDirector::BeginPlay()
{
	Super::BeginPlay();
	bRecording = true;
	SampleAccumulator = SamplePeriod;
}

void AMatsuriReplayDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const AMatsuriFloorGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AMatsuriFloorGameMode>() : nullptr;
	if (bReplaying || !bRecording || !IsEnabled() || !Mode || !Mode->IsFloorActive())
	{
		return;
	}
	SampleAccumulator += DeltaSeconds;
	if (SampleAccumulator >= SamplePeriod)
	{
		SampleAccumulator = FMath::Fmod(SampleAccumulator, SamplePeriod);
		CaptureFrame();
	}
}

void AMatsuriReplayDirector::CaptureFrame(bool bForce)
{
	if (!GetWorld() || (!bForce && GetWorld()->IsPaused()))
	{
		return;
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	Player = Pawn;
	FMatsuriReplayFrame Frame;
	Frame.Time = GetWorld()->GetTimeSeconds();
	Frame.PlayerTransform = Pawn->GetActorTransform();
	if (!Boss.IsValid())
	{
		for (TActorIterator<AMatsuriBoss> It(GetWorld()); It; ++It)
		{
			Boss = *It;
			break;
		}
	}
	if (Boss.IsValid())
	{
		Frame.BossTransform = Boss->GetActorTransform();
	}
	if (!Field.IsValid())
	{
		for (TActorIterator<AMatsuriDanmakuField> It(GetWorld()); It; ++It)
		{
			Field = *It;
			break;
		}
	}
	if (Field.IsValid())
	{
		Frame.Bullets = Field->GetBullets();
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && (Actor->ActorHasTag(Matsuri::Combat::YokaiTag) || Actor->IsA<AMatsuriProjectile>() || Actor == ReplaySource.Get()))
		{
			FMatsuriReplayActorFrame& State = Frame.Actors.AddDefaulted_GetRef();
			State.Id = Actor->GetUniqueID();
			State.Transform = Actor->GetActorTransform();
		}
	}
	History.Add(MoveTemp(Frame));
}

bool AMatsuriReplayDirector::Start(const AActor* Source, const FText& Cause, int32 Skill, FSimpleDelegate OnFinished)
{
	if (bReplaying || !IsEnabled() || !GetWorld())
	{
		return false;
	}
	ReplaySource = const_cast<AActor*>(Source);
	ReplaySourceId = Source ? Source->GetUniqueID() : 0;
	// The fatal frame may happen between two samples; capture it before pausing.
	CaptureFrame(true);
	if (History.Num() < 2 || History.Duration() <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	Player = PC ? PC->GetPawn() : nullptr;
	if (!PC || !Player.IsValid())
	{
		return false;
	}
	ReplaySkill = Skill;
	Finished = MoveTemp(OnFinished);
	bCompletionSent = false;
	bReplaying = true;
	bRecording = false;
	PlaybackAge = 0.0f;
	PlaybackDuration = History.Duration();
	PlaybackFrameIndex = 0;
	PlaybackBulletCount = 0;
	bSavedPaused = GetWorld()->IsPaused();
	bSavedPlayerHidden = Player->IsHidden();
	bSavedPlayerCollision = Player->GetActorEnableCollision();
	SavedPlayerTransform = Player->GetActorTransform();
	SavedControlRotation = PC->GetControlRotation();
	SavedViewTarget = PC->GetViewTarget();
	if (Boss.IsValid())
	{
		SavedBossTransform = Boss->GetActorTransform();
		bSavedBossTransform = true;
		bSavedBossHidden = Boss->IsHidden();
		if (ReplaySource.Get() == Boss.Get())
		{
			Boss->SetActorHiddenInGame(true);
		}
	}
	if (ReplaySource.IsValid() && ReplaySource.Get() != Boss.Get())
	{
		bSavedSourceHidden = ReplaySource->IsHidden();
		ReplaySource->SetActorHiddenInGame(true);
	}
	SaveVisualActors();
	EnsureHistoricalActors();
	BuildProxy();
	BuildOverlay(Cause);
	ReplayCamera = GetWorld()->SpawnActor<ACameraActor>();
	if (ReplayCamera)
	{
		ReplayCamera->SetActorEnableCollision(false);
		PC->SetViewTarget(ReplayCamera);
	}
	Player->SetActorHiddenInGame(true);
	Player->SetActorEnableCollision(false);
	PC->SetPause(true);
	if (History.At(0))
	{
		ApplyFrame(*History.At(0));
	}
	ReplayTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
	{
		if (!bReplaying)
		{
			return false;
		}
		AdvanceReplay(DeltaTime);
		return bReplaying;
	}));
	return true;
}

void AMatsuriReplayDirector::BuildProxy()
{
	if (HeroProxy || !GetWorld())
	{
		return;
	}
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	const EMatsuriHero Hero = Run ? Run->GetRunState().Hero : EMatsuriHero::Sakura;
	USkeletalMesh* Mesh = Matsuri::Art::HeroMesh(Hero, TEXT("Idle"));
	if (Mesh)
	{
		HeroProxy = NewObject<USkeletalMeshComponent>(this, TEXT("ReplayHeroProxy"));
		HeroProxy->SetSkeletalMesh(Mesh);
		if (Hero == EMatsuriHero::Suzune)
		{
			Matsuri::Toon::Apply(HeroProxy, false);
		}
		HeroProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HeroProxy->SetCanEverAffectNavigation(false);
		HeroProxy->SetRelativeScale3D(FVector(Matsuri::Art::HeroineScale));
		HeroProxy->RegisterComponent();
		if (UAnimSequence* Idle = Matsuri::Art::HeroAnim(Hero, TEXT("Idle")))
		{
			HeroProxy->PlayAnimation(Idle, true);
		}
	}
	else
	{
		HeroFallbackProxy = NewObject<UStaticMeshComponent>(this, TEXT("ReplayHeroFallbackProxy"));
		HeroFallbackProxy->SetStaticMesh(Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Cylinder));
		HeroFallbackProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HeroFallbackProxy->SetCanEverAffectNavigation(false);
		HeroFallbackProxy->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.9f));
		HeroFallbackProxy->RegisterComponent();
		Matsuri::Graybox::Paint(HeroFallbackProxy, FLinearColor(1.0f, 0.65f, 0.1f), true, 4.0f);
	}
	if (ReplaySource.IsValid() && ReplaySource.Get() != Boss.Get())
	{
		KillerProxy = NewObject<UStaticMeshComponent>(this, TEXT("ReplayKillerProxy"));
		KillerProxy->SetStaticMesh(Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Sphere));
		KillerProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		KillerProxy->SetCanEverAffectNavigation(false);
		KillerProxy->RegisterComponent();
		Matsuri::Graybox::Paint(KillerProxy, FLinearColor(0.95f, 0.08f, 0.08f), true, 8.0f);
	}
}

void AMatsuriReplayDirector::EnsureHistoricalActors()
{
	TSet<uint32> Seen;
	for (const FMatsuriReplaySavedActor& Saved : SavedVisualActors)
	{
		Seen.Add(Saved.Id);
	}
	auto MakeProxy = [this](AActor* Actor, uint32 Id) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Proxy = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("ReplayHistoricalProxy_%u"), Id));
		UStaticMesh* Mesh = nullptr;
		if (Actor)
		{
			TArray<UStaticMeshComponent*> Parts;
			Actor->GetComponents<UStaticMeshComponent>(Parts);
			for (UStaticMeshComponent* Part : Parts)
			{
				if (Part && Part->GetStaticMesh())
				{
					Mesh = Part->GetStaticMesh();
					break;
				}
			}
		}
		Proxy->SetStaticMesh(Mesh ? Mesh : Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Sphere));
		if (Actor)
		{
			TArray<UStaticMeshComponent*> Parts;
			Actor->GetComponents<UStaticMeshComponent>(Parts);
			for (UStaticMeshComponent* Part : Parts)
			{
				if (Part && Part->GetStaticMesh())
				{
					for (int32 MaterialIndex = 0; MaterialIndex < Part->GetNumMaterials(); ++MaterialIndex)
					{
						Proxy->SetMaterial(MaterialIndex, Part->GetMaterial(MaterialIndex));
					}
					break;
				}
			}
		}
		Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Proxy->SetCanEverAffectNavigation(false);
		Proxy->SetVisibility(false);
		Proxy->RegisterComponent();
		if (!Mesh)
		{
			Matsuri::Graybox::Paint(Proxy, FLinearColor(0.8f, 0.2f, 0.9f), true, 4.0f);
		}
		return Proxy;
	};
	for (int32 FrameIndex = 0; FrameIndex < History.Num(); ++FrameIndex)
	{
		const FMatsuriReplayFrame* Frame = History.At(FrameIndex);
		if (!Frame)
		{
			continue;
		}
		for (const FMatsuriReplayActorFrame& State : Frame->Actors)
		{
			if (Seen.Contains(State.Id))
			{
				continue;
			}
			Seen.Add(State.Id);
			FMatsuriReplaySavedActor& Saved = SavedVisualActors.AddDefaulted_GetRef();
			Saved.Id = State.Id;
			Saved.Transform = State.Transform;
			SavedActorProxies.Add(MakeProxy(nullptr, State.Id));
		}
	}
}

void AMatsuriReplayDirector::SaveVisualActors()
{
	SavedVisualActors.Reset();
	SavedActorProxies.Reset();
	if (!GetWorld())
	{
		return;
	}
	TSet<uint32> Seen;
	auto SaveActor = [this, &Seen](AActor* Actor)
	{
		if (!Actor || Actor == Player.Get() || Actor == Boss.Get() || Actor == this || Seen.Contains(Actor->GetUniqueID()))
		{
			return;
		}
		Seen.Add(Actor->GetUniqueID());
		FMatsuriReplaySavedActor& Saved = SavedVisualActors.AddDefaulted_GetRef();
		Saved.Actor = Actor;
		Saved.Transform = Actor->GetActorTransform();
		Saved.bHidden = Actor->IsHidden();
		Saved.bCollision = Actor->GetActorEnableCollision();
		Saved.Id = Actor->GetUniqueID();
		Actor->SetActorEnableCollision(false);
		UStaticMeshComponent* Proxy = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("ReplayActorProxy_%u"), Saved.Id));
		UStaticMesh* VisualMesh = Matsuri::Graybox::Mesh(Matsuri::Graybox::EShape::Sphere);
		TArray<UStaticMeshComponent*> Parts;
		Actor->GetComponents<UStaticMeshComponent>(Parts);
		for (UStaticMeshComponent* Part : Parts)
		{
			if (Part && Part->GetStaticMesh())
			{
				VisualMesh = Part->GetStaticMesh();
				break;
			}
		}
		Proxy->SetStaticMesh(VisualMesh);
		for (UStaticMeshComponent* Part : Parts)
		{
			if (Part && Part->GetStaticMesh() == VisualMesh)
			{
				for (int32 MaterialIndex = 0; MaterialIndex < Part->GetNumMaterials(); ++MaterialIndex)
				{
					Proxy->SetMaterial(MaterialIndex, Part->GetMaterial(MaterialIndex));
				}
				break;
			}
		}
		Proxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Proxy->SetCanEverAffectNavigation(false);
		Proxy->SetVisibility(false);
		Proxy->RegisterComponent();
		Matsuri::Graybox::Paint(Proxy, FLinearColor(0.8f, 0.2f, 0.9f), true, 4.0f);
		SavedActorProxies.Add(Proxy);
	};
	for (TActorIterator<AMatsuriYokai> It(GetWorld()); It; ++It)
	{
		SaveActor(*It);
	}
	for (TActorIterator<AMatsuriProjectile> It(GetWorld()); It; ++It)
	{
		SaveActor(*It);
	}
}

void AMatsuriReplayDirector::BuildOverlay(const FText& Cause)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}
	Overlay = CreateWidget<UMatsuriReplayWidget>(PC, UMatsuriReplayWidget::StaticClass());
	if (Overlay)
	{
		Overlay->SetDirector(this);
		Overlay->Configure(Cause, ReplaySource.IsValid());
		Overlay->AddToViewport(static_cast<int32>(ReplayZOrder));
		PC->SetInputMode(FInputModeGameAndUI());
		PC->SetShowMouseCursor(true);
	}
}

void AMatsuriReplayDirector::AdvanceReplay(float DeltaSeconds)
{
	PlaybackAge = FMath::Min(PlaybackDuration, PlaybackAge + FMath::Max(0.0f, DeltaSeconds));
	const float AbsoluteTime = (History.Num() > 0 ? History.At(0)->Time : 0.0f) + PlaybackAge;
	const FMatsuriReplayFrame* Frame = History.Sample(AbsoluteTime);
	if (Frame)
	{
		for (int32 Index = 0; Index < History.Num(); ++Index)
		{
			if (History.At(Index) == Frame)
			{
				PlaybackFrameIndex = Index;
				break;
			}
		}
		ApplyFrame(*Frame);
	}
	if (Overlay)
	{
		Overlay->SetProgress(GetPlaybackProgress(), PlaybackAge);
	}
	if (PlaybackAge >= PlaybackDuration)
	{
		FinishReplay();
	}
}

float AMatsuriReplayDirector::GetPlaybackProgress() const
{
	return PlaybackDuration > KINDA_SMALL_NUMBER ? FMath::Clamp(PlaybackAge / PlaybackDuration, 0.0f, 1.0f) : 1.0f;
}

void AMatsuriReplayDirector::ApplyFrame(const FMatsuriReplayFrame& Frame)
{
	if (HeroProxy)
	{
		FTransform ProxyTransform = Frame.PlayerTransform;
		if (const ACharacter* Character = Cast<ACharacter>(Player.Get()))
		{
			if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
			{
				ProxyTransform.AddToTranslation(FVector(0.0f, 0.0f, -Capsule->GetScaledCapsuleHalfHeight()));
			}
		}
		ProxyTransform.SetScale3D(FVector(Matsuri::Art::HeroineScale));
		HeroProxy->SetWorldTransform(ProxyTransform);
	}
	if (HeroFallbackProxy)
	{
		FTransform ProxyTransform = Frame.PlayerTransform;
		if (const ACharacter* Character = Cast<ACharacter>(Player.Get()))
		{
			if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
			{
				ProxyTransform.AddToTranslation(FVector(0.0f, 0.0f, -Capsule->GetScaledCapsuleHalfHeight() + 90.0f));
			}
		}
		HeroFallbackProxy->SetWorldTransform(ProxyTransform);
	}
	if (Boss.IsValid())
	{
		Boss->SetActorTransform(Frame.BossTransform);
	}
	if (Field.IsValid())
	{
		Field->ShowReplayBullets(Frame.Bullets);
		PlaybackBulletCount = Frame.Bullets.Num();
	}
	ApplyVisualActors(Frame);
	if (ReplayCamera)
	{
		FTransform Killer = Frame.BossTransform;
		bool bHasKiller = ReplaySourceId != 0 && Boss.IsValid() && ReplaySource.Get() == Boss.Get();
		if (ReplaySourceId != 0 && !bHasKiller)
		{
			for (const FMatsuriReplayActorFrame& State : Frame.Actors)
			{
				if (State.Id == ReplaySourceId)
				{
					Killer = State.Transform;
					bHasKiller = true;
					break;
				}
			}
		}
		const FVector Victim = Frame.PlayerTransform.GetLocation() + FVector(0.0f, 0.0f, 70.0f);
		const FVector CameraLocation = bHasKiller
			? Killer.GetLocation() + FVector(0.0f, 0.0f, 110.0f)
			: Victim + Frame.PlayerTransform.GetRotation().RotateVector(FVector(-300.0f, 0.0f, 120.0f));
		ReplayCamera->SetActorLocationAndRotation(CameraLocation, (Victim - CameraLocation).Rotation());
		if (KillerProxy && bHasKiller)
		{
			KillerProxy->SetWorldTransform(Killer);
		}
	}
}

void AMatsuriReplayDirector::ApplyVisualActors(const FMatsuriReplayFrame& Frame)
{
	for (int32 ActorIndex = 0; ActorIndex < SavedVisualActors.Num(); ++ActorIndex)
	{
		FMatsuriReplaySavedActor& Saved = SavedVisualActors[ActorIndex];
		const bool bIsSource = ReplaySourceId != 0 && Saved.Id == ReplaySourceId;
		const FMatsuriReplayActorFrame* Historical = nullptr;
		for (const FMatsuriReplayActorFrame& State : Frame.Actors)
		{
			if (State.Id == Saved.Id)
			{
				Historical = &State;
				break;
			}
		}
		if (bIsSource)
		{
			if (Saved.Actor.IsValid())
			{
				Saved.Actor->SetActorHiddenInGame(true);
			}
			if (SavedActorProxies.IsValidIndex(ActorIndex))
			{
				SavedActorProxies[ActorIndex]->SetVisibility(false);
			}
			continue;
		}
		if (Saved.Actor.IsValid() && Historical)
		{
			Saved.Actor->SetActorTransform(Historical->Transform);
			Saved.Actor->SetActorHiddenInGame(false);
			if (SavedActorProxies.IsValidIndex(ActorIndex))
			{
				SavedActorProxies[ActorIndex]->SetVisibility(false);
			}
		}
		else if (Historical && SavedActorProxies.IsValidIndex(ActorIndex))
		{
			SavedActorProxies[ActorIndex]->SetWorldTransform(Historical->Transform);
			SavedActorProxies[ActorIndex]->SetVisibility(true);
		}
		else
		{
			if (Saved.Actor.IsValid())
			{
				Saved.Actor->SetActorHiddenInGame(true);
			}
			if (SavedActorProxies.IsValidIndex(ActorIndex))
			{
				SavedActorProxies[ActorIndex]->SetVisibility(false);
			}
		}
	}
}

void AMatsuriReplayDirector::DestroyReplayCamera()
{
	if (ReplayCamera)
	{
		ReplayCamera->Destroy();
		ReplayCamera = nullptr;
	}
}

void AMatsuriReplayDirector::Complete(bool bNotify)
{
	if (!bReplaying)
	{
		return;
	}
	if (ReplayTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReplayTicker);
		ReplayTicker.Reset();
	}
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC && bNotify)
	{
		if (SavedViewTarget.IsValid())
		{
			PC->SetViewTarget(SavedViewTarget.Get());
		}
		PC->SetControlRotation(SavedControlRotation);
		PC->SetPause(bSavedPaused);
		PC->SetShowMouseCursor(false);
	}
	if (Player.IsValid())
	{
		if (bNotify)
		{
			Player->SetActorTransform(SavedPlayerTransform);
			Player->SetActorHiddenInGame(bSavedPlayerHidden);
			Player->SetActorEnableCollision(bSavedPlayerCollision);
		}
	}
	if (Boss.IsValid() && bSavedBossTransform && bNotify)
	{
		Boss->SetActorTransform(SavedBossTransform);
		Boss->SetActorHiddenInGame(bSavedBossHidden);
	}
	if (ReplaySource.IsValid() && ReplaySource.Get() != Boss.Get() && bNotify)
	{
		ReplaySource->SetActorHiddenInGame(bSavedSourceHidden);
	}
	if (bNotify)
	{
		for (const FMatsuriReplaySavedActor& Saved : SavedVisualActors)
		{
			if (Saved.Actor.IsValid())
			{
				Saved.Actor->SetActorTransform(Saved.Transform);
				Saved.Actor->SetActorHiddenInGame(Saved.bHidden);
				Saved.Actor->SetActorEnableCollision(Saved.bCollision);
			}
		}
	}
	if (Field.IsValid() && bNotify)
	{
		Field->ShowReplayBullets(TArray<FMatsuriBullet>());
	}
	PlaybackBulletCount = 0;
	if (Overlay)
	{
		Overlay->RemoveFromParent();
		Overlay = nullptr;
	}
	if (HeroProxy)
	{
		HeroProxy->DestroyComponent();
		HeroProxy = nullptr;
	}
	if (KillerProxy)
	{
		KillerProxy->DestroyComponent();
		KillerProxy = nullptr;
	}
	if (HeroFallbackProxy)
	{
		HeroFallbackProxy->DestroyComponent();
		HeroFallbackProxy = nullptr;
	}
	for (UStaticMeshComponent* Proxy : SavedActorProxies)
	{
		if (Proxy)
		{
			Proxy->DestroyComponent();
		}
	}
	SavedActorProxies.Reset();
	SavedVisualActors.Reset();
	DestroyReplayCamera();
	bReplaying = false;
	bRecording = false;
	if (bNotify && !bCompletionSent)
	{
		bCompletionSent = true;
		FSimpleDelegate Callback = MoveTemp(Finished);
		if (Callback.IsBound())
		{
			Callback.Execute();
		}
	}
}

void AMatsuriReplayDirector::FinishReplay()
{
	Complete(true);
}

void AMatsuriReplayDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Complete(false);
	Super::EndPlay(EndPlayReason);
}

