// Suzune's Night Festival - which Tripo mesh dresses which game object, and how it is posed.

#include "MatsuriArtTable.h"

FMatsuriArtPose Matsuri::ArtTable::Collectible(EMatsuriCollectible Kind)
{
	switch (Kind)
	{
	case EMatsuriCollectible::Koban:	return { TEXT("SM_Pickup_Koban") };
	case EMatsuriCollectible::Ginka:	return { TEXT("SM_Pickup_Ginka") };
	case EMatsuriCollectible::Magatama:	return { TEXT("SM_Pickup_Magatama") };
	}
	return {};
}

FMatsuriArtPose Matsuri::ArtTable::AmmoTalisman()
{
	return { TEXT("SM_Pickup_Ofuda") };
}

FMatsuriArtPose Matsuri::ArtTable::Chochin()
{
	return { TEXT("SM_Yokai_Chochin") };
}

FMatsuriArtPose Matsuri::ArtTable::Kasa()
{
	return { TEXT("SM_Yokai_Kasa"), FRotator::ZeroRotator, 0.82f, FVector(0.0f, 0.0f, 0.0f) };	// shrunk to its 50 cm body radius so it never sinks into walls
}

FMatsuriArtPose Matsuri::ArtTable::Kappa()
{
	return { TEXT("SM_Yokai_Kappa"), FRotator::ZeroRotator, 0.9f };	// 1.1 m wide model on a 50 cm body radius
}

FMatsuriArtPose Matsuri::ArtTable::ViewWeapon(EMatsuriHero Hero)
{
	switch (Hero)
	{
	// Offsets put the mesh's centre there (forward, right, up from the eye). Forward stays within the
	// capsule's 34 cm so the weapon never sinks into a wall the player is hugging.
	case EMatsuriHero::Sakura:	return { TEXT("SM_Weapon_HamaYumi"), FRotator(0.0f, 0.0f, 12.0f), 0.3f, FVector(31.0f, -20.0f, -15.0f) };	// at the lower left like Hanzo's: the middle of the view stays clear
	case EMatsuriHero::Suzune:	return { TEXT("SM_Weapon_Shuriken"), FRotator(18.0f, -28.0f, 0.0f), 1.0f, FVector(24.0f, 10.0f, -6.0f) };	// kunai tipped up and in, toward the crosshair
	case EMatsuriHero::Mio:		return { TEXT("SM_Weapon_Suzu"), FRotator(0.0f, 20.0f, 0.0f), 0.55f, FVector(21.0f, 11.0f, -7.0f) };
	case EMatsuriHero::Count:	break;
	}
	return {};
}

bool Matsuri::ArtTable::IsBow(EMatsuriHero Hero)
{
	return Hero == EMatsuriHero::Sakura;
}

FMatsuriArtPose Matsuri::ArtTable::Shot(EMatsuriHero Hero)
{
	// Meshes lie along +X, head first (the arrow is turned that way by normalize_matsuri.py "align").
	switch (Hero)
	{
	case EMatsuriHero::Sakura:	return { TEXT("SM_Weapon_Arrow"), FRotator::ZeroRotator, 0.75f };	// a 64 cm hama-ya
	case EMatsuriHero::Suzune:	return { TEXT("SM_Weapon_Shuriken"), FRotator::ZeroRotator, 1.2f };	// the kunai, tumbling
	case EMatsuriHero::Mio:		return { TEXT("SM_Weapon_Suzu"), FRotator::ZeroRotator, 1.3f };
	case EMatsuriHero::Count:	break;
	}
	return {};
}

FMatsuriArtPose Matsuri::ArtTable::ZeniCoin()
{
	return { TEXT("SM_Weapon_Ooban"), FRotator::ZeroRotator, 1.6f };
}
