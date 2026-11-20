#include "Misc/AutomationTest.h"
#include "MatsuriCinematicWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriUltimateMoviePathsTest, "SuzuneMatsuri.Ultimate.MoviePaths",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriUltimateMoviePathsTest::RunTest(const FString&)
{
 TestTrue(TEXT("Sakura has her own movie"), UMatsuriCinematicWidget::RelativeMoviePath(EMatsuriCinematic::UltimateSakura).Contains(TEXT("ultimate_sakura")));
 TestTrue(TEXT("Suzune has her own movie"), UMatsuriCinematicWidget::RelativeMoviePath(EMatsuriCinematic::UltimateSuzune).Contains(TEXT("ultimate_suzune")));
 TestTrue(TEXT("Mio has her own movie"), UMatsuriCinematicWidget::RelativeMoviePath(EMatsuriCinematic::UltimateMio).Contains(TEXT("ultimate_mio")));
 return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "MatsuriTestKit.h"
#include "MatsuriCombatComponent.h"
#include "MatsuriSkillComponent.h"
#include "MatsuriHUDWidget.h"
#include "MatsuriLoadingWidget.h"
#include "MatsuriFloors.h"
#include "MatsuriFloorGameMode.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
 using namespace MatsuriTest;
 bool RunUltimateMovie(FAutomationTestBase& Test, EMatsuriHero Heroine, const TCHAR* Shot)
 {
  if (!FApp::CanEverRender()) { Test.AddInfo(TEXT("Ultimate movie playback needs the rendered test run.")); return true; }
  struct FState { bool Started=false; bool SawOpen=false; int32 OpenFrame=0; int32 Ammo=0; float WorldTime=0; double OpenTime=0; };
  TSharedRef<FState> State=MakeShared<FState>();
  TArray<TPair<FString,FStepRunner::FStep>> Steps;
  Steps.Add({TEXT("charged Q plays and automatically resumes"), [&Test,State,Shot](UWorld* World,int32 Frame)
  {
   if (!Hero(World) || !FindWidget<UMatsuriHUDWidget>(World) || FindWidget<UMatsuriLoadingWidget>(World)) return EStep::Wait;
   auto* Skills=Hero(World)->FindComponentByClass<UMatsuriSkillComponent>();
   auto* Combat=Hero(World)->FindComponentByClass<UMatsuriCombatComponent>();
   if (!State->Started)
   {
    Run(World)->UseRecordSlot(TestRecordSlot);
    Run(World)->SetFloorTargetForTesting(100000);
    Test.TestFalse(TEXT("uncharged Q does not fire"),Skills->TryUseSkill(EMatsuriSkillSlot::Q));
    Test.TestNull(TEXT("uncharged Q does not open a movie"),FindWidget<UMatsuriCinematicWidget>(World));
    Run(World)->AddUltCharge(1);
    Test.TestTrue(TEXT("charged Q fires"),Skills->TryUseSkill(EMatsuriSkillSlot::Q));
    State->Ammo=Run(World)->GetRunState().Ammo;
    State->WorldTime=World->GetTimeSeconds();
    State->Started=true;
    return EStep::Continue;
   }
   auto* Movie=FindWidget<UMatsuriCinematicWidget>(World);
   if (Movie)
   {
    Test.TestTrue(TEXT("battle pauses while animation plays"),World->IsPaused());
    Test.TestFalse(TEXT("animation blocks gunfire"),Combat->TryFire());
    Test.TestFalse(TEXT("animation blocks another skill"),Skills->TryUseSkill(EMatsuriSkillSlot::Shift));
    Test.TestEqual(TEXT("animation does not consume additional ammo"),Run(World)->GetRunState().Ammo,State->Ammo);
    if (Movie->HasMediaOpened())
    {
     if (!State->SawOpen) { State->SawOpen=true;State->OpenFrame=Frame;State->OpenTime=FPlatformTime::Seconds(); }
     SettleAndShoot(Frame-State->OpenFrame,Shot,15);
    }
    return EStep::Continue;
   }
   Test.TestTrue(TEXT("movie actually opened before finishing"),State->SawOpen);
   Test.TestTrue(TEXT("short animation finished within its configured limit"),FPlatformTime::Seconds()-State->OpenTime < 5);
   Test.TestFalse(TEXT("battle resumes after animation"),World->IsPaused());
   Test.TestTrue(TEXT("first-person view stays on the hero"),World->GetFirstPlayerController()->GetViewTarget()==Hero(World));
   Test.TestTrue(TEXT("combat lock is released"),Combat->TryFire());
   return EStep::Done;
  }});
  SetIntCVar(TEXT("matsuri.Hero"),static_cast<int32>(Heroine));
  SetCVar(TEXT("matsuri.Threats"),false);
  ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Matsuri::Floors::MapFor(0)));
  ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
  ADD_LATENT_AUTOMATION_COMMAND(FStepRunner(&Test,MoveTemp(Steps),false,false,true));
  ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
  ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([]{ SetCVar(TEXT("matsuri.Threats"),true); },0));
  return true;
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriUltimateSakuraE2ETest,"SuzuneMatsuri.E2E.UltimateSakura",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FMatsuriUltimateSakuraE2ETest::RunTest(const FString&) { return RunUltimateMovie(*this,EMatsuriHero::Sakura,TEXT("ultimate_sakura")); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriUltimateSuzuneE2ETest,"SuzuneMatsuri.E2E.UltimateSuzune",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FMatsuriUltimateSuzuneE2ETest::RunTest(const FString&) { return RunUltimateMovie(*this,EMatsuriHero::Suzune,TEXT("ultimate_suzune")); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriUltimateMioE2ETest,"SuzuneMatsuri.E2E.UltimateMio",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FMatsuriUltimateMioE2ETest::RunTest(const FString&) { return RunUltimateMovie(*this,EMatsuriHero::Mio,TEXT("ultimate_mio")); }
#endif
