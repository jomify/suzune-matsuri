// Suzune's Night Festival - background music: a track per scene, crossfaded and carried across level loads.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MatsuriMusic.generated.h"

class UAudioComponent;
class USoundBase;

UENUM()
enum class EMatsuriMusic : uint8
{
	None,
	Title,
	Floor1,			// 竹林の参道
	Floor2,			// 夏祭りの夜店街
	Floor3,			// 千本鳥居
	BossOpening,	// her first two health bars
	BossFinal,		// 本気: her last bar
	Story,			// under the story cutscenes (the movies are silent)
	Ending			// victory: the cutscene and the results
};

UENUM()
enum class EMatsuriSting : uint8
{
	Clear,
	Defeat
};

/**
 *  Owns the playing track. It belongs to the game instance, so a track keeps playing through a level load
 *  and crossfades into the next scene's; it plays as UI sound, so a paused menu does not cut it. Like the
 *  art, music is optional: a track that is not imported is silence, but the scene's choice is still kept
 *  (GetCurrent), so tests can check it without an audio device.
 */
UCLASS()
class SUZUNEMATSURI_API UMatsuriMusicSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UMatsuriMusicSubsystem* Get(const UObject* WorldContext);

	/** Crossfades to Track (None fades the music out). Asking for the track already playing changes nothing. */
	void Play(EMatsuriMusic Track);

	/** Fades the music out and plays Sting once. */
	void PlaySting(EMatsuriSting InSting);

	EMatsuriMusic GetCurrent() const { return Current; }

	/** The sting played since the last track began, if any. */
	TOptional<EMatsuriSting> GetSting() const { return Sting; }

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	UAudioComponent* Start(USoundBase* Sound, float Volume, float FadeInSeconds);
	static void FadeAway(UAudioComponent* Component, float Seconds);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> TrackComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> StingComponent;

	/** Every track and sting, loaded up front and kept: switching tracks mid-fight never reads the disk. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> Loaded;

	EMatsuriMusic Current = EMatsuriMusic::None;
	TOptional<EMatsuriSting> Sting;
};

namespace Matsuri::Music
{
	/** The asset under /Game/Matsuri/Audio/Music, e.g. "BGM_Title" (empty for None). */
	SUZUNEMATSURI_API const TCHAR* AssetName(EMatsuriMusic Track);
	SUZUNEMATSURI_API const TCHAR* AssetName(EMatsuriSting Sting);

	/** The imported sound, or nullptr. */
	SUZUNEMATSURI_API USoundBase* Sound(EMatsuriMusic Track);
	SUZUNEMATSURI_API USoundBase* Sound(EMatsuriSting Sting);

	/** The track of floor FloorIndex (0-2 the mazes, 3 the boss). */
	SUZUNEMATSURI_API EMatsuriMusic ForFloor(int32 FloorIndex);

	/** The boss's track for her health bar Phase (1-3): the final one gets its own. */
	SUZUNEMATSURI_API EMatsuriMusic ForBossPhase(int32 Phase);

	/** Shorthands through the game instance's subsystem (no-ops without one). */
	SUZUNEMATSURI_API void Play(const UObject* WorldContext, EMatsuriMusic Track);
	SUZUNEMATSURI_API void PlaySting(const UObject* WorldContext, EMatsuriSting Sting);
	SUZUNEMATSURI_API EMatsuriMusic Current(const UObject* WorldContext);
}
