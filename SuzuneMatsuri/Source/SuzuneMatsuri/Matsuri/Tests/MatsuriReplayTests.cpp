// Suzune's Night Festival - pure death replay buffer tests.

#include "Misc/AutomationTest.h"
#include "MatsuriReplay.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriReplayBufferTest, "SuzuneMatsuri.Replay.BoundedHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriReplayBufferTest::RunTest(const FString&)
{
	FMatsuriReplayBuffer Buffer;
	for (int32 Index = 0; Index < 120; ++Index)
	{
		FMatsuriReplayFrame Frame;
		Frame.Time = Index / 15.0f;
		Frame.PlayerTransform.SetLocation(FVector(static_cast<float>(Index), 0.0f, 0.0f));
		Buffer.Add(MoveTemp(Frame));
	}
	TestEqual(TEXT("history is bounded to six seconds at 15 Hz"), Buffer.Num(), FMatsuriReplayBuffer::DefaultMaxFrames);
	TestTrue(TEXT("oldest frame was discarded"), Buffer.At(0) && FMath::IsNearlyEqual(Buffer.At(0)->Time, 30.0f / 15.0f));
	TestTrue(TEXT("duration is bounded"), Buffer.Duration() < 6.0f);
	const FMatsuriReplayFrame* Sample = Buffer.Sample(4.55f);
	TestTrue(TEXT("sample chooses a recorded frame"), Sample && Sample->PlayerTransform.GetLocation().X >= 68.0f && Sample->PlayerTransform.GetLocation().X <= 69.0f);
	FMatsuriReplayBuffer Sparse;
	for (const float Time : { 0.0f, 2.0f, 4.0f, 8.0f })
	{
		FMatsuriReplayFrame Frame;
		Frame.Time = Time;
		Sparse.Add(MoveTemp(Frame));
	}
	TestTrue(TEXT("time span is capped even when samples are sparse"), Sparse.Duration() <= 6.0f && Sparse.At(0) && FMath::IsNearlyEqual(Sparse.At(0)->Time, 2.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMatsuriReplayBufferEmptyTest, "SuzuneMatsuri.Replay.EmptyHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMatsuriReplayBufferEmptyTest::RunTest(const FString&)
{
	FMatsuriReplayBuffer Buffer;
	TestEqual(TEXT("empty history has no frames"), Buffer.Num(), 0);
	TestTrue(TEXT("empty history duration is zero"), FMath::IsNearlyZero(Buffer.Duration()));
	TestNull(TEXT("empty history has no sample"), Buffer.Sample(0.0f));
	return true;
}

#endif

