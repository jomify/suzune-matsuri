// Suzune's Night Festival - the rendering quality the game plays at.

#include "MatsuriQuality.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"
#include "SuzuneMatsuri.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

namespace
{
	constexpr int32 EpicLevel = 3;
	constexpr int32 HighLevel = 2;

	TAutoConsoleVariable<int32> CVarMaxQuality(
		TEXT("matsuri.MaxQuality"), EpicLevel,
		TEXT("Highest scalability level (0 low .. 4 cinematic) any group renders at while the game plays. -1 = no cap."));

	TAutoConsoleVariable<int32> CVarRenderHeight(
		TEXT("matsuri.RenderHeight"), 900,
		TEXT("Lines the game renders at before TSR upscales to the window (a 1440p window renders at ~63%). 0 = full resolution."));

	constexpr float MinScreenPercentage = 50.0f;	// below this TSR's upscale starts to smear

	TAutoConsoleVariable<int32> CVarMaxGIQuality(
		TEXT("matsuri.MaxGIQuality"), HighLevel,
		TEXT("Highest Lumen global illumination level while the game plays (High: the final gather costs about an eighth of Cinematic). -1 = no cap."));

	/** The levels before the first cap of this play session; restored when Play stops (editor). */
	TOptional<Scalability::FQualityLevels> LevelsBeforePlay;

	int32 Cap(int32 Level, int32 Max)
	{
		return Max < 0 ? Level : FMath::Min(Level, Max);
	}

#if WITH_EDITOR
	void RestoreWhenPlayStops()
	{
		static FDelegateHandle Handle;
		if (Handle.IsValid())
		{
			return;
		}
		Handle = FEditorDelegates::EndPIE.AddLambda([](bool)
		{
			if (LevelsBeforePlay.IsSet())
			{
				Scalability::SetQualityLevels(LevelsBeforePlay.GetValue(), true);
				LevelsBeforePlay.Reset();
			}
		});
	}
#endif
}

Scalability::FQualityLevels Matsuri::Quality::Capped(const Scalability::FQualityLevels& Current, int32 MaxGroup, int32 MaxGI)
{
	Scalability::FQualityLevels Out = Current;
	Out.ViewDistanceQuality = Cap(Current.ViewDistanceQuality, MaxGroup);
	Out.AntiAliasingQuality = Cap(Current.AntiAliasingQuality, MaxGroup);
	Out.ShadowQuality = Cap(Current.ShadowQuality, MaxGroup);
	Out.GlobalIlluminationQuality = Cap(Cap(Current.GlobalIlluminationQuality, MaxGroup), MaxGI);
	Out.ReflectionQuality = Cap(Current.ReflectionQuality, MaxGroup);
	Out.PostProcessQuality = Cap(Current.PostProcessQuality, MaxGroup);
	Out.TextureQuality = Cap(Current.TextureQuality, MaxGroup);
	Out.EffectsQuality = Cap(Current.EffectsQuality, MaxGroup);
	Out.FoliageQuality = Cap(Current.FoliageQuality, MaxGroup);
	Out.ShadingQuality = Cap(Current.ShadingQuality, MaxGroup);
	Out.LandscapeQuality = Cap(Current.LandscapeQuality, MaxGroup);
	return Out;
}

float Matsuri::Quality::ScreenPercentageFor(int32 ViewportHeight, int32 RenderHeight)
{
	if (RenderHeight <= 0 || ViewportHeight <= RenderHeight)
	{
		return 100.0f;
	}
	return FMath::Clamp(100.0f * RenderHeight / ViewportHeight, MinScreenPercentage, 100.0f);
}

void Matsuri::Quality::ApplyForPlay()
{
	const Scalability::FQualityLevels Current = Scalability::GetQualityLevels();
	Scalability::FQualityLevels Target = Capped(Current, CVarMaxQuality.GetValueOnGameThread(), CVarMaxGIQuality.GetValueOnGameThread());
	const FViewport* Viewport = GEngine && GEngine->GameViewport ? GEngine->GameViewport->Viewport : nullptr;
	const int32 ViewportHeight = Viewport ? Viewport->GetSizeXY().Y : 0;
	Target.ResolutionQuality = FMath::Min(Current.ResolutionQuality, ScreenPercentageFor(ViewportHeight, CVarRenderHeight.GetValueOnGameThread()));
	if (Target == Current)
	{
		return;
	}
	if (!LevelsBeforePlay.IsSet())
	{
		LevelsBeforePlay = Current;
	}
#if WITH_EDITOR
	RestoreWhenPlayStops();
#endif
	Scalability::SetQualityLevels(Target, true);
	UE_LOG(LogSuzuneMatsuri, Log, TEXT("Play quality: GI %d -> %d, shadows %d -> %d, post %d -> %d, resolution %.0f%% -> %.0f%% of %d lines (matsuri.MaxQuality %d, matsuri.MaxGIQuality %d, matsuri.RenderHeight %d)"),
		Current.GlobalIlluminationQuality, Target.GlobalIlluminationQuality, Current.ShadowQuality, Target.ShadowQuality,
		Current.PostProcessQuality, Target.PostProcessQuality, Current.ResolutionQuality, Target.ResolutionQuality, ViewportHeight,
		CVarMaxQuality.GetValueOnGameThread(), CVarMaxGIQuality.GetValueOnGameThread(), CVarRenderHeight.GetValueOnGameThread());
}
