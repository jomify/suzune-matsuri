// Suzune's Night Festival - 本殿・月下 dressed with the Mountain Temple pack (when it is in the project).

#pragma once

#include "CoreMinimal.h"

class UWorld;
struct FMatsuriBossArena;

namespace Matsuri::BossArena
{
	/**
	 *  Lays the pack's art over the courtyard's gameplay boxes: real stone lanterns and guardian statues
	 *  (their boxes hidden, their collision kept), columns and eaves round the hall, a shimenawa on the
	 *  torii, ginkgo trees and rocks beyond the walls, leaves, grass and stones on the ground, falling
	 *  leaves and fireflies. Visual only: bullets, cover, perches and zones are untouched. No-op without
	 *  the pack. Returns how many pieces were placed.
	 */
	SUZUNEMATSURI_API int32 Dress(UWorld& World, const FMatsuriBossArena& Arena);
}
