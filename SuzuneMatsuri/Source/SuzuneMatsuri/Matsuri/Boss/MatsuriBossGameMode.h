// Suzune's Night Festival - game mode for 本殿・月下, the boss floor.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriFloorGameMode.h"
#include "MatsuriBossArena.h"
#include "MatsuriBossGameMode.generated.h"

class AMatsuriBoss;
class AMatsuriDanmakuField;
class UMatsuriBossBarWidget;
struct FMatsuriFloorSpec;
struct FMatsuriFloorTheme;

/**
 *  Everything a floor has (HP, death replay, the clock, the HUD, victory and game-over screens), but the
 *  world is the fixed courtyard and the floor is cleared by defeating Tamamo, not by a score. The
 *  heroine gets the grapple here.
 */
UCLASS()
class SUZUNEMATSURI_API AMatsuriBossGameMode : public AMatsuriFloorGameMode
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;

	AMatsuriBoss* GetBoss() const { return Boss; }
	AMatsuriDanmakuField* GetField() const { return Field; }
	const FMatsuriBossArena& GetArena() const { return Arena; }
	UMatsuriBossBarWidget* GetBossBar() const { return BossBar; }

	/** Tamamo starts fighting (BeginPlay does this unless matsuri.Threats is 0; tests call it themselves). */
	void StartBossFight();
	bool HasBossFightStarted() const { return bBossStarted; }

protected:
	virtual void BuildFloorContent(UWorld& World, int32 FloorIndex, const FMatsuriFloorSpec& Spec, const FMatsuriFloorTheme& Theme) override;
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;
	virtual void BeginPlay() override;
	virtual void OnFloorEnded(bool bPlayerWon) override;
	virtual int32 DirectPlayFloor() const override;

private:
	void HandleBossDefeated();

	UPROPERTY(Transient)
	TObjectPtr<AMatsuriBoss> Boss;

	UPROPERTY(Transient)
	TObjectPtr<AMatsuriDanmakuField> Field;

	UPROPERTY(Transient)
	TObjectPtr<UMatsuriBossBarWidget> BossBar;

	FMatsuriBossArena Arena;
	bool bBossStarted = false;
};
