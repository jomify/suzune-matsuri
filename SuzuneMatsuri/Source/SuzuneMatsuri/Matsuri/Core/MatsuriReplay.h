// Suzune's Night Festival - bounded death-replay capture and playback.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameFramework/Actor.h"
#include "MatsuriDanmaku.h"
#include "MatsuriReplay.generated.h"

class ACameraActor;
class AMatsuriBoss;
class AMatsuriDanmakuField;
class APawn;
class UUserWidget;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UMatsuriReplayWidget;

struct FMatsuriReplayActorFrame
{
	uint32 Id = 0;
	FTransform Transform;
};

struct FMatsuriReplaySavedActor
{
	TWeakObjectPtr<AActor> Actor;
	FTransform Transform;
	bool bHidden = false;
	bool bCollision = true;
	uint32 Id = 0;
};

/** One sampled visual state. It contains no gameplay state and is safe to replay after the world pauses. */
struct FMatsuriReplayFrame
{
	float Time = 0.0f;
	FTransform PlayerTransform;
	FTransform BossTransform;
	TArray<FMatsuriBullet> Bullets;
	TArray<FMatsuriReplayActorFrame> Actors;
};

/** Small, deterministic ring buffer used by the runtime director and pure automation tests. */
class SUZUNEMATSURI_API FMatsuriReplayBuffer
{
public:
	static constexpr int32 DefaultMaxFrames = 90; // 6 seconds at 15 Hz

	void Reset();
	void Add(FMatsuriReplayFrame Frame);
	int32 Num() const { return Frames.Num(); }
	int32 MaxFrames() const { return Capacity; }
	float Duration() const;
	const FMatsuriReplayFrame* At(int32 Index) const;
	const FMatsuriReplayFrame* Sample(float Time) const;

	void SetMaxFrames(int32 InCapacity);

private:
	TArray<FMatsuriReplayFrame> Frames;
	int32 Capacity = DefaultMaxFrames;
};

/** Records active gameplay and plays a finite historical sequence after fatal damage. */
UCLASS()
class SUZUNEMATSURI_API AMatsuriReplayDirector : public AActor
{
	GENERATED_BODY()

public:
	AMatsuriReplayDirector();

	/** Starts a replay from the captured history. Returns false when disabled or history is too short. */
	bool Start(const AActor* Source, const FText& Cause, int32 Skill, FSimpleDelegate OnFinished);
	/** Completes a replay once. The callback is never sent twice. */
	void FinishReplay();
	bool IsReplaying() const { return bReplaying; }
	bool IsRecording() const { return bRecording; }
	const FMatsuriReplayBuffer& GetHistory() const { return History; }
	float GetPlaybackProgress() const;
	int32 GetPlaybackFrameIndex() const { return PlaybackFrameIndex; }
	int32 GetPlaybackBulletCount() const { return PlaybackBulletCount; }

	/** CVar gate used by tests and the developer panel. */
	static bool IsEnabled();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void CaptureFrame(bool bForce = false);
	void AdvanceReplay(float DeltaSeconds);
	void ApplyFrame(const FMatsuriReplayFrame& Frame);
	void Complete(bool bNotify);
	void BuildProxy();
	void BuildOverlay(const FText& Cause);
	void SaveVisualActors();
	void EnsureHistoricalActors();
	void ApplyVisualActors(const FMatsuriReplayFrame& Frame);
	void DestroyReplayCamera();

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> ReplayCamera;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> HeroProxy;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HeroFallbackProxy;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> KillerProxy;

	UPROPERTY(Transient)
	TObjectPtr<UMatsuriReplayWidget> Overlay;

	TWeakObjectPtr<AActor> ReplaySource;
	TWeakObjectPtr<AMatsuriBoss> Boss;
	TWeakObjectPtr<AMatsuriDanmakuField> Field;
	TWeakObjectPtr<APawn> Player;
	uint32 ReplaySourceId = 0;

	FMatsuriReplayBuffer History;
	FSimpleDelegate Finished;
	FTSTicker::FDelegateHandle ReplayTicker;
	FTransform SavedPlayerTransform;
	FTransform SavedBossTransform;
	FRotator SavedControlRotation;
	TWeakObjectPtr<AActor> SavedViewTarget;
	bool bSavedPlayerHidden = false;
	bool bSavedPlayerCollision = true;
	bool bSavedBossTransform = false;
	bool bSavedBossHidden = false;
	bool bSavedSourceHidden = false;
	bool bSavedPaused = false;
	bool bRecording = false;
	bool bReplaying = false;
	bool bCompletionSent = false;
	float SampleAccumulator = 0.0f;
	float PlaybackAge = 0.0f;
	float PlaybackDuration = 0.0f;
	int32 PlaybackFrameIndex = 0;
	int32 PlaybackBulletCount = 0;
	int32 ReplaySkill = -1;
	TArray<FMatsuriReplaySavedActor> SavedVisualActors;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SavedActorProxies;
};

