// Suzune's Night Festival - frame-time probe (not part of the SuzuneMatsuri suite: needs a renderer and
// the numbers depend on the machine). Plays 1F as Sakura with live yokai - walking, turning, drawing and
// loosing arrows, striking - and logs where each frame's time goes, like `stat unit`. Twice: the cold
// pass meets every sound, effect and mesh for the first time in this editor session; the warm pass is
// the steady state.
// Run: Automation RunTests MatsuriPerf.Floors   (rendered: -windowed -ResX=2560 -ResY=1440; matsuri.Seed fixes the layout)
// Each pass logs one "[matsuri-perf]" summary line and a "[matsuri-perf] hitch" line per frame over 50 ms;
// the warm pass also dumps one frame's GPU passes (ProfileGPU) to the log.
// MatsuriPerf.Boss does the same during the boss fight (her local learner, no network), with the bullet count.

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "DynamicRHI.h"
#include "RenderTimer.h"
#include "Scalability.h"
#include "Tests/AutomationEditorCommon.h"
#include "MatsuriCombatComponent.h"
#include "MatsuriFloors.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriBossGameMode.h"
#include "MatsuriDanmakuField.h"

using namespace MatsuriTest;

namespace
{
	constexpr int32 SettleFramesBeforeMeasuring = 90;	// the arrival card covers this in play; Lumen converges
	constexpr double MeasureSeconds = 15.0;
	constexpr float HitchMs = 50.0f;
	constexpr float TurnDegreesPerSecond = 25.0f;
	constexpr double DrawSeconds = 0.7;
	constexpr double BowCycleSeconds = 1.0;
	constexpr double StrikeEverySeconds = 4.0;
	constexpr double RepathEverySeconds = 2.0;

	struct FFrameTiming
	{
		float FrameMs, GameMs, RenderMs, RhiMs, GpuMs;
	};

	struct FPerfState
	{
		TArray<FFrameTiming> Frames;
		double StartedAt = 0.0;
		double LastStrike = 0.0;
		double LastRepath = -100.0;
		FVector Heading = FVector::ForwardVector;
		bool bDrawing = false;
		bool bProfiledGpu = false;
		FString Doing;		// what the hero did this frame, printed next to a hitch
	};

	float Percentile(TArray<float> Values, float P)
	{
		Values.Sort();
		return Values.Num() ? Values[FMath::Clamp(FMath::FloorToInt(P * (Values.Num() - 1)), 0, Values.Num() - 1)] : 0.0f;
	}

	float Mean(const TArray<FFrameTiming>& Frames, float FFrameTiming::* Field)
	{
		double Sum = 0.0;
		for (const FFrameTiming& Frame : Frames) { Sum += Frame.*Field; }
		return Frames.Num() ? static_cast<float>(Sum / Frames.Num()) : 0.0f;
	}

	const TCHAR* PassName(int32 Pass)
	{
		return Pass == 0 ? TEXT("cold") : TEXT("warm");
	}

	void Report(const UWorld* World, const FString& Label, const TArray<FFrameTiming>& Frames, const FString& Extra = FString())
	{
		TArray<float> FrameMs;
		int32 Hitches = 0;
		for (const FFrameTiming& Frame : Frames)
		{
			FrameMs.Add(Frame.FrameMs);
			Hitches += Frame.FrameMs > HitchMs ? 1 : 0;
		}
		FIntPoint Size = FIntPoint::ZeroValue;
		if (const UGameViewportClient* Viewport = World->GetGameViewport())
		{
			Size = Viewport->Viewport ? Viewport->Viewport->GetSizeXY() : Size;
		}
		const float AvgMs = Mean(Frames, &FFrameTiming::FrameMs);
		UE_LOG(LogTemp, Warning, TEXT("[matsuri-perf] %s %dx%d frames=%d fps=%.1f frame avg=%.1f p50=%.1f p95=%.1f p99=%.1f max=%.1f | game=%.1f render=%.1f rhi=%.1f gpu=%.1f | hitches>%.0fms=%d%s"),
			*Label, Size.X, Size.Y, Frames.Num(), AvgMs > 0.0f ? 1000.0f / AvgMs : 0.0f, AvgMs,
			Percentile(FrameMs, 0.5f), Percentile(FrameMs, 0.95f), Percentile(FrameMs, 0.99f), Percentile(FrameMs, 1.0f),
			Mean(Frames, &FFrameTiming::GameMs), Mean(Frames, &FFrameTiming::RenderMs), Mean(Frames, &FFrameTiming::RhiMs), Mean(Frames, &FFrameTiming::GpuMs),
			HitchMs, Hitches, *Extra);
	}

