// Suzune's Night Festival - game mode for a playable floor.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MatsuriMaze.h"
#include "MatsuriRunState.h"
#include "MatsuriFloorGameMode.generated.h"

class ACharacter;
class APlayerStart;
class UUserWidget;
class UMatsuriHUDWidget;
class UMatsuriVictoryWidget;
class UMatsuriGameOverWidget;
class AMatsuriReplayDirector;
struct FMatsuriFloorSpec;
struct FMatsuriFloorTheme;

/**
 *  Builds the current floor of the run (environment, gray-box, coins, ammo,
 *  yokai, hazards), runs the countdown, applies health damage (with brief protection after a hit) and shows victory / game-over screens.
 */
UCLASS()
class SUZUNEMATSURI_API AMatsuriFloorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMatsuriFloorGameMode();

	virtual void Tick(float DeltaSeconds) override;

	/** Applies damage unless protected. True only when HP was actually lost. */
	bool HurtPlayer(const AActor* Source, float Damage = 30.0f, const FText& Cause = FText::GetEmpty(), int32 Skill = -1,
		const FSimpleDelegate& OnDamageAccepted = FSimpleDelegate());

	AMatsuriReplayDirector* GetReplayDirector() const { return ReplayDirector; }

	float GetRemainingSeconds() const { return RemainingSeconds; }
	bool IsFloorActive() const { return bFloorActive; }
	bool IsPlayerInvulnerable() const;

	/** Sakura's 結界: no hits for Seconds (never shortens an existing window). */
	void GrantInvulnerability(float Seconds);

	/** Mio's 千客万来: yokai drop twice for Seconds. */
	void SetDoubleDrops(float Seconds);
	bool IsDoubleDropActive() const;

	/** The floor's maze, or nullptr on an open (test arena) floor. Yokai path along it. */
	const FMatsuriMaze* GetMaze() const { return Maze.IsValid() ? &Maze : nullptr; }
	FVector GetCheckpoint() const;

	/** Developer mode (F5): the floor counts as cleared now. */
	void DevClearFloor() { HandleFloorCleared(); }

	/** Debug / test hook: jump the countdown. */
	UFUNCTION(Exec, BlueprintCallable, Category="Matsuri|Debug")
	void SetRemainingSeconds(float Seconds);

protected:
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category="Matsuri|UI")
	TSubclassOf<UMatsuriHUDWidget> HUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Matsuri|UI")
	TSubclassOf<UMatsuriVictoryWidget> VictoryWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Matsuri|UI")
	TSubclassOf<UMatsuriGameOverWidget> GameOverWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="Matsuri|Rules", meta=(Units="s"))
	float InvulnerableAfterHit = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category="Matsuri|Rules", meta=(Units="s"))
	float InvulnerableAfterBlessing = 1.0f;

	/** Spawns this floor's world and sets FloorStart. Maze floors by default; the boss floor builds its courtyard. */
	virtual void BuildFloorContent(UWorld& World, int32 FloorIndex, const FMatsuriFloorSpec& Spec, const FMatsuriFloorTheme& Theme);

	/** The floor a run starts on when this map is played directly (PIE), not reached through the run. */
	virtual int32 DirectPlayFloor() const { return 0; }

	/** The floor is over: bPlayerWon after a clear, false after a game over. */
	virtual void OnFloorEnded(bool bPlayerWon) {}

	UFUNCTION()
	void HandleFloorCleared();

	UPROPERTY()
	TObjectPtr<APlayerStart> FloorStart;

private:
	/** The heroine's Tripo weapon held in view (no-op without the art). */
	void AttachViewWeapon(ACharacter& Hero, EMatsuriHero Heroine);
	/** Idempotent: players can log in before BeginPlay, so both paths call this. */
	void EnsureFloorBuilt();
	void EndFloorWithGameOver(EMatsuriRunEnd Reason);
	void ShowGameOverScreen();
	void ShowVictoryScreen();
	void ShowModal(UUserWidget* Widget);

	UPROPERTY(Transient)
	TObjectPtr<AMatsuriReplayDirector> ReplayDirector;

	float RemainingSeconds = 0.0f;
	float InvulnerableUntil = 0.0f;
	float DoubleDropsUntil = -1.0f;
	FMatsuriMaze Maze;
	float BaseWalkSpeed = 0.0f;
	bool bFloorBuilt = false;
	bool bFloorActive = false;
	bool bGameOverFinalized = false;
};
