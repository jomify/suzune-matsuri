// Suzune's Night Festival - helpers for PIE-driven end-to-end tests.
// A test is a list of named steps. Each step is polled once per frame and answers Wait (not ready yet,
// frame counter untouched), Continue (working, frame counter advances) or Done (move to the next step).

#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "MatsuriCollectible.h"
#include "MatsuriDanmakuField.h"
#include "MatsuriFx.h"
#include "MatsuriRunSubsystem.h"

namespace MatsuriTest
{
	constexpr double StepTimeoutSeconds = 40.0;
	constexpr int32 SettleFrames = 90;		// let Lumen / sky capture converge before a screenshot
	constexpr int32 CaptureFrames = 10;		// screenshots are written a frame or more after the request
	inline const TCHAR* TestRecordSlot = TEXT("MatsuriRecord_Test");

	enum class EStep : uint8
	{
		Wait,		// preconditions not met yet (level loading, widget not shown); Frame stays at 0
		Continue,	// running; Frame counts these calls
		Done
	};

	inline EStep DoneIf(bool bDone)
	{
		return bDone ? EStep::Done : EStep::Continue;
	}

	inline UWorld* PIEWorld()
	{
		return GEditor ? GEditor->PlayWorld.Get() : nullptr;
	}

	inline bool IsMap(const UWorld* World, const TCHAR* MapPath)
	{
		return World && World->GetOutermost()->GetName().EndsWith(FPackageName::GetShortName(MapPath));
	}