	FFrameTiming LastFrameTiming()
	{
		// The timings the engine published for the frame that just finished (what `stat unit` shows).
		return {
			static_cast<float>(FApp::GetDeltaTime() * 1000.0),
			static_cast<float>(FPlatformTime::ToMilliseconds(GGameThreadTime)),
			static_cast<float>(FPlatformTime::ToMilliseconds(GRenderThreadTime)),
			static_cast<float>(FPlatformTime::ToMilliseconds(GRHIThreadTime)),
			static_cast<float>(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0))) };
	}

	constexpr double SlowCallMs = 5.0;

	/** Runs Call and logs it when it alone takes longer than SlowCallMs (finds which action a hitch belongs to). */
	template <typename TCall>
	void Timed(const TCHAR* What, TCall&& Call)
	{
		const double Start = FPlatformTime::Seconds();
		Call();
		const double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
		if (Ms > SlowCallMs)
		{
			UE_LOG(LogTemp, Warning, TEXT("[matsuri-perf] slow call %s %.1f ms"), What, Ms);
		}
	}

	/** One frame of play: turn, walk where it is clear, loose an arrow each second, strike now and then. */
	void Play(FPerfState& State, UWorld* World, double Now)
	{
		ACharacter* Pawn = Hero(World);
		UMatsuriCombatComponent* Combat = Pawn->FindComponentByClass<UMatsuriCombatComponent>();
		APlayerController* PC = World->GetFirstPlayerController();
		State.Doing.Reset();

		FRotator View = PC->GetControlRotation();
		View.Yaw += TurnDegreesPerSecond * World->GetDeltaSeconds();
		View.Pitch = -4.0f;
		PC->SetControlRotation(View);

		if (Now - State.LastRepath > RepathEverySeconds)
		{
			State.Heading = ClearDirection(World, 400.0f);
			State.LastRepath = Now;
		}
		Pawn->AddMovementInput(State.Heading, 1.0f);

		const double InCycle = FMath::Fmod(Now - State.StartedAt, BowCycleSeconds);
		if (!State.bDrawing && InCycle < DrawSeconds)
		{
			Timed(TEXT("draw"), [Combat] { Combat->StartFiring(); });
			State.bDrawing = true;
			State.Doing = TEXT("draw");
		}
		else if (State.bDrawing && InCycle >= DrawSeconds)
		{
			Timed(TEXT("loose"), [Combat] { Combat->StopFiring(); });
			State.bDrawing = false;
			State.Doing = TEXT("loose");
			Run(World)->AddAmmo(1);
		}
		if (Now - State.LastStrike > StrikeEverySeconds)
		{
			Timed(TEXT("strike"), [Combat] { Combat->TryMelee(); });
			State.LastStrike = Now;
			State.Doing += TEXT(" strike");
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriPerfProbe, "MatsuriPerf.Floors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriPerfProbe::RunTest(const FString&)
{
	SetCVar(TEXT("matsuri.God"), true);
	SetIntCVar(TEXT("matsuri.Hero"), static_cast<int32>(EMatsuriHero::Sakura));
	constexpr int32 Passes = 2;
	for (int32 Pass = 0; Pass < Passes; ++Pass)
	{
		TSharedRef<FPerfState> State = MakeShared<FPerfState>();
		TArray<TPair<FString, FStepRunner::FStep>> Steps;
		Steps.Add({ TEXT("floor loads"), [State](UWorld* World, int32 Frame)
		{
			if (!Hero(World) || !FindWidget<UMatsuriHUDWidget>(World))
			{
				return EStep::Wait;
			}
			if (Frame == 0)
			{
				Run(World)->UseRecordSlot(TestRecordSlot);
			}
			return DoneIf(Frame > SettleFramesBeforeMeasuring);
		} });
		Steps.Add({ TEXT("play and measure"), [this, State, Pass](UWorld* World, int32 Frame)
		{
			const double Now = World->GetTimeSeconds();
			if (Frame == 0)
			{
				if (Pass == 1 && FApp::CanEverRender())
				{
					// The same view every run with a fixed matsuri.Seed: compare quality levels side by side.
					const FString Name = FString::Printf(TEXT("70_perf_view_gi%d"), Scalability::GetQualityLevels().GlobalIlluminationQuality);
					FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Automation/MatsuriShots") / (Name + TEXT(".png")), true, false);
				}
				State->StartedAt = Now;
				State->LastStrike = Now;
				State->Frames.Reserve(2000);
				return EStep::Continue;
			}
			const FFrameTiming Last = LastFrameTiming();
			State->Frames.Add(Last);
			if (Last.FrameMs > HitchMs)
			{
				UE_LOG(LogTemp, Warning, TEXT("[matsuri-perf] hitch %s t=%.2f frame=%.0f game=%.0f render=%.0f gpu=%.0f after '%s'"),
					PassName(Pass), Now - State->StartedAt, Last.FrameMs, Last.GameMs, Last.RenderMs, Last.GpuMs, *State->Doing);
			}
			if (Pass == 1 && !State->bProfiledGpu && Now - State->StartedAt > MeasureSeconds * 0.5)
			{
				GEngine->Exec(World, TEXT("ProfileGPU"));	// one frame's GPU passes, written to the log
				State->bProfiledGpu = true;
			}
			if (Now - State->StartedAt < MeasureSeconds)
			{
				Play(*State, World, Now);
				return EStep::Continue;
			}
			Hero(World)->FindComponentByClass<UMatsuriCombatComponent>()->StopFiring();
			Report(World, FString::Printf(TEXT("1F %s"), PassName(Pass)), State->Frames);
			TestTrue(TEXT("measured some frames"), State->Frames.Num() > 30);
			return EStep::Done;
		} });
		ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
		ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	}
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] { SetCVar(TEXT("matsuri.God"), false); }, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriBossPerfProbe, "MatsuriPerf.Boss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriBossPerfProbe::RunTest(const FString&)
{
	constexpr double BossMeasureSeconds = 30.0;
	TSharedRef<FPerfState> State = MakeShared<FPerfState>();
	TSharedRef<int32> MostBullets = MakeShared<int32>(0);
	TArray<TPair<FString, FStepRunner::FStep>> Steps;
	Steps.Add({ TEXT("courtyard loads"), [](UWorld* World, int32 Frame)
	{
		const AMatsuriBossGameMode* Mode = World->GetAuthGameMode<AMatsuriBossGameMode>();
		if (!Hero(World) || !Mode || !Mode->GetBoss())
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			Run(World)->UseRecordSlot(TestRecordSlot);
		}
		return DoneIf(Frame > SettleFramesBeforeMeasuring);
	} });
	Steps.Add({ TEXT("fight and measure"), [this, State, MostBullets](UWorld* World, int32 Frame)
	{
		const double Now = World->GetTimeSeconds();
		const AMatsuriDanmakuField* Field = World->GetAuthGameMode<AMatsuriBossGameMode>()->GetField();
		if (Frame == 0)
		{
			State->StartedAt = Now;
			State->LastStrike = Now;
			return EStep::Continue;
		}
		const FFrameTiming Last = LastFrameTiming();
		State->Frames.Add(Last);
		*MostBullets = FMath::Max(*MostBullets, Field->Num());
		if (Last.FrameMs > HitchMs)
		{
			UE_LOG(LogTemp, Warning, TEXT("[matsuri-perf] hitch boss t=%.2f frame=%.0f game=%.0f render=%.0f gpu=%.0f bullets=%d after '%s'"),
				Now - State->StartedAt, Last.FrameMs, Last.GameMs, Last.RenderMs, Last.GpuMs, Field->Num(), *State->Doing);
		}
		if (Now - State->StartedAt < BossMeasureSeconds)
		{
			Play(*State, World, Now);
			return EStep::Continue;
		}
		Hero(World)->FindComponentByClass<UMatsuriCombatComponent>()->StopFiring();
		Report(World, TEXT("boss"), State->Frames, FString::Printf(TEXT(" | bullets max=%d"), *MostBullets));
		TestTrue(TEXT("measured some frames"), State->Frames.Num() > 30);
		return EStep::Done;
	} });
	UseTestBossBrain();
	SetCVar(TEXT("matsuri.God"), true);
	SetIntCVar(TEXT("matsuri.Hero"), static_cast<int32>(EMatsuriHero::Sakura));
	SetStringCVar(TEXT("matsuri.BossHealth"), TEXT("100000"));	// she lives through the measurement
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(Matsuri::Floors::BossFloor)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] {
		SetCVar(TEXT("matsuri.God"), false);
		SetStringCVar(TEXT("matsuri.BossHealth"), TEXT("150"));
		RestoreBossBrain();
	}, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
