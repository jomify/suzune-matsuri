// Suzune's Night Festival - finished art with gray-box fallback.

#include "MatsuriArt.h"
#include "Animation/AnimSequence.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#if WITH_EDITOR
#include "StaticMeshCompiler.h"
#endif

namespace
{
	TAutoConsoleVariable<bool> CVarArt(
		TEXT("matsuri.Art"), true,
		TEXT("Use the generated Tripo art where it exists. 0 = gray-box everywhere (for comparison)."));

	TAutoConsoleVariable<bool> CVarShrinePack(
		TEXT("matsuri.ShrinePack"), true,
		TEXT("Dress the scenes with the Mountain Temple pack when it is in the project. 0 = without it (for comparison)."));

	const TCHAR* HeroAssetName(EMatsuriHero Hero)
	{
		switch (Hero)
		{
		case EMatsuriHero::Sakura:	return TEXT("SK_Hero_Sakura");
		case EMatsuriHero::Suzune:	return TEXT("SK_Hero_Suzune");
		case EMatsuriHero::Mio:		return TEXT("SK_Hero_Mio");
		case EMatsuriHero::Count:	break;
		}
		return TEXT("");
	}

	/**
	 *  Every lookup is cached, found or missing. Asking the disk (DoesPackageExist) on each spawn made
	 *  every shot, coin and lantern hitch the game thread - felt in play as late hits.
	 */
	TMap<FString, TWeakObjectPtr<UObject>>& FoundArt()
	{
		static TMap<FString, TWeakObjectPtr<UObject>> Found;
		return Found;
	}

	TSet<FString>& MissingArt()
	{
		static TSet<FString> Missing;
		return Missing;
	}

	/** Loads Package.Asset if the package exists; never warns, so missing art is silent. */
	template <typename T>
	T* LoadIfPresent(const FString& Package)
	{
		if (!Matsuri::Art::IsEnabled() || MissingArt().Contains(Package))
		{
			return nullptr;
		}
		if (const TWeakObjectPtr<UObject>* Cached = FoundArt().Find(Package))
		{
			if (T* Asset = Cast<T>(Cached->Get()))
			{
				return Asset;	// still loaded: no disk access at all
			}
		}
		if (!FPackageName::DoesPackageExist(Package))
		{
			MissingArt().Add(Package);
			return nullptr;
		}
		const FString Object = Package + TEXT(".") + FPackageName::GetShortName(Package);
		T* Asset = LoadObject<T>(nullptr, *Object, nullptr, LOAD_Quiet | LOAD_NoWarn);
		FoundArt().Add(Package, Asset);
		return Asset;
	}
}

bool Matsuri::Art::IsEnabled()
{
	return CVarArt.GetValueOnAnyThread();
}

bool Matsuri::Art::HasShrinePack()
{
	return PackMesh(TEXT("SM_Lantern_01")) != nullptr;
}

UStaticMesh* Matsuri::Art::PackMesh(const TCHAR* Name)
{
	return CVarShrinePack.GetValueOnGameThread() ? LoadIfPresent<UStaticMesh>(FString(TEXT("/Game/Mountain_Shrine/Meshes/")) + Name) : nullptr;
}

UNiagaraSystem* Matsuri::Art::PackEffect(const TCHAR* Name)
{
	return CVarShrinePack.GetValueOnGameThread() ? LoadIfPresent<UNiagaraSystem>(FString(TEXT("/Game/Mountain_Shrine/VFX/")) + Name) : nullptr;
}

UTexture2D* Matsuri::Art::PackTexture(const TCHAR* Path)
{
	return CVarShrinePack.GetValueOnGameThread() ? LoadIfPresent<UTexture2D>(FString(TEXT("/Game/Mountain_Shrine/Textures/")) + Path) : nullptr;
}

UMaterialInterface* Matsuri::Art::PackMaterial(const TCHAR* Name)
{
	return CVarShrinePack.GetValueOnGameThread() && Name && *Name
		? LoadIfPresent<UMaterialInterface>(FString(TEXT("/Game/Mountain_Shrine/Materials/Simple_Materials/")) + Name) : nullptr;
}

UStaticMesh* Matsuri::Art::Mesh(const TCHAR* Name)
{
	if (!Name || !*Name)
	{
		return nullptr;	// "no art for this one" (e.g. Sakura's arrows stay light streaks)
	}
	return LoadIfPresent<UStaticMesh>(FString(TEXT("/Game/Matsuri/Tripo/Meshes/")) + Name);
}

