// Suzune's Night Festival - screen-to-screen navigation shared by the menus.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriHeroes.h"
#include "MatsuriOmamori.h"

namespace Matsuri::Flow
{
	/** New run, floor 1, with the given heroine. */
	SUZUNEMATSURI_API void StartNewRun(const UObject* WorldContext, EMatsuriHero Hero);

	/** Developer mode: a new run that starts on FloorIndex (Matsuri::Floors::BossFloor for the boss). */
	SUZUNEMATSURI_API void StartAtFloor(const UObject* WorldContext, EMatsuriHero Hero, int32 FloorIndex);

	/** New run with the same heroine as the run that just ended (Try Again / Play Again). */
	SUZUNEMATSURI_API void RestartWithSameHero(const UObject* WorldContext);

	/** Back to the title screen. */
	SUZUNEMATSURI_API void ReturnToTitle(const UObject* WorldContext);

	/** Takes the chosen charm and loads the next floor of the current run. */
	SUZUNEMATSURI_API void ContinueWithCharm(const UObject* WorldContext, EMatsuriOmamori Charm);
}