	template <typename TWidget>
	TWidget* FindWidget(const UWorld* World)
	{
		for (TObjectIterator<TWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsInViewport())
			{
				return *It;
			}
		}
		return nullptr;
	}

	inline UButton* FindButton(UUserWidget& Widget, const FString& Label)
	{
		UButton* Found = nullptr;
		Widget.WidgetTree->ForEachWidget([&Found, &Label](UWidget* Child)
		{
			UButton* Button = Cast<UButton>(Child);
			const UTextBlock* Text = Button ? Cast<UTextBlock>(Button->GetChildAt(0)) : nullptr;
			if (Text && Text->GetText().ToString() == Label)
			{
				Found = Button;
			}
		});
		return Found;
	}

	inline ACharacter* Hero(const UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	}

	inline UMatsuriRunSubsystem* Run(const UWorld* World)
	{
		return World ? UMatsuriRunSubsystem::Get(World) : nullptr;
	}

	inline void TeleportHero(UWorld* World, const FVector& Location)
	{
		Hero(World)->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		for (TActorIterator<AMatsuriDanmakuField> Field(World); Field; ++Field)
		{
			Field->ResetPlayerSweep();
		}
	}

	/**
	 *  A flat direction from the hero with nothing solid for Distance: the arena's crates are random per
	 *  run, so steps that need open ground (a long shot, a knockback) look for it instead of assuming +X.
	 */
	inline FVector ClearDirection(UWorld* World, float Distance)
	{
		// A 55 cm sphere centred 75 cm up: clear of the ground slab (touching it would block every direction),
		// low enough to meet waist-high crates.
		const FVector From = Hero(World)->GetActorLocation() * FVector(1, 1, 0) + FVector(0, 0, 75.0f);
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Hero(World));
		for (int32 Step = 0; Step < 24; ++Step)
		{
			const FVector Dir = FRotator(0.0f, Step * 15.0f, 0.0f).Vector();
			if (!World->SweepTestByChannel(From, From + Dir * Distance, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(55.0f), Params))
			{
				return Dir;
			}
		}
		return FVector::ForwardVector;
	}

	/** Points the player's view at Target. The camera (which shots aim with) follows on the next frame. */
	inline void AimAt(UWorld* World, const FVector& Target)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		FVector Eye;
		FRotator Ignored;
		PC->GetPlayerViewPoint(Eye, Ignored);
		PC->SetControlRotation((Target - Eye).Rotation());
	}

	inline int32 CountFx(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<AMatsuriFx> It(World); It; ++It) { Count += It->IsActive() ? 1 : 0; }	// not the pooled ones
		return Count;
	}

	inline int32 CountCollectibles(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<AMatsuriCollectible> It(World); It; ++It) { ++Count; }
		return Count;
	}

	/** Waits SettleFrames, asks for a screenshot (renderer only), then holds CaptureFrames. True once done. */
	inline bool SettleAndShoot(int32 Frame, const TCHAR* ShotName, int32 Settle = SettleFrames)
	{
		if (Frame == Settle && FApp::CanEverRender())
		{
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("Automation/MatsuriShots");
			IFileManager::Get().MakeDirectory(*Dir, true);
			FScreenshotRequest::RequestScreenshot(Dir / FString(ShotName) + TEXT(".png"), true, false);
		}
		return Frame >= Settle + CaptureFrames;
	}

	/**
	 *  Sends a key event through the game viewport, the same entry point real keyboard / mouse
	 *  input uses. Returns false when the viewport refused it (e.g. stuck in UI-only input mode).
	 */
	inline bool SendKey(UWorld* World, const FKey& Key, EInputEvent Event)
	{
		UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
		if (!Viewport)
		{
			return false;
		}
		const FInputDeviceId Device = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
		return Viewport->InputKey(FInputKeyEventArgs(Viewport->Viewport, Device, Key, Event, 1.0f, false, FPlatformTime::Cycles64()));
	}

	inline bool ViewportAcceptsGameInput(UWorld* World)
	{
		UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
		return Viewport && !Viewport->IgnoreInput();
	}

	inline void SetIntCVar(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByCode);
		}
	}

	inline void SetStringCVar(const TCHAR* Name, const FString& Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(*Value, ECVF_SetByCode);
		}
	}

	/**
	 *  The boss decides with her local learner only (no network, no cost) and remembers into a test folder,
	 *  never the player's Saved/BossMemory. RestoreBossBrain puts the defaults back.
	 */
	inline FString TestBossMemoryFolder()
	{
		return FPaths::ProjectSavedDir() / TEXT("BossMemory_Test");
	}

	inline void UseTestBossBrain()
	{
		SetStringCVar(TEXT("matsuri.BossLLM"), TEXT("off"));
		SetStringCVar(TEXT("matsuri.BossMemory"), TestBossMemoryFolder());
	}

	inline void RestoreBossBrain()
	{
		SetStringCVar(TEXT("matsuri.BossLLM"), TEXT("deepseek"));
		SetStringCVar(TEXT("matsuri.BossMemory"), TEXT(""));
	}

	inline void SetCVar(const TCHAR* Name, bool bValue)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(bValue, ECVF_SetByCode);
		}
	}

	/**
	 *  Collects coins one at a time by teleporting onto them, checking that each pickup is
	 *  destroyed and adds exactly its value. Returns true once the floor is cleared.
	 */
	struct FCoinCollector
	{
		TWeakObjectPtr<AMatsuriCollectible> Target;
		EMatsuriCollectible Kind = EMatsuriCollectible::Koban;
		int32 ScoreBefore = 0;
		int32 Collected = 0;

		bool Tick(FAutomationTestBase& Test, UWorld* World)
		{
			UMatsuriRunSubsystem* RunState = Run(World);
			if (Target.IsValid())
			{
				return false;	// overlap not processed yet; the step timeout guards a coin that never triggers
			}
			if (Collected > 0)
			{
				const int32 Expected = ScoreBefore + RunState->GetRunState().PointsFor(Kind);
				Test.TestEqual(TEXT("coin destroyed and score rises by its value"), RunState->GetRunState().Score.RunScore, Expected);
			}
			if (RunState->GetScoreState().bFloorCleared)
			{
				return true;
			}
			TActorIterator<AMatsuriCollectible> It(World);
			if (!It)
			{
				Test.AddError(TEXT("Ran out of coins before reaching the target"));
				return true;
			}
			Target = *It;
			Kind = It->GetKind();
			ScoreBefore = RunState->GetRunState().Score.RunScore;
			++Collected;
			const FVector Spot = It->GetActorLocation();
			TeleportHero(World, FVector(Spot.X, Spot.Y, 110.0f));
			return false;
		}
	};

	/** Polls named steps in order, one call per frame, each with its own timeout. */
	class FStepRunner : public IAutomationLatentCommand
	{
	public:
		/** Frame = number of earlier calls that returned Continue for this step. */
		using FStep = TFunction<EStep(UWorld* World, int32 Frame)>;

		FStepRunner(FAutomationTestBase* InTest, TArray<TPair<FString, FStep>> InSteps,
			bool bStoryEnabled = false, bool bReplayEnabled = false, bool bUltimateEnabled = false)
			: Test(InTest), Steps(MoveTemp(InSteps))
		{
			if (ActiveRunners++ == 0)
			{
				if (const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("matsuri.Story"))) { SavedStory = Var->GetInt(); }
				if (const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("matsuri.Replay"))) { SavedReplay = Var->GetInt(); }
				if (const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("matsuri.UltimateCinematic"))) { SavedUltimate = Var->GetInt(); }
			}
			SetCVar(TEXT("matsuri.Story"), bStoryEnabled);
			SetCVar(TEXT("matsuri.Replay"), bReplayEnabled);
			SetCVar(TEXT("matsuri.UltimateCinematic"), bUltimateEnabled);
		}

		virtual ~FStepRunner() override
		{
			if (--ActiveRunners == 0)
			{
				SetIntCVar(TEXT("matsuri.Story"), SavedStory);
				SetIntCVar(TEXT("matsuri.Replay"), SavedReplay);
				SetIntCVar(TEXT("matsuri.UltimateCinematic"), SavedUltimate);
			}
		}

		virtual bool Update() override
		{
			if (!Steps.IsValidIndex(Index))
			{
				return true;
			}
			if (FPlatformTime::Seconds() - StepStart > StepTimeoutSeconds)
			{
				Test->AddError(FString::Printf(TEXT("Timed out in step '%s'"), *Steps[Index].Key));
				return true;
			}
			UWorld* World = PIEWorld();
			if (bSawWorld && (!World || World->bIsTearingDown))
			{
				// PIE ended under us (e.g. someone pressed Esc in the visible PIE window): fail, never touch a dying world.
				Test->AddError(FString::Printf(TEXT("PIE session ended unexpectedly during step '%s'"), *Steps[Index].Key));
				return true;
			}
			if (!World || World->bIsTearingDown)
			{
				return false;	// PIE still starting
			}
			bSawWorld = true;
			const EStep Result = Steps[Index].Value(World, Frame);
			if (Result == EStep::Continue)
			{
				++Frame;
			}
			else if (Result == EStep::Done)
			{
				++Index;
				Frame = 0;
				StepStart = FPlatformTime::Seconds();
			}
			return !Steps.IsValidIndex(Index);
		}

	private:
		FAutomationTestBase* Test;
		TArray<TPair<FString, FStep>> Steps;
		int32 Index = 0;
		int32 Frame = 0;
		double StepStart = FPlatformTime::Seconds();
		bool bSawWorld = false;
		inline static int32 ActiveRunners = 0;
		inline static int32 SavedStory = 1;
		inline static int32 SavedReplay = 1;
		inline static int32 SavedUltimate = 1;
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
