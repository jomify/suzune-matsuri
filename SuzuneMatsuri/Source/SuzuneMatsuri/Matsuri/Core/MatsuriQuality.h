// Suzune's Night Festival - the rendering quality the game plays at.

#pragma once

#include "CoreMinimal.h"
#include "Scalability.h"

/**
 *  Playtest: the game ran at 17 fps. The editor's scalability was Cinematic (a per-user, all-projects
 *  setting meant for offline renders), and Lumen's final gather alone took 24 of the GPU's 29 ms a frame.
 *  While the game plays, each scalability group is capped (Epic by default, Lumen GI at High); in the
 *  editor the previous levels come back when Play stops, so the editor's own setting is never changed.
 *  matsuri.MaxQuality / matsuri.MaxGIQuality set the caps (-1 = no cap).
 *
 *  Playtest: "still stuttering" at 2560x1600. The GPU cost grows with the pixel count: at full resolution
 *  1F took 19 ms and the boss 19-20 ms, past the 16.7 ms a 60 Hz frame allows, so frames kept dropping to
 *  30 fps. Rendered at about 900 lines and upscaled by TSR, 1F holds 60 fps (10.5 ms) and the boss 58.
 *  matsuri.RenderHeight sets the lines (0 = always full resolution).
 */
namespace Matsuri::Quality
{
	/** Pure: Current with every group at most MaxGroup and GI at most MaxGI (-1 leaves a group alone). */
	SUZUNEMATSURI_API Scalability::FQualityLevels Capped(const Scalability::FQualityLevels& Current, int32 MaxGroup, int32 MaxGI);

	/** Pure: the screen percentage that renders a ViewportHeight-line view at about RenderHeight lines (never above 100). */
	SUZUNEMATSURI_API float ScreenPercentageFor(int32 ViewportHeight, int32 RenderHeight);

	/** Applies the caps for this play session (each game mode's BeginPlay calls it; repeat calls are free). */
	SUZUNEMATSURI_API void ApplyForPlay();
}
