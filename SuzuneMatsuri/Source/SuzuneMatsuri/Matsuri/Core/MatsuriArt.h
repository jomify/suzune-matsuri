// Suzune's Night Festival - finished art (Tripo meshes, heroine models, portraits) with gray-box fallback.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriArtTable.h"
#include "MatsuriHeroes.h"

class UStaticMesh;
class USkeletalMesh;
class UAnimSequence;
class UTexture2D;
class USceneComponent;
class UStaticMeshComponent;
class UNiagaraSystem;

/**
 *  Art is imported by tools/ue/import_tripo.py to fixed paths. Every getter returns nullptr when the
 *  asset is missing or art is switched off (matsuri.Art 0), and callers then keep the gray-box look,
 *  so the game always runs, with or without the generated art.
 */
namespace Matsuri::Art
{
	SUZUNEMATSURI_API bool IsEnabled();

	/** /Game/Matsuri/Tripo/Meshes/<Name>, e.g. TEXT("SM_Pickup_Koban"). */
	SUZUNEMATSURI_API UStaticMesh* Mesh(const TCHAR* Name);

	/**
	 *  The purchased Fab pack "Mountain Temple" by Hivemind, if it has been added to the project
	 *  (Content/Mountain_Shrine; git-ignored: it is licensed per user, never committed). Scenes dress
	 *  themselves with it when present and look as before when not. matsuri.ShrinePack 0 turns it off.
	 */
	SUZUNEMATSURI_API bool HasShrinePack();
	/** /Game/Mountain_Shrine/Meshes/<Name>, e.g. TEXT("SM_Lantern_01"). */
	SUZUNEMATSURI_API UStaticMesh* PackMesh(const TCHAR* Name);
	/** /Game/Mountain_Shrine/VFX/<Name>, e.g. TEXT("NS_Fireflies"). */
	SUZUNEMATSURI_API UNiagaraSystem* PackEffect(const TCHAR* Name);
	/** /Game/Mountain_Shrine/Textures/<Path> (e.g. "Tileable_Wood/T_Wood_Tileable_BaseColor"), or nullptr. */
	SUZUNEMATSURI_API UTexture2D* PackTexture(const TCHAR* Path);
	/** /Game/Mountain_Shrine/Materials/Simple_Materials/<Name>, or nullptr. */
	SUZUNEMATSURI_API class UMaterialInterface* PackMaterial(const TCHAR* Name);

	/**
	 *  Swaps a gray-box part's shape for the art mesh in Pose: rotation and scale from the pose, the
	 *  offset added once to the part's place, tint overrides cleared so the texture shows. Returns false
	 *  and leaves the part untouched when the art is missing.
	 */
	SUZUNEMATSURI_API bool Dress(UStaticMeshComponent* Part, const FMatsuriArtPose& Pose);

	/** Runtime: adds the art mesh as a visual-only child of Parent; nullptr when the art is missing. */
	SUZUNEMATSURI_API UStaticMeshComponent* Attach(USceneComponent* Parent, const FMatsuriArtPose& Pose);

	/**
	 *  Loads every Tripo static mesh the floors use and (in the editor) finishes compiling them now,
	 *  while the floor is built, instead of stalling the first frame a yokai, shot or prop appears.
	 */
	SUZUNEMATSURI_API void Preload();

	/** A rigged Tripo character's skeletal mesh for one clip (/Game/Matsuri/Tripo/Heroes/<Asset>_<Clip>) and the clip itself. */
	SUZUNEMATSURI_API USkeletalMesh* CharacterMesh(const TCHAR* Asset, const TCHAR* Clip);
	SUZUNEMATSURI_API UAnimSequence* CharacterAnim(const TCHAR* Asset, const TCHAR* Clip);

	/** The heroine's skeletal mesh for one clip ("Idle", "Run", "Jump") and the clip itself. */
	SUZUNEMATSURI_API USkeletalMesh* HeroMesh(EMatsuriHero Hero, const TCHAR* Clip);
	SUZUNEMATSURI_API UAnimSequence* HeroAnim(EMatsuriHero Hero, const TCHAR* Clip);

	/** /Game/Matsuri/Audio/SFX_<Name>: sound effects (tools/audio/build_sfx.py, tools/ue/import_audio.py). */
	SUZUNEMATSURI_API class USoundBase* Sound(const TCHAR* Name);
	/** /Game/Matsuri/Audio/Music/<Name> (tools/ue/import_audio.py), e.g. TEXT("BGM_Title"); nullptr when not imported. */
	SUZUNEMATSURI_API class USoundBase* Music(const TCHAR* Name);

	/** /Game/Matsuri/Tripo/Textures/<Name>: tiling surface textures (tools/tripo/make_tileable.py). */
	SUZUNEMATSURI_API UTexture2D* Texture(const TCHAR* Name);

	/** The rigged heroines come out 1 m tall; this makes them 1.6 m. */
	inline constexpr float HeroineScale = 1.6f;

	/** The heroine's concept-art portrait (hero cards, HUD). */
	SUZUNEMATSURI_API UTexture2D* Portrait(EMatsuriHero Hero);

	/** UV crops of the portraits (all three are drawn with the face at about (0.48, 0.15)). */
	inline const FBox2f PortraitBustUV(FVector2f(0.29f, 0.03f), FVector2f(0.67f, 0.40f));
	inline const FBox2f PortraitFaceUV(FVector2f(0.40f, 0.07f), FVector2f(0.56f, 0.23f));
}
