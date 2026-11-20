// Suzune's Night Festival - the three shrine floors: rules, map and look.

#include "MatsuriFloors.h"

#define LOCTEXT_NAMESPACE "MatsuriFloors"

namespace
{
	int32 ClampFloor(int32 FloorIndex)
	{
		return FMath::Clamp(FloorIndex, 0, Matsuri::Floors::Total - 1);
	}

	const TCHAR* const FloorMaps[Matsuri::Floors::Total] = {
		TEXT("/Game/Matsuri/Maps/L_Floor01"),
		TEXT("/Game/Matsuri/Maps/L_Floor02"),
		TEXT("/Game/Matsuri/Maps/L_Floor03"),
		TEXT("/Game/Matsuri/Maps/L_Boss_Honden"),
	};

	/** Nothing to collect toward: the boss floor is cleared by defeating her. */
	constexpr int32 BossTargetScore = 1000000;
}

FMatsuriFloorSpec Matsuri::Floors::SpecFor(int32 FloorIndex, int32 RunSeed)
{
	const int32 Floor = ClampFloor(FloorIndex);

	FMatsuriFloorSpec Spec;
	Spec.Seed = static_cast<int32>(HashCombine(GetTypeHash(RunSeed), GetTypeHash(Floor + 1)));

	switch (Floor)
	{
	case 0:	// 1F 竹林の参道: learn the controls
		Spec.HalfExtent = 2400.0f;	Spec.ObstacleCount = 22;
		Spec.KobanCount = 14;	Spec.GinkaCount = 6;	Spec.MagatamaCount = 2;	// 36 pts
		Spec.TargetScore = 15;	Spec.TimeLimitSeconds = 150.0f;
		Spec.bMaze = true;	Spec.MazeCells = 7;	Spec.LoopChance = 0.25f;	Spec.Plazas = 2;
		Spec.ChochinCount = 3;	Spec.KasaCount = 2;	Spec.KappaCount = 1;
		Spec.FoxfireCount = 1;	Spec.SpikeCount = 2;	Spec.AmmoSpotCount = 6;
		break;
	case 1:	// 2F 夏祭りの夜店街: crowded stalls, more pressure
		Spec.HalfExtent = 2500.0f;	Spec.ObstacleCount = 26;
		Spec.KobanCount = 16;	Spec.GinkaCount = 8;	Spec.MagatamaCount = 3;	// 47 pts
		Spec.TargetScore = 25;	Spec.TimeLimitSeconds = 140.0f;
		Spec.bMaze = true;	Spec.MazeCells = 8;	Spec.LoopChance = 0.18f;	Spec.Plazas = 2;
		Spec.ChochinCount = 4;	Spec.KasaCount = 3;	Spec.KappaCount = 2;
		Spec.FoxfireCount = 2;	Spec.SpikeCount = 3;	Spec.AmmoSpotCount = 7;
		break;
	case 2:	// 3F 千本鳥居: dense pillars, least time
		Spec.HalfExtent = 2600.0f;	Spec.ObstacleCount = 30;
		Spec.KobanCount = 18;	Spec.GinkaCount = 10;	Spec.MagatamaCount = 4;	// 58 pts
		Spec.TargetScore = 35;	Spec.TimeLimitSeconds = 130.0f;
		Spec.bMaze = true;	Spec.MazeCells = 9;	Spec.LoopChance = 0.12f;	Spec.Plazas = 1;
		Spec.ChochinCount = 5;	Spec.KasaCount = 4;	Spec.KappaCount = 3;
		Spec.FoxfireCount = 3;	Spec.SpikeCount = 4;	Spec.AmmoSpotCount = 8;
		break;
	default:	// 本殿・月下: the courtyard is fixed (MatsuriBossArena); only the clock and the goal matter
		Spec.HalfExtent = 3000.0f;	Spec.bMaze = false;
		Spec.TargetScore = BossTargetScore;	Spec.TimeLimitSeconds = 300.0f;
		break;
	}
	return Spec;
}

