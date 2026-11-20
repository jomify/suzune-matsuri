// Cinematic media failure and skip must never strand the player in a paused state.

#include "MatsuriTestKit.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "MatsuriCinematicWidget.h"
#include "MatsuriFloors.h"
#include "MatsuriHUDWidget.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Tests/AutomationEditorCommon.h"

using namespace MatsuriTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriCinematicFailureE2ETest, "SuzuneMatsuri.E2E.CinematicFailureAndSkip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriCinematicFailureE2ETest::RunTest(const FString&)
{
	// WMF can log an error for the deliberately corrupt fixture before the
	// timeout fires. The assertions below still require correct recovery.
	AddExpectedError(TEXT("Failed to resolve byte stream"), EAutomationExpectedErrorFlags::Contains, -1, false);
	struct FState
	{
		int32 CallbackCount = 0;
		int32 Phase = 0;
		FString Path;
	};
	TSharedRef<FState> State = MakeShared<FState>();
	TArray<TPair<FString, FStepRunner::FStep>> Steps;
	Steps.Add({ TEXT("bad movie fails or can be skipped"), [this, State](UWorld* World, int32 Frame)
	{
		if (!World || !Hero(World) || !FindWidget<UMatsuriHUDWidget>(World))
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			Run(World)->UseRecordSlot(TestRecordSlot);
			State->Path = FPaths::ProjectSavedDir() / TEXT("Automation/CinematicTests") /
				FString::Printf(TEXT("corrupt_%s.mp4"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
			TestFalse(TEXT("missing movie immediately falls back"), UMatsuriCinematicWidget::TryPlayPath(
				*World->GetFirstPlayerController(), State->Path,
				FSimpleDelegate::CreateLambda([State]() { ++State->CallbackCount; })));
			TestEqual(TEXT("missing movie does not invoke the asynchronous callback"), State->CallbackCount, 0);
			TestFalse(TEXT("missing movie does not pause the world"), World->IsPaused());
			if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(TEXT("matsuri.CinematicOpenTimeout")))
			{
				Timeout->Set(0.1f, ECVF_SetByCode);
			}
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(State->Path), true);
			const TArray<uint8> InvalidBytes = { 0x00, 0x01, 0x02, 0x03 };
			TestTrue(TEXT("test-only corrupt movie is written"), FFileHelper::SaveArrayToFile(InvalidBytes, *State->Path));
			TestTrue(TEXT("cinematic starts for an existing file"), UMatsuriCinematicWidget::TryPlayPath(*World->GetFirstPlayerController(), State->Path,
				FSimpleDelegate::CreateLambda([State]() { ++State->CallbackCount; })));
			return EStep::Continue;
		}
		if (State->Phase == 0 && State->CallbackCount == 1 && Frame > 2)
		{
			if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(TEXT("matsuri.CinematicOpenTimeout")))
			{
				Timeout->Set(10.0f, ECVF_SetByCode);
			}
			TestTrue(TEXT("a second playback can be started for skip coverage"), UMatsuriCinematicWidget::TryPlayPath(*World->GetFirstPlayerController(), State->Path,
				FSimpleDelegate::CreateLambda([State]() { ++State->CallbackCount; })));
			State->Phase = 1;
			return EStep::Continue;
		}
		if (State->Phase == 1 && Frame > 8)
		{
			if (UMatsuriCinematicWidget* Widget = FindWidget<UMatsuriCinematicWidget>(World))
			{
				Widget->Finish();
			}
		}
		if (State->Phase == 1 && Frame > 12)
		{
			TestEqual(TEXT("failure and skip each notify once"), State->CallbackCount, 2);
			TestFalse(TEXT("failure or skip does not leave the world paused"), World->IsPaused());
			IFileManager::Get().Delete(*State->Path, false, true, true);
			if (IConsoleVariable* Timeout = IConsoleManager::Get().FindConsoleVariable(TEXT("matsuri.CinematicOpenTimeout")))
			{
				Timeout->Set(10.0f, ECVF_SetByCode);
			}
			return EStep::Done;
		}
		return EStep::Continue;
	} });

	SetCVar(TEXT("matsuri.Threats"), false);
	SetCVar(TEXT("matsuri.Story"), false);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([]
	{
		SetCVar(TEXT("matsuri.Threats"), true);
		SetCVar(TEXT("matsuri.Story"), true);
	}, 0.0f));
	return true;
}