USkeletalMesh* Matsuri::Art::CharacterMesh(const TCHAR* Asset, const TCHAR* Clip)
{
	return LoadIfPresent<USkeletalMesh>(FString::Printf(TEXT("/Game/Matsuri/Tripo/Heroes/%s_%s"), Asset, Clip));
}

UAnimSequence* Matsuri::Art::CharacterAnim(const TCHAR* Asset, const TCHAR* Clip)
{
	return LoadIfPresent<UAnimSequence>(FString::Printf(TEXT("/Game/Matsuri/Tripo/Heroes/A_%s_%s"), Asset, Clip));
}

USkeletalMesh* Matsuri::Art::HeroMesh(EMatsuriHero Hero, const TCHAR* Clip)
{
	return CharacterMesh(HeroAssetName(Hero), Clip);
}

UAnimSequence* Matsuri::Art::HeroAnim(EMatsuriHero Hero, const TCHAR* Clip)
{
	return CharacterAnim(HeroAssetName(Hero), Clip);
}

USoundBase* Matsuri::Art::Sound(const TCHAR* Name)
{
	return LoadIfPresent<USoundBase>(FString(TEXT("/Game/Matsuri/Audio/SFX_")) + Name);
}

USoundBase* Matsuri::Art::Music(const TCHAR* Name)
{
	return LoadIfPresent<USoundBase>(FString(TEXT("/Game/Matsuri/Audio/Music/")) + Name);
}

UTexture2D* Matsuri::Art::Texture(const TCHAR* Name)
{
	return LoadIfPresent<UTexture2D>(FString(TEXT("/Game/Matsuri/Tripo/Textures/")) + Name);
}

UTexture2D* Matsuri::Art::Portrait(EMatsuriHero Hero)
{
	return LoadIfPresent<UTexture2D>(FString::Printf(TEXT("/Game/Matsuri/Tripo/Heroes/T_%s_Portrait"), HeroAssetName(Hero)));
}

bool Matsuri::Art::Dress(UStaticMeshComponent* Part, const FMatsuriArtPose& Pose)
{
	UStaticMesh* Art = Part ? Mesh(Pose.Mesh) : nullptr;
	if (!Art)
	{
		return false;
	}
	Part->EmptyOverrideMaterials();
	Part->SetStaticMesh(Art);
	Part->SetRelativeRotation(Pose.Rotation);
	Part->SetRelativeScale3D(FVector(Pose.Scale));
	Part->AddRelativeLocation(Pose.Offset);
	return true;
}

UStaticMeshComponent* Matsuri::Art::Attach(USceneComponent* Parent, const FMatsuriArtPose& Pose)
{
	UStaticMesh* Art = Parent ? Mesh(Pose.Mesh) : nullptr;
	if (!Art || !Parent->GetOwner())
	{
		return nullptr;
	}
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Parent->GetOwner());
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCanEverAffectNavigation(false);
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetStaticMesh(Art);
	Part->SetupAttachment(Parent);
	Part->SetRelativeTransform(FTransform(Pose.Rotation, Pose.Offset, FVector(Pose.Scale)));
	Part->RegisterComponent();
	return Part;
}

void Matsuri::Art::Preload()
{
	static const TCHAR* const FloorMeshes[] = {
		TEXT("SM_Weapon_HamaYumi"), TEXT("SM_Weapon_Arrow"), TEXT("SM_Weapon_Shuriken"), TEXT("SM_Weapon_Suzu"), TEXT("SM_Weapon_Ooban"),
		TEXT("SM_Pickup_Koban"), TEXT("SM_Pickup_Ginka"), TEXT("SM_Pickup_Magatama"), TEXT("SM_Pickup_Ofuda"),
		TEXT("SM_Yokai_Chochin"), TEXT("SM_Yokai_Kasa"), TEXT("SM_Yokai_Kappa"),
		TEXT("SM_Prop_Chochin"), TEXT("SM_Prop_StoneLantern"), TEXT("SM_Prop_Yatai"), TEXT("SM_Prop_Torii"),
		TEXT("SM_Prop_KitsuneStatue"), TEXT("SM_Prop_Bamboo") };
	TArray<UStaticMesh*> Loaded;
	for (const TCHAR* Name : FloorMeshes)
	{
		if (UStaticMesh* Found = Mesh(Name))
		{
			Loaded.Add(Found);
		}
	}
#if WITH_EDITOR
	FStaticMeshCompilingManager::Get().FinishCompilation(Loaded);
#endif
}