FMatsuriFloorTheme Matsuri::Floors::ThemeFor(int32 FloorIndex)
{
	FMatsuriFloorTheme Theme;
	switch (ClampFloor(FloorIndex))
	{
	case 0:
		Theme.Title = LOCTEXT("F1", "1F  竹林の参道");
		// Dusk: low warm sun through haze, lanterns just lit.
		Theme.SunRotation = FRotator(-11.0f, 35.0f, 0.0f);
		Theme.SunColor = FLinearColor(1.0f, 0.62f, 0.36f);	Theme.SunLux = 6.0f;
		Theme.FogDensity = 0.02f;	Theme.FogColor = FLinearColor(0.60f, 0.36f, 0.26f);
		Theme.ExposureBias = -0.3f;	Theme.LanternIntensity = 1500.0f;
		Theme.WallColor = FLinearColor(1.0f, 0.88f, 0.70f);	// ochre earthen plaster (土塀)
		Theme.CoverColor = FLinearColor(0.20f, 0.30f, 0.10f);	// bamboo fences
		Theme.PillarColor = FLinearColor(0.42f, 0.41f, 0.38f);	// stone lanterns
		break;
	case 1:
		Theme.Title = LOCTEXT("F2", "2F  夏祭りの夜店街");
		// Twilight: the sun just under the horizon paints the sky; the stalls run on lantern light.
		Theme.SunRotation = FRotator(3.0f, 200.0f, 0.0f);
		Theme.SunColor = FLinearColor(1.0f, 0.50f, 0.32f);	Theme.SunLux = 5.0f;
		Theme.FogDensity = 0.025f;	Theme.FogColor = FLinearColor(0.22f, 0.10f, 0.20f);
		Theme.ExposureBias = -0.8f;	Theme.SkyLightIntensity = 0.6f;	Theme.LanternIntensity = 4000.0f;
		Theme.WallColor = FLinearColor(1.0f, 0.84f, 0.66f);
		Theme.CoverColor = FLinearColor(0.35f, 0.18f, 0.08f);	// yatai stalls
		Theme.PillarColor = FLinearColor(0.85f, 0.45f, 0.10f);	// chochin poles
		break;
	case 2:
		Theme.Title = LOCTEXT("F3", "3F  千本鳥居");
		// Deep night: cold moonlight, indigo mist, every torii lit from below.
		Theme.SunRotation = FRotator(-40.0f, 120.0f, 0.0f);
		Theme.SunColor = FLinearColor(0.55f, 0.65f, 1.0f);	Theme.SunLux = 0.6f;	// moonlight
		Theme.FogDensity = 0.035f;	Theme.FogColor = FLinearColor(0.05f, 0.06f, 0.16f);
		Theme.bSkyAtmosphere = false;	Theme.ExposureBias = -1.0f;	Theme.SkyLightIntensity = 0.4f;
		Theme.LanternIntensity = 5000.0f;	Theme.LanternColor = FLinearColor(1.0f, 0.40f, 0.10f);
		Theme.WallColor = FLinearColor(0.90f, 0.76f, 0.62f);
		Theme.CoverColor = FLinearColor(0.30f, 0.30f, 0.32f);	// fox statues
		Theme.PillarColor = FLinearColor(0.75f, 0.10f, 0.04f);	// vermilion torii posts
		break;
	default:
		Theme.Title = LOCTEXT("Boss", "本殿・月下");
		// Under the full moon: silver light, thin violet mist, the hall's lamps warm against it.
		Theme.SunRotation = FRotator(-48.0f, 160.0f, 0.0f);
		Theme.SunColor = FLinearColor(0.65f, 0.72f, 1.0f);	Theme.SunLux = 1.2f;
		Theme.FogDensity = 0.012f;	Theme.FogColor = FLinearColor(0.08f, 0.06f, 0.16f);
		Theme.bSkyAtmosphere = false;	Theme.ExposureBias = -0.6f;	Theme.SkyLightIntensity = 0.6f;
		Theme.LanternIntensity = 6000.0f;	Theme.LanternColor = FLinearColor(1.0f, 0.5f, 0.18f);
		break;
	}
	return Theme;
}

const TCHAR* Matsuri::Floors::MapFor(int32 FloorIndex)
{
	return FloorMaps[ClampFloor(FloorIndex)];
}

bool Matsuri::Floors::IsLastFloor(int32 FloorIndex)
{
	return FloorIndex >= Total - 1;
}

#undef LOCTEXT_NAMESPACE