// Exercise an actual generated movie in a rendered editor, including the real Skip button.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriCinematicPlaybackE2ETest, "SuzuneMatsuri.E2E.CinematicPlayback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriCinematicPlaybackE2ETest::RunTest(const FString&)
{
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("Rendered playback is verified in the separate media test run."));
		return true;
	}
	struct FPlaybackState { int32 Callbacks = 0; int32 OpenFrame = -1; bool bStarted = false; };
	TSharedRef<FPlaybackState> State = MakeShared<FPlaybackState>();
	TArray<TPair<FString, FStepRunner::FStep>> Steps;
	Steps.Add({ TEXT("play and skip a real movie"), [this, State](UWorld* World, int32 Frame)
	{
		if (!Hero(World) || !FindWidget<UMatsuriHUDWidget>(World)) { return EStep::Wait; }
		if (!State->bStarted)
		{
			Run(World)->UseRecordSlot(TestRecordSlot);
			const FString Path = UMatsuriCinematicWidget::AbsoluteMoviePath(EMatsuriCinematic::FirstFloor);
			if (!FPaths::FileExists(Path)) { AddError(TEXT("First-floor cinematic asset is missing")); return EStep::Done; }
			State->bStarted = true;
			if (!UMatsuriCinematicWidget::TryPlay(*World->GetFirstPlayerController(), EMatsuriCinematic::FirstFloor,
				FSimpleDelegate::CreateLambda([State]() { ++State->Callbacks; })))
			{
				AddError(TEXT("The real cinematic did not start")); return EStep::Done;
			}
			return EStep::Continue;
		}
		UMatsuriCinematicWidget* Widget = FindWidget<UMatsuriCinematicWidget>(World);
		if (!Widget)
		{
			AddError(TEXT("Real cinematic closed or failed before Skip")); return EStep::Done;
		}
		if (!Widget->HasMediaOpened()) { return EStep::Continue; }
		if (State->OpenFrame < 0) { State->OpenFrame = Frame; }
		TestTrue(TEXT("world remains paused while the video plays"), World->IsPaused());
		if (!SettleAndShoot(Frame - State->OpenFrame, TEXT("cinematic_intro"), 15)) { return EStep::Continue; }
		UButton* Skip = FindButton(*Widget, TEXT("跳过  Skip"));
		if (!Skip) { AddError(TEXT("The cinematic Skip button is missing")); return EStep::Done; }
		Skip->OnClicked.Broadcast();
		TestEqual(TEXT("Skip completes exactly once"), State->Callbacks, 1);
		TestFalse(TEXT("Skip resumes the world"), World->IsPaused());
		return EStep::Done;
	} });
	SetCVar(TEXT("matsuri.Threats"), false);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] { SetCVar(TEXT("matsuri.Threats"), true); }, 0.0f));
	return true;
}

// Each generated file must open and its real skip control must release the game.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriCinematicFiveMoviesE2ETest, "SuzuneMatsuri.E2E.CinematicFiveMovies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriCinematicFiveMoviesE2ETest::RunTest(const FString&)
{
	if (!FApp::CanEverRender()) { AddInfo(TEXT("Five movie playback is verified with a renderer.")); return true; }
	struct FMovieState { int32 Index = 0; int32 Callbacks = 0; bool bPending = false; };
	TSharedRef<FMovieState> State = MakeShared<FMovieState>();
	TArray<TPair<FString, FStepRunner::FStep>> Steps;
	Steps.Add({ TEXT("open and skip all five movies"), [this, State](UWorld* World, int32)
	{
		if (!Hero(World) || !FindWidget<UMatsuriHUDWidget>(World)) { return EStep::Wait; }
		if (State->Index >= 5) { return EStep::Done; }
		if (!State->bPending)
		{
			Run(World)->UseRecordSlot(TestRecordSlot);
			const EMatsuriCinematic Movie = static_cast<EMatsuriCinematic>(State->Index);
			if (!UMatsuriCinematicWidget::TryPlay(*World->GetFirstPlayerController(), Movie,
				FSimpleDelegate::CreateLambda([State]() { ++State->Callbacks; })))
			{
				AddError(FString::Printf(TEXT("Movie %d is missing or failed to start"), State->Index)); return EStep::Done;
			}
			State->bPending = true;
			return EStep::Continue;
		}
		UMatsuriCinematicWidget* Widget = FindWidget<UMatsuriCinematicWidget>(World);
		if (!Widget) { AddError(TEXT("Movie closed or failed before it opened")); return EStep::Done; }
		if (!Widget->HasMediaOpened()) { return EStep::Continue; }
		TestTrue(TEXT("movie pauses the game"), World->IsPaused());
		UButton* Skip = FindButton(*Widget, TEXT("跳过  Skip"));
		if (!Skip) { AddError(TEXT("Movie Skip button is missing")); return EStep::Done; }
		Skip->OnClicked.Broadcast();
		++State->Index;
		TestEqual(TEXT("each skip invokes exactly one callback"), State->Callbacks, State->Index);
		TestFalse(TEXT("each skip restores gameplay"), World->IsPaused());
		State->bPending = false;
		return State->Index == 5 ? EStep::Done : EStep::Continue;
	} });
	SetCVar(TEXT("matsuri.Threats"), false);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] { SetCVar(TEXT("matsuri.Threats"), true); }, 0.0f));
	return true;
}

#endif
