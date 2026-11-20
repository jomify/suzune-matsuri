// Suzune's Night Festival - background music.

#include "MatsuriMusic.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "MatsuriArt.h"
#include "SuzuneMatsuri.h"

namespace
{
	TAutoConsoleVariable<float> CVarMusicVolume(
		TEXT("matsuri.MusicVolume"), 0.55f,
		TEXT("Background music volume, 0-1 (0 = off). Effects keep their own volume."));

	constexpr float CrossfadeSeconds = 1.5f;	// one scene's track into the next
	constexpr float StingCutSeconds = 0.4f;		// the music gets out of a sting's way quickly
	constexpr float StingVolume = 1.3f;			// relative to the music volume: a sting is the moment's headline

	float MusicVolume()
	{
		return FMath::Clamp(CVarMusicVolume.GetValueOnGameThread(), 0.0f, 1.0f);
	}
}

const TCHAR* Matsuri::Music::AssetName(EMatsuriMusic Track)
{
	switch (Track)
	{
	case EMatsuriMusic::Title:			return TEXT("BGM_Title");
	case EMatsuriMusic::Floor1:			return TEXT("BGM_1F_Bamboo");
	case EMatsuriMusic::Floor2:			return TEXT("BGM_2F_Yatai");
	case EMatsuriMusic::Floor3:			return TEXT("BGM_3F_Torii");
	case EMatsuriMusic::BossOpening:	return TEXT("BGM_Boss_P1");
	case EMatsuriMusic::BossFinal:		return TEXT("BGM_Boss_P2");
	case EMatsuriMusic::Story:			return TEXT("BGM_Story");
	case EMatsuriMusic::Ending:			return TEXT("BGM_Ending");
	case EMatsuriMusic::None:			break;
	}
	return TEXT("");
}

const TCHAR* Matsuri::Music::AssetName(EMatsuriSting Sting)
{
	return Sting == EMatsuriSting::Clear ? TEXT("STING_Clear") : TEXT("STING_Defeat");
}

USoundBase* Matsuri::Music::Sound(EMatsuriMusic Track)
{
	const TCHAR* Name = AssetName(Track);
	return *Name ? Matsuri::Art::Music(Name) : nullptr;
}

USoundBase* Matsuri::Music::Sound(EMatsuriSting Sting)
{
	return Matsuri::Art::Music(AssetName(Sting));
}

EMatsuriMusic Matsuri::Music::ForFloor(int32 FloorIndex)
{
	switch (FloorIndex)
	{
	case 0:		return EMatsuriMusic::Floor1;
	case 1:		return EMatsuriMusic::Floor2;
	case 2:		return EMatsuriMusic::Floor3;
	default:	return EMatsuriMusic::BossOpening;
	}
}

EMatsuriMusic Matsuri::Music::ForBossPhase(int32 Phase)
{
	return Phase >= 3 ? EMatsuriMusic::BossFinal : EMatsuriMusic::BossOpening;
}

void Matsuri::Music::Play(const UObject* WorldContext, EMatsuriMusic Track)
{
	if (UMatsuriMusicSubsystem* Music = UMatsuriMusicSubsystem::Get(WorldContext))
	{
		Music->Play(Track);
	}
}

void Matsuri::Music::PlaySting(const UObject* WorldContext, EMatsuriSting Sting)
{
	if (UMatsuriMusicSubsystem* Music = UMatsuriMusicSubsystem::Get(WorldContext))
	{
		Music->PlaySting(Sting);
	}
}

EMatsuriMusic Matsuri::Music::Current(const UObject* WorldContext)
{
	const UMatsuriMusicSubsystem* Music = UMatsuriMusicSubsystem::Get(WorldContext);
	return Music ? Music->GetCurrent() : EMatsuriMusic::None;
}

UMatsuriMusicSubsystem* UMatsuriMusicSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UMatsuriMusicSubsystem>() : nullptr;
}

void UMatsuriMusicSubsystem::Play(EMatsuriMusic Track)
{
	if (Track == Current && (Track == EMatsuriMusic::None || (TrackComponent && TrackComponent->IsPlaying())))
	{
		return;	// already playing: a floor restarting its own track would be a hiccup
	}
	if (Track == Current && !TrackComponent && !Matsuri::Music::Sound(Track))
	{
		return;	// chosen but not imported: stays silent
	}
	FadeAway(TrackComponent, CrossfadeSeconds);
	FadeAway(StingComponent, CrossfadeSeconds);
	TrackComponent = nullptr;
	StingComponent = nullptr;
	Current = Track;
	Sting.Reset();
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Music: %s"), Track == EMatsuriMusic::None ? TEXT("(none)") : Matsuri::Music::AssetName(Track));
	if (USoundBase* Sound = Matsuri::Music::Sound(Track))
	{
		TrackComponent = Start(Sound, MusicVolume(), CrossfadeSeconds);
	}
}

void UMatsuriMusicSubsystem::PlaySting(EMatsuriSting InSting)
{
	FadeAway(TrackComponent, StingCutSeconds);
	FadeAway(StingComponent, StingCutSeconds);
	TrackComponent = nullptr;
	StingComponent = nullptr;
	Current = EMatsuriMusic::None;
	Sting = InSting;
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Music: sting %s"), Matsuri::Music::AssetName(InSting));
	if (USoundBase* Sound = Matsuri::Music::Sound(InSting))
	{
		StingComponent = Start(Sound, FMath::Min(1.0f, MusicVolume() * StingVolume), 0.0f);
	}
}

void UMatsuriMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	for (int32 i = static_cast<int32>(EMatsuriMusic::Title); i <= static_cast<int32>(EMatsuriMusic::Ending); ++i)
	{
		if (USoundBase* Sound = Matsuri::Music::Sound(static_cast<EMatsuriMusic>(i)))
		{
			Loaded.Add(Sound);
		}
	}
	for (const EMatsuriSting Each : { EMatsuriSting::Clear, EMatsuriSting::Defeat })
	{
		if (USoundBase* Sound = Matsuri::Music::Sound(Each))
		{
			Loaded.Add(Sound);
		}
	}
}

void UMatsuriMusicSubsystem::Deinitialize()
{
	for (UAudioComponent* Component : { TrackComponent.Get(), StingComponent.Get() })
	{
		if (Component)
		{
			Component->Stop();
		}
	}
	TrackComponent = nullptr;
	StingComponent = nullptr;
	Loaded.Reset();
	Super::Deinitialize();
}

UAudioComponent* UMatsuriMusicSubsystem::Start(USoundBase* Sound, float Volume, float FadeInSeconds)
{
	if (Volume <= 0.0f)
	{
		return nullptr;
	}
	// Persists across level loads (the next scene crossfades from it); no world audio device (-NoSound): nullptr.
	UAudioComponent* Component = UGameplayStatics::CreateSound2D(GetGameInstance(), Sound, Volume, 1.0f, 0.0f, nullptr, true, false);
	if (!Component)
	{
		return nullptr;
	}
	Component->bIsUISound = true;	// keeps playing under a paused menu (results, game over)
	if (FadeInSeconds > 0.0f)
	{
		Component->FadeIn(FadeInSeconds);	// to full: the component's own multiplier is the volume
	}
	else
	{
		Component->Play();
	}
	return Component;
}

void UMatsuriMusicSubsystem::FadeAway(UAudioComponent* Component, float Seconds)
{
	if (Component)
	{
		Component->bAutoDestroy = true;	// gone once the fade ends
		Component->FadeOut(Seconds, 0.0f);
	}
}
