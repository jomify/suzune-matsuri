// Suzune's Night Festival - background music: every scene picks its track, every track is imported, and a run
// moves through them (title, floors, clear sting, boss bars, ending, defeat sting).
// Run: Automation RunTests SuzuneMatsuri.Music   /   Automation RunTests SuzuneMatsuri.E2E.Music

#include "MatsuriTestKit.h"
#include "MatsuriMusic.h"
#include "Sound/SoundWave.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriMusicScenesTest, "SuzuneMatsuri.Music.EachSceneHasItsTrack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriMusicScenesTest::RunTest(const FString&)
{
	TestEqual(TEXT("1F"), Matsuri::Music::ForFloor(0), EMatsuriMusic::Floor1);
	TestEqual(TEXT("2F"), Matsuri::Music::ForFloor(1), EMatsuriMusic::Floor2);
	TestEqual(TEXT("3F"), Matsuri::Music::ForFloor(2), EMatsuriMusic::Floor3);
	TestEqual(TEXT("the courtyard opens on the boss track"), Matsuri::Music::ForFloor(3), EMatsuriMusic::BossOpening);
	TestEqual(TEXT("her first bar"), Matsuri::Music::ForBossPhase(1), EMatsuriMusic::BossOpening);
	TestEqual(TEXT("her second bar keeps it"), Matsuri::Music::ForBossPhase(2), EMatsuriMusic::BossOpening);
	TestEqual(TEXT("her last bar has its own"), Matsuri::Music::ForBossPhase(3), EMatsuriMusic::BossFinal);

	TSet<FString> Names;
	for (int32 i = static_cast<int32>(EMatsuriMusic::Title); i <= static_cast<int32>(EMatsuriMusic::Ending); ++i)
	{
		const FString Name = Matsuri::Music::AssetName(static_cast<EMatsuriMusic>(i));
		TestFalse(TEXT("every track has an asset"), Name.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s is used once"), *Name), Names.Contains(Name));
		Names.Add(Name);
	}
	TestTrue(TEXT("None is silence"), FString(Matsuri::Music::AssetName(EMatsuriMusic::None)).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriMusicImportedTest, "SuzuneMatsuri.Music.EveryTrackIsImported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriMusicImportedTest::RunTest(const FString&)
{
	if (!Matsuri::Music::Sound(EMatsuriMusic::Title))
	{
		AddInfo(TEXT("no music imported (tools/ue/import_audio.py): nothing to check"));
		return true;
	}
	for (int32 i = static_cast<int32>(EMatsuriMusic::Title); i <= static_cast<int32>(EMatsuriMusic::Ending); ++i)
	{
		const EMatsuriMusic Track = static_cast<EMatsuriMusic>(i);
		const USoundWave* Wave = Cast<USoundWave>(Matsuri::Music::Sound(Track));
		TestNotNull(FString::Printf(TEXT("%s is imported"), Matsuri::Music::AssetName(Track)), Wave);
		TestTrue(FString::Printf(TEXT("%s loops (a floor can outlast it)"), Matsuri::Music::AssetName(Track)), Wave && Wave->bLooping);
	}
	for (const EMatsuriSting Sting : { EMatsuriSting::Clear, EMatsuriSting::Defeat })
	{
		const USoundWave* Wave = Cast<USoundWave>(Matsuri::Music::Sound(Sting));
		TestNotNull(FString::Printf(TEXT("%s is imported"), Matsuri::Music::AssetName(Sting)), Wave);
		TestTrue(FString::Printf(TEXT("%s plays once"), Matsuri::Music::AssetName(Sting)), Wave && !Wave->bLooping);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "MatsuriBoss.h"
#include "MatsuriBossGameMode.h"
#include "MatsuriFloorGameMode.h"
#include "MatsuriFloors.h"
#include "MatsuriFlow.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriMainMenuWidget.h"

using namespace MatsuriTest;

namespace
{
	UMatsuriMusicSubsystem* Music(UWorld* World)
	{
		return UMatsuriMusicSubsystem::Get(World);
	}

	bool OnFloor(UWorld* World, int32 FloorIndex)
	{
		return IsMap(World, Matsuri::Floors::MapFor(FloorIndex)) && Hero(World) && FindWidget<UMatsuriHUDWidget>(World);
	}

	/** Waits for FloorIndex and checks its track. */
	FStepRunner::FStep ArriveTo(FAutomationTestBase& Test, int32 FloorIndex, EMatsuriMusic Expected)
	{
		return [&Test, FloorIndex, Expected](UWorld* World, int32 Frame)
		{
			if (!OnFloor(World, FloorIndex))
			{
				return EStep::Wait;
			}
			Test.TestEqual(FString::Printf(TEXT("floor %d plays its track"), FloorIndex + 1), Music(World)->GetCurrent(), Expected);
			return EStep::Done;
		};
	}

	AMatsuriBoss* Boss(UWorld* World)
	{
		const AMatsuriBossGameMode* Mode = World ? World->GetAuthGameMode<AMatsuriBossGameMode>() : nullptr;
		return Mode ? Mode->GetBoss() : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriMusicE2ETest, "SuzuneMatsuri.E2E.Music",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMatsuriMusicE2ETest::RunTest(const FString&)
{
	TArray<TPair<FString, FStepRunner::FStep>> Steps;
	Steps.Add({ TEXT("the title plays its theme"), [this](UWorld* World, int32 Frame)
	{
		if (!IsMap(World, Matsuri::Maps::Title) || !FindWidget<UMatsuriMainMenuWidget>(World))
		{
			return EStep::Wait;
		}
		Run(World)->UseRecordSlot(TestRecordSlot);
		TestEqual(TEXT("title theme"), Music(World)->GetCurrent(), EMatsuriMusic::Title);
		Matsuri::Flow::StartNewRun(World, EMatsuriHero::Mio);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("1F"), ArriveTo(*this, 0, EMatsuriMusic::Floor1) });
	Steps.Add({ TEXT("clearing a floor plays the clear sting"), [this](UWorld* World, int32 Frame)
	{
		if (Frame == 0)
		{
			Run(World)->SetFloorTargetForTesting(1);
			Run(World)->Collect(EMatsuriCollectible::Koban);
			return EStep::Continue;
		}
		TestTrue(TEXT("the clear sting"), Music(World)->GetSting() == EMatsuriSting::Clear);
		TestEqual(TEXT("the floor's track has stopped"), Music(World)->GetCurrent(), EMatsuriMusic::None);
		Matsuri::Flow::ContinueWithCharm(World, static_cast<EMatsuriOmamori>(0));
		return EStep::Done;
	} });
	Steps.Add({ TEXT("2F"), ArriveTo(*this, 1, EMatsuriMusic::Floor2) });
	Steps.Add({ TEXT("to the boss"), [](UWorld* World, int32 Frame)
	{
		Matsuri::Flow::StartAtFloor(World, EMatsuriHero::Mio, Matsuri::Floors::BossFloor);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("her bars change the track at the last one"), [this](UWorld* World, int32 Frame)
	{
		if (!OnFloor(World, Matsuri::Floors::BossFloor) || !Boss(World))
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			TestEqual(TEXT("the courtyard opens on the boss track"), Music(World)->GetCurrent(), EMatsuriMusic::BossOpening);
			Boss(World)->DevNextBar();
			TestEqual(TEXT("her second bar keeps it"), Music(World)->GetCurrent(), EMatsuriMusic::BossOpening);
			Boss(World)->DevNextBar();
			TestEqual(TEXT("her last bar has its own"), Music(World)->GetCurrent(), EMatsuriMusic::BossFinal);
			Boss(World)->DevDefeat();
			return EStep::Continue;
		}
		if (Music(World)->GetCurrent() != EMatsuriMusic::Ending && Frame < 300)
		{
			return EStep::Continue;
		}
		TestEqual(TEXT("defeating her plays the ending"), Music(World)->GetCurrent(), EMatsuriMusic::Ending);
		Matsuri::Flow::StartAtFloor(World, EMatsuriHero::Mio, 0);
		return EStep::Done;
	} });
	Steps.Add({ TEXT("falling plays the defeat sting"), [this](UWorld* World, int32 Frame)
	{
		if (!OnFloor(World, 0))
		{
			return EStep::Wait;
		}
		if (Frame == 0)
		{
			TestEqual(TEXT("a new run starts on 1F's track"), Music(World)->GetCurrent(), EMatsuriMusic::Floor1);
			AMatsuriFloorGameMode* Mode = World->GetAuthGameMode<AMatsuriFloorGameMode>();
			Mode->HurtPlayer(nullptr, 1.0e6f, FText::GetEmpty(), INDEX_NONE, FSimpleDelegate());
			return EStep::Continue;
		}
		if (!Music(World)->GetSting().IsSet() && Frame < 300)
		{
			return EStep::Continue;
		}
		TestTrue(TEXT("the defeat sting"), Music(World)->GetSting() == EMatsuriSting::Defeat);
		return EStep::Done;
	} });

	UseTestBossBrain();
	SetCVar(TEXT("matsuri.Story"), false);
	SetCVar(TEXT("matsuri.Threats"), false);
	SetCVar(TEXT("matsuri.God"), false);
	SetCVar(TEXT("matsuri.Replay"), false);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Maps::Title));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(this, MoveTemp(Steps)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([] {
		SetCVar(TEXT("matsuri.Story"), true);
		SetCVar(TEXT("matsuri.Threats"), true);
		SetCVar(TEXT("matsuri.Replay"), true);
		RestoreBossBrain();
	}, 0.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
