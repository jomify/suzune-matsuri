// Suzune's Night Festival - screen-to-screen navigation shared by the menus.

#include "MatsuriFlow.h"
#include "Containers/Ticker.h"
#include "Kismet/GameplayStatics.h"
#include "MatsuriLoadingWidget.h"
#include "MatsuriAudio.h"
#include "MatsuriFloors.h"
#include "MatsuriRunSubsystem.h"
#include "MatsuriTypes.h"
#include "SuzuneMatsuri.h"

namespace
{
	TWeakObjectPtr<UWorld> TravellingWorld;	// a second click while leaving does not start a second load

	/**
	 *  Fades into the loading screen, then loads Map. The delay runs on the core ticker (real time), so it
	 *  also works from menus that pause the game.
	 */
	void TravelTo(const UObject* WorldContext, const FName Map, const FText& Destination)
	{
		UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (!World || TravellingWorld.Get() == World)
		{
			return;
		}
		TravellingWorld = World;
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			UMatsuriLoadingWidget::ShowLeaving(*PC, Destination);
			Matsuri::Audio::Play2D(PC, EMatsuriSfx::Loading, 0.6f);
		}
		TWeakObjectPtr<UWorld> WeakWorld = World;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld, Map](float)
		{
			if (UWorld* Leaving = WeakWorld.Get())
			{
				UGameplayStatics::OpenLevel(Leaving, Map);
			}
			return false;	// once
		}), UMatsuriLoadingWidget::LeaveSeconds);
	}
}

void Matsuri::Flow::StartNewRun(const UObject* WorldContext, EMatsuriHero Hero)
{
	if (UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(WorldContext))
	{
		Run->StartNewRun(Hero);
	}
	TravelTo(WorldContext, FName(Matsuri::Floors::MapFor(0)), Matsuri::Floors::ThemeFor(0).Title);
}

void Matsuri::Flow::StartAtFloor(const UObject* WorldContext, EMatsuriHero Hero, int32 FloorIndex)
{
	const int32 Floor = FMath::Clamp(FloorIndex, 0, Matsuri::Floors::Total - 1);
	if (UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(WorldContext))
	{
		Run->StartNewRun(Hero, UMatsuriRunSubsystem::DefaultSeed());	// matsuri.Seed reproduces a layout (0 = random)
		Run->StartOnFloor(Floor);
	}
	TravelTo(WorldContext, FName(Matsuri::Floors::MapFor(Floor)), Matsuri::Floors::ThemeFor(Floor).Title);
}

void Matsuri::Flow::RestartWithSameHero(const UObject* WorldContext)
{
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(WorldContext);
	StartNewRun(WorldContext, Run ? Run->GetRunState().Hero : UMatsuriRunSubsystem::DefaultHero());
}

void Matsuri::Flow::ReturnToTitle(const UObject* WorldContext)
{
	TravelTo(WorldContext, FName(Matsuri::Maps::Title), NSLOCTEXT("MatsuriFlow", "Title", "鈴音の夜祭り"));
}

void Matsuri::Flow::ContinueWithCharm(const UObject* WorldContext, EMatsuriOmamori Charm)
{
	UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(WorldContext);
	if (!Run || !Run->IsRunActive())
	{
		UE_LOG(LogSuzuneMatsuri, Error, TEXT("No active run to continue; returning to title"));
		ReturnToTitle(WorldContext);
		return;
	}
	Run->ChooseOmamoriAndAdvance(Charm);
	const int32 Floor = Run->GetRunState().FloorIndex;
	TravelTo(WorldContext, FName(Matsuri::Floors::MapFor(Floor)), Matsuri::Floors::ThemeFor(Floor).Title);
}
