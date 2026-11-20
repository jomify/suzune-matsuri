// Suzune's Night Festival - run state that survives level loads, plus the saved record.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MatsuriFloorLayout.h"
#include "MatsuriRecord.h"
#include "MatsuriRunState.h"
#include "MatsuriRunSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMatsuriRunChangedSignature, const FMatsuriRunState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMatsuriFloorClearedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMatsuriPlayerHurtSignature, EMatsuriHurtResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMatsuriRunEndedSignature, EMatsuriRunEnd, Reason);

/**
 *  Owns the current run (score, health, ammo, charms, floor) and the persistent
 *  high-score record. Health lives on the GameInstance so it carries across floor
 *  loads; the record is loaded automatically when the game launches.
 */
UCLASS()
class SUZUNEMATSURI_API UMatsuriRunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Broadcast after any change to score, health or ammo. */
	UPROPERTY(BlueprintAssignable, Category="Matsuri")
	FMatsuriRunChangedSignature OnRunChanged;

	/** Broadcast once, when the floor target is reached. */
	UPROPERTY(BlueprintAssignable, Category="Matsuri")
	FMatsuriFloorClearedSignature OnFloorCleared;

	UPROPERTY(BlueprintAssignable, Category="Matsuri")
	FMatsuriPlayerHurtSignature OnPlayerHurt;

	UPROPERTY(BlueprintAssignable, Category="Matsuri")
	FMatsuriRunEndedSignature OnRunEnded;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Starts a fresh run on floor 1 with the chosen heroine. Seed 0 picks a random seed. */
	UFUNCTION(BlueprintCallable, Category="Matsuri")
	void StartNewRun(EMatsuriHero Hero, int32 Seed = 0);

	/** Heroine for a floor opened directly (PIE on a floor map): the matsuri.Hero cvar. */
	static EMatsuriHero DefaultHero();

	/** matsuri.Seed: the seed for a floor played directly (0 = random). */
	static int32 DefaultSeed();

	/** Moves a run started on a map played directly to that map's floor (the boss map in PIE). */
	void StartOnFloor(int32 FloorIndex);

	/** Resets the floor counters for the current floor. */
	UFUNCTION(BlueprintCallable, Category="Matsuri")
	void BeginFloor();

	FMatsuriFloorSpec GetCurrentFloorSpec() const;

	UFUNCTION(BlueprintCallable, Category="Matsuri")
	void Collect(EMatsuriCollectible Kind);

	UFUNCTION(BlueprintCallable, Category="Matsuri")
	EMatsuriHurtResult HurtPlayer(float Damage = 30.0f);

	UFUNCTION(BlueprintCallable, Category="Matsuri")
	float HealPlayer(float Amount);

	UFUNCTION(BlueprintCallable, Category="Matsuri")
	int32 AddAmmo(int32 Amount);

	UFUNCTION(BlueprintCallable, Category="Matsuri")
	bool ConsumeAmmo(int32 Amount);

	/** Damage the player dealt; charges the ultimate. */
	void AddDamageDealt(float Damage);

	/** Charges the ultimate directly (tests / debug). */
	void AddUltCharge(float Amount);

	bool ConsumeUlt();

	bool SpendScore(int32 Amount);

	/** Debug / test hook: change the current floor's target without touching the score. */
	void SetFloorTargetForTesting(int32 Target);

	TArray<EMatsuriOmamori> GetOmamoriOffer() const;

	/** Takes the chosen charm and moves the run to the next floor index. */
	void ChooseOmamoriAndAdvance(EMatsuriOmamori Charm);

	/** Ends the run once: updates and saves the record. Further calls are ignored. */
	void EndRun(EMatsuriRunEnd Reason);

	const FMatsuriRunState& GetRunState() const { return Run; }
	const FMatsuriScoreState& GetScoreState() const { return Run.Score; }
	const FMatsuriRecord& GetRecord() const { return Record; }
	EMatsuriRunEnd GetLastRunEnd() const { return LastRunEnd; }
	bool WasNewBest() const { return bLastRunWasNewBest; }
	bool IsRunActive() const { return bRunActive; }

	/** Switches the save slot (tests use their own so they never touch the player's record). */
	void UseRecordSlot(const FString& SlotName);

	static UMatsuriRunSubsystem* Get(const UObject* WorldContext);

private:
	void Broadcast();
	void LoadRecord();
	void SaveRecord() const;

	FMatsuriRunState Run;
	FMatsuriRecord Record;
	FString RecordSlot = TEXT("MatsuriRecord");
	EMatsuriRunEnd LastRunEnd = EMatsuriRunEnd::None;
	bool bRunActive = false;
	bool bLastRunWasNewBest = false;
};
