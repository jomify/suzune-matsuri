// Suzune's Night Festival - the three shrine floors and the boss's courtyard: rules, map and look.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriFloorLayout.h"

/** Presentation-only settings for a floor (lighting mood, gray-box palette). */
struct FMatsuriFloorTheme
{
	FText Title;
	FRotator SunRotation = FRotator(-22.0f, 35.0f, 0.0f);
	FLinearColor SunColor = FLinearColor::White;
	float SunLux = 7.0f;
	float FogDensity = 0.015f;
	FLinearColor FogColor = FLinearColor::Gray;
	FLinearColor WallColor = FLinearColor::Red;
	FLinearColor CoverColor = FLinearColor::Gray;
	FLinearColor PillarColor = FLinearColor::Gray;

	/** Off for true night: no sun-lit sky, the fog colour becomes the backdrop. */
	bool bSkyAtmosphere = true;
	/** Post-process exposure compensation (negative = darker, keeps night floors dark). */
	float ExposureBias = 0.0f;
	float SkyLightIntensity = 1.0f;
	/** Festival lantern strings along the walls and lamps on stone lanterns. */
	FLinearColor LanternColor = FLinearColor(1.0f, 0.45f, 0.12f);
	float LanternIntensity = 2500.0f;
};

namespace Matsuri::Floors
{
	/** The maze floors (1F-3F). */
	constexpr int32 Count = 3;
	/** 本殿・月下: Tamamo's courtyard, after 3F. */
	constexpr int32 BossFloor = Count;
	/** Every floor of a run, the boss included. */
	constexpr int32 Total = Count + 1;

	inline bool IsBossFloor(int32 FloorIndex) { return FloorIndex == BossFloor; }

	/** Rules for floor FloorIndex (0-based) of a run; the layout seed mixes in RunSeed. */
	SUZUNEMATSURI_API FMatsuriFloorSpec SpecFor(int32 FloorIndex, int32 RunSeed);

	SUZUNEMATSURI_API FMatsuriFloorTheme ThemeFor(int32 FloorIndex);

	/** Package path of the map that hosts FloorIndex. */
	SUZUNEMATSURI_API const TCHAR* MapFor(int32 FloorIndex);

	/** The boss floor: clearing it ends the run in victory. */
	SUZUNEMATSURI_API bool IsLastFloor(int32 FloorIndex);
}
