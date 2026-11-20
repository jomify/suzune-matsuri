// Suzune's Night Festival - shared gameplay types.

#pragma once

#include "CoreMinimal.h"
#include "MatsuriTypes.generated.h"

/** The three collectible tiers from the brief: gold +1, silver +2, gem +5. */
UENUM(BlueprintType, meta=(ScriptName="MatsuriCollectibleKind"))
enum class EMatsuriCollectible : uint8
{
	Koban		UMETA(DisplayName = "Koban (gold, +1)"),
	Ginka		UMETA(DisplayName = "Ginka (silver, +2)"),
	Magatama	UMETA(DisplayName = "Magatama (gem, +5)")
};

namespace Matsuri
{
	constexpr int32 KobanPoints = 1;
	constexpr int32 GinkaPoints = 2;
	constexpr int32 MagatamaPoints = 5;

	SUZUNEMATSURI_API int32 PointsFor(EMatsuriCollectible Kind);

	/** Title map; floor maps come from Matsuri::Floors::MapFor. */
	namespace Maps
	{
		inline const TCHAR* Title = TEXT("/Game/Matsuri/Maps/L_Title");
	}
}
