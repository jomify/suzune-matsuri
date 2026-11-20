// Suzune's Night Festival - shared gameplay types.

#include "MatsuriTypes.h"

int32 Matsuri::PointsFor(EMatsuriCollectible Kind)
{
	switch (Kind)
	{
	case EMatsuriCollectible::Koban:	return KobanPoints;
	case EMatsuriCollectible::Ginka:	return GinkaPoints;
	case EMatsuriCollectible::Magatama:	return MagatamaPoints;
	}
	checkNoEntry();
	return 0;
}
