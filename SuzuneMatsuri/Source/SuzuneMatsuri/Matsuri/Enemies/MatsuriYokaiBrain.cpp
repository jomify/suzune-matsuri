// Suzune's Night Festival - yokai tactics as pure decisions.

#include "MatsuriYokaiBrain.h"

namespace
{
	constexpr float ChochinStalkSpeed = 420.0f;		// faster than its old open chase (250): the threat is being unwatched
	constexpr float ChochinLungeSpeed = 820.0f;
	constexpr float ChochinCreepSpeed = 60.0f;		// watched, it still edges closer: staring is no escape
}

bool Matsuri::YokaiBrain::IsInViewCone(const FVector& Eye, const FVector& ViewDirection, const FVector& Point, float CosHalfAngle)
{
	const FVector To = (Point - Eye).GetSafeNormal();
	return FVector::DotProduct(To, ViewDirection.GetSafeNormal()) >= CosHalfAngle;
}

Matsuri::YokaiBrain::EChochinMove Matsuri::YokaiBrain::ChochinMove(bool bWatched, float Distance)
{
	if (bWatched)
	{
		return EChochinMove::Creep;
	}
	return Distance <= ChochinLungeRange ? EChochinMove::Lunge : EChochinMove::Stalk;
}

float Matsuri::YokaiBrain::ChochinSpeed(EChochinMove Move)
{
	switch (Move)
	{
	case EChochinMove::Creep:	return ChochinCreepSpeed;
	case EChochinMove::Stalk:	return ChochinStalkSpeed;
	case EChochinMove::Lunge:	return ChochinLungeSpeed;
	}
	return 0.0f;
}

Matsuri::YokaiBrain::EKasaMove Matsuri::YokaiBrain::KasaMove(bool bHasSight, float Distance, int32 StepsAway)
{
	if (bHasSight && Distance <= KasaPounceRange)
	{
		return EKasaMove::Pounce;
	}
	// Around the corner: one corridor step away on the maze (or simply unseen nearby off it).
	const bool bOneCornerAway = StepsAway == INDEX_NONE ? true : StepsAway <= 1;
	if (!bHasSight && bOneCornerAway && Distance <= KasaLurkRange)
	{
		return EKasaMove::Lurk;
	}
	return EKasaMove::Path;
}

namespace
{
	int32 NearestReachable(const TArray<Matsuri::YokaiBrain::FCoverSpot>& Spots, const FVector& From, bool bHidden)
	{
		int32 Best = INDEX_NONE;
		double BestDistanceSq = TNumericLimits<double>::Max();
		for (int32 i = 0; i < Spots.Num(); ++i)
		{
			const Matsuri::YokaiBrain::FCoverSpot& Spot = Spots[i];
			const double DistanceSq = FVector::DistSquared2D(Spot.Location, From);
			if (Spot.bReachable && Spot.bHiddenFromPlayer == bHidden && DistanceSq < BestDistanceSq)
			{
				Best = i;
				BestDistanceSq = DistanceSq;
			}
		}
		return Best;
	}
}

int32 Matsuri::YokaiBrain::PickCover(const TArray<FCoverSpot>& Spots, const FVector& From)
{
	return NearestReachable(Spots, From, true);
}

int32 Matsuri::YokaiBrain::PickPeek(const TArray<FCoverSpot>& Spots, const FVector& From)
{
	return NearestReachable(Spots, From, false);
}

Matsuri::YokaiBrain::EKappaMove Matsuri::YokaiBrain::KappaMove(bool bSeesPlayer, bool bReadyToThrow, bool bInRange)
{
	if (!bInRange)
	{
		return EKappaMove::Approach;	// the cover game only starts within throwing range
	}
	if (bReadyToThrow)
	{
		return bSeesPlayer ? EKappaMove::Throw : EKappaMove::Peek;
	}
	return bSeesPlayer ? EKappaMove::Hide : EKappaMove::Hold;
}
