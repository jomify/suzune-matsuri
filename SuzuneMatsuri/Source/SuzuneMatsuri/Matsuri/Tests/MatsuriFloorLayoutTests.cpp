// Suzune's Night Festival - floor layout unit tests.
// Run: Automation RunTests SuzuneMatsuri.Layout

#include "Misc/AutomationTest.h"
#include "MatsuriFloorLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags MatsuriTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int32 SeedsToSweep = 25;

	TArray<FBox2D> ObstacleFootprints(const FMatsuriFloorLayout& Layout)
	{
		TArray<FBox2D> Result;
		for (const FMatsuriBlockPlacement& Block : Layout.Blocks)
		{
			if (Block.Type == EMatsuriBlock::LowCover || Block.Type == EMatsuriBlock::Pillar)
			{
				Result.Add(Block.GetFootprint());
			}
		}
		return Result;
	}

	FMatsuriFloorSpec SpecWithSeed(int32 Seed)
	{
		FMatsuriFloorSpec Spec;
		Spec.Seed = Seed;
		return Spec;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriLayoutDeterministicTest, "SuzuneMatsuri.Layout.SameSeedSameLayout", MatsuriTestFlags)
bool FMatsuriLayoutDeterministicTest::RunTest(const FString&)
{
	const FMatsuriFloorLayout A = Matsuri::Layout::BuildFloorLayout(SpecWithSeed(7));
	const FMatsuriFloorLayout B = Matsuri::Layout::BuildFloorLayout(SpecWithSeed(7));
	const FMatsuriFloorLayout C = Matsuri::Layout::BuildFloorLayout(SpecWithSeed(8));

	TestEqual(TEXT("same coin count"), A.Coins.Num(), B.Coins.Num());
	for (int32 i = 0; i < A.Coins.Num() && i < B.Coins.Num(); ++i)
	{
		TestTrue(TEXT("same coin location"), A.Coins[i].Location.Equals(B.Coins[i].Location));
	}
	TestFalse(TEXT("different seed moves the first coin"), A.Coins[0].Location.Equals(C.Coins[0].Location));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriLayoutCountsTest, "SuzuneMatsuri.Layout.PlacesEveryCoinAndCanBeWon", MatsuriTestFlags)
bool FMatsuriLayoutCountsTest::RunTest(const FString&)
{
	for (int32 Seed = 1; Seed <= SeedsToSweep; ++Seed)
	{
		const FMatsuriFloorSpec Spec = SpecWithSeed(Seed);
		const FMatsuriFloorLayout Layout = Matsuri::Layout::BuildFloorLayout(Spec);

		TestEqual(FString::Printf(TEXT("seed %d koban"), Seed), Layout.CountCoins(EMatsuriCollectible::Koban), Spec.KobanCount);
		TestEqual(FString::Printf(TEXT("seed %d ginka"), Seed), Layout.CountCoins(EMatsuriCollectible::Ginka), Spec.GinkaCount);
		TestEqual(FString::Printf(TEXT("seed %d magatama"), Seed), Layout.CountCoins(EMatsuriCollectible::Magatama), Spec.MagatamaCount);
		TestTrue(FString::Printf(TEXT("seed %d target reachable"), Seed), Layout.TotalPoints() >= Spec.TargetScore);
		TestEqual(FString::Printf(TEXT("seed %d obstacles"), Seed), ObstacleFootprints(Layout).Num(), Spec.ObstacleCount);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriLayoutCoinsReachableTest, "SuzuneMatsuri.Layout.CoinsAreClearAndInBounds", MatsuriTestFlags)
bool FMatsuriLayoutCoinsReachableTest::RunTest(const FString&)
{
	for (int32 Seed = 1; Seed <= SeedsToSweep; ++Seed)
	{
		const FMatsuriFloorSpec Spec = SpecWithSeed(Seed);
		const FMatsuriFloorLayout Layout = Matsuri::Layout::BuildFloorLayout(Spec);
		const TArray<FBox2D> Obstacles = ObstacleFootprints(Layout);
		const FVector2D Start(Layout.PlayerStart);

		for (const FMatsuriCoinPlacement& Coin : Layout.Coins)
		{
			const FVector2D Spot(Coin.Location);
			TestTrue(TEXT("coin inside walls"), FMath::Abs(Spot.X) < Spec.HalfExtent && FMath::Abs(Spot.Y) < Spec.HalfExtent);
			TestTrue(TEXT("coin away from spawn"), FVector2D::Distance(Spot, Start) >= Matsuri::Layout::SpawnClearRadius);
			for (const FBox2D& Box : Obstacles)
			{
				TestFalse(TEXT("coin not buried in an obstacle"), Box.ExpandBy(Matsuri::Layout::CoinClearance).IsInside(Spot));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriLayoutWalkableTest, "SuzuneMatsuri.Layout.ObstaclesLeaveWalkableGaps", MatsuriTestFlags)
bool FMatsuriLayoutWalkableTest::RunTest(const FString&)
{
	constexpr float Tolerance = 1.0f;
	for (int32 Seed = 1; Seed <= SeedsToSweep; ++Seed)
	{
		const FMatsuriFloorSpec Spec = SpecWithSeed(Seed);
		const TArray<FBox2D> Obstacles = ObstacleFootprints(Matsuri::Layout::BuildFloorLayout(Spec));
		const float Inner = Spec.HalfExtent - Matsuri::Layout::ObstacleGap + Tolerance;

		for (int32 i = 0; i < Obstacles.Num(); ++i)
		{
			const FBox2D& A = Obstacles[i];
			TestTrue(TEXT("gap to walls"), A.Min.X >= -Inner && A.Max.X <= Inner && A.Min.Y >= -Inner && A.Max.Y <= Inner);
			for (int32 j = i + 1; j < Obstacles.Num(); ++j)
			{
				const FBox2D Grown = A.ExpandBy(Matsuri::Layout::ObstacleGap - Tolerance);
				TestFalse(TEXT("gap between obstacles"), Grown.Intersect(Obstacles[j]));
			}
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
