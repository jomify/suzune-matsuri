// Suzune's Night Festival - sound effects.

#include "MatsuriAudio.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "MatsuriArt.h"

namespace
{
	constexpr int32 FootstepVariants = 4;
	constexpr float PitchJitter = 0.06f;
	constexpr float HearInner = 300.0f;		// cm: full volume within this
	constexpr float HearFalloff = 2600.0f;	// cm: silent beyond inner + this

	int32 Counts[static_cast<int32>(EMatsuriSfx::Count)] = {};

	bool Jitters(EMatsuriSfx Sfx)
	{
		return Sfx == EMatsuriSfx::Footstep || Sfx == EMatsuriSfx::HitTick || Sfx == EMatsuriSfx::CoinPickup
			|| Sfx == EMatsuriSfx::BellBounce || Sfx == EMatsuriSfx::CoinBounce || Sfx == EMatsuriSfx::ArrowThunk || Sfx == EMatsuriSfx::MeleeHit
		|| Sfx == EMatsuriSfx::Graze || Sfx == EMatsuriSfx::BossShot;
	}

	USoundAttenuation* WorldFalloff()
	{
		static TStrongObjectPtr<USoundAttenuation> Attenuation;
		if (!Attenuation)
		{
			Attenuation.Reset(NewObject<USoundAttenuation>());
			Attenuation->Attenuation.bAttenuate = true;
			Attenuation->Attenuation.bSpatialize = true;
			Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			Attenuation->Attenuation.AttenuationShapeExtents = FVector(HearInner, 0.0f, 0.0f);
			Attenuation->Attenuation.FalloffDistance = HearFalloff;
		}
		return Attenuation.Get();
	}

	float Pitch(EMatsuriSfx Sfx)
	{
		return Jitters(Sfx) ? FMath::FRandRange(1.0f - PitchJitter, 1.0f + PitchJitter) : 1.0f;
	}
}

const TCHAR* Matsuri::Audio::Name(EMatsuriSfx Sfx)
{
	switch (Sfx)
	{
	case EMatsuriSfx::Footstep:			return TEXT("Footstep");
	case EMatsuriSfx::CoinPickup:		return TEXT("CoinPickup");
	case EMatsuriSfx::GemPickup:		return TEXT("GemPickup");
	case EMatsuriSfx::TalismanPickup:	return TEXT("TalismanPickup");
	case EMatsuriSfx::BowDraw:			return TEXT("BowDraw");
	case EMatsuriSfx::BowRelease:		return TEXT("BowRelease");
	case EMatsuriSfx::ArrowWhoosh:		return TEXT("ArrowWhoosh");
	case EMatsuriSfx::ArrowThunk:		return TEXT("ArrowThunk");
	case EMatsuriSfx::KunaiThrow:		return TEXT("KunaiThrow");
	case EMatsuriSfx::BellShot:			return TEXT("BellShot");
	case EMatsuriSfx::BellBounce:		return TEXT("BellBounce");
	case EMatsuriSfx::ZeniThrow:		return TEXT("ZeniThrow");
	case EMatsuriSfx::CoinBounce:		return TEXT("CoinBounce");
	case EMatsuriSfx::MeleeSwing:		return TEXT("MeleeSwing");
	case EMatsuriSfx::MeleeHit:			return TEXT("MeleeHit");
	case EMatsuriSfx::HitTick:			return TEXT("HitTick");
	case EMatsuriSfx::Kill:				return TEXT("Kill");
	case EMatsuriSfx::YokaiDeath:		return TEXT("YokaiDeath");
	case EMatsuriSfx::PlayerHurt:		return TEXT("PlayerHurt");
	case EMatsuriSfx::DryFire:			return TEXT("DryFire");
	case EMatsuriSfx::UiClick:			return TEXT("UiClick");
	case EMatsuriSfx::UiConfirm:		return TEXT("UiConfirm");
	case EMatsuriSfx::Loading:			return TEXT("Loading");
	case EMatsuriSfx::FloorClear:		return TEXT("FloorClear");
	case EMatsuriSfx::Graze:			return TEXT("Graze");
	case EMatsuriSfx::BossShot:			return TEXT("BossShot");
	case EMatsuriSfx::BossPhase:		return TEXT("BossPhase");
	case EMatsuriSfx::BossTeleport:		return TEXT("BossTeleport");
	case EMatsuriSfx::GrappleFire:		return TEXT("GrappleFire");
	case EMatsuriSfx::BossDefeat:		return TEXT("BossDefeat");
	case EMatsuriSfx::Count:			break;
	}
	return TEXT("");
}

USoundBase* Matsuri::Audio::Sound(EMatsuriSfx Sfx)
{
	if (Sfx == EMatsuriSfx::Footstep)
	{
		return Matsuri::Art::Sound(*FString::Printf(TEXT("Footstep_%d"), FMath::RandRange(1, FootstepVariants)));
	}
	return Matsuri::Art::Sound(Name(Sfx));
}

void Matsuri::Audio::Play2D(const UObject* WorldContext, EMatsuriSfx Sfx, float Volume)
{
	++Counts[static_cast<int32>(Sfx)];
	if (USoundBase* Found = Sound(Sfx))
	{
		UGameplayStatics::PlaySound2D(WorldContext, Found, Volume, Pitch(Sfx));
	}
}

UAudioComponent* Matsuri::Audio::Spawn2D(const UObject* WorldContext, EMatsuriSfx Sfx, float Volume)
{
	++Counts[static_cast<int32>(Sfx)];
	USoundBase* Found = Sound(Sfx);
	return Found ? UGameplayStatics::SpawnSound2D(WorldContext, Found, Volume, Pitch(Sfx)) : nullptr;
}

void Matsuri::Audio::Preload()
{
	for (int32 Index = 0; Index < static_cast<int32>(EMatsuriSfx::Count); ++Index)
	{
		const EMatsuriSfx Sfx = static_cast<EMatsuriSfx>(Index);
		if (Sfx != EMatsuriSfx::Footstep)
		{
			Sound(Sfx);
		}
	}
	for (int32 Variant = 1; Variant <= FootstepVariants; ++Variant)
	{
		Matsuri::Art::Sound(*FString::Printf(TEXT("Footstep_%d"), Variant));
	}
}

void Matsuri::Audio::PlayAt(const UObject* WorldContext, EMatsuriSfx Sfx, const FVector& Location, float Volume)
{
	++Counts[static_cast<int32>(Sfx)];
	if (USoundBase* Found = Sound(Sfx))
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContext, Found, Location, Volume, Pitch(Sfx), 0.0f, WorldFalloff());
	}
}

int32 Matsuri::Audio::PlayCount(EMatsuriSfx Sfx)
{
	return Counts[static_cast<int32>(Sfx)];
}
