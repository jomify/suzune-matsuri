// Suzune's Night Festival - the redemption story chapters and ending.

#include "MatsuriStory.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "MatsuriStory"

namespace
{
	TAutoConsoleVariable<bool> CVarStory(
		TEXT("matsuri.Story"), true,
		TEXT("Play the chapter and ending cinematics. 0 skips cinematic story playback."));

	FMatsuriStoryLine Line(const FText& Speaker, const FText& Text)
	{
		FMatsuriStoryLine Result;
		Result.Speaker = Speaker;
		Result.Text = Text;
		return Result;
	}

	FText HeroName(EMatsuriHero Hero)
	{
		return Matsuri::Heroes::Get(Hero).Name;
	}

	FText HeroVoice(EMatsuriHero Hero, const TCHAR* Sakura, const TCHAR* Suzune, const TCHAR* Mio)
	{
		switch (Hero)
		{
		case EMatsuriHero::Sakura: return FText::FromString(Sakura);
		case EMatsuriHero::Suzune: return FText::FromString(Suzune);
		default: return FText::FromString(Mio);
		}
	}

	FMatsuriStoryChapter Chapter(int32 FloorIndex, EMatsuriHero Hero)
	{
		FMatsuriStoryChapter Result;
		const FText HeroText = HeroName(Hero);
		if (FloorIndex <= 0)
		{
			Result.Title = LOCTEXT("FloorOneTitle", "第一幕・失われた願い");
			Result.Objective = LOCTEXT("FloorOneObjective", "在夜祭崩坏前，找回散落各处的愿钱。");
			Result.Lines = {
				Line(LOCTEXT("Narrator", "祭りの声"), LOCTEXT("FloorOneLine", "灯笼一盏接一盏熄灭，原本献给村子的愿望正在迷宫里流失。")),
				Line(HeroText, HeroVoice(Hero, TEXT("愿钱不会自己消失。我们把它们带回去。"), TEXT("是谁偷走了愿望？边走边查清楚吧。"), TEXT("愿望掉得到处都是……捡回来就能开宴会了吧？"))),
				Line(LOCTEXT("ShrineBell", "神社の铃"), LOCTEXT("FloorOneClue", "还有三座鸟居亮着微光。沿着愿钱的踪迹走，夜祭或许还能醒来。"))
			};
		}
		else if (FloorIndex == 1)
		{
			Result.Title = LOCTEXT("FloorTwoTitle", "第二幕・追う影");
			Result.Objective = LOCTEXT("FloorTwoObjective", "找回第二段愿钱踪迹，查明妖怪为何失去理智。");
			Result.Lines = {
				Line(LOCTEXT("Kasa", "唐傘"), LOCTEXT("FloorTwoLine", "我们不是来追你的，是破碎的愿望在每条走廊里呼唤我们。")),
				Line(HeroText, HeroVoice(Hero, TEXT("妖怪也在受苦。不能只靠射击结束这一切。"), TEXT("敌人的动作不对劲。有人把它们困在这里。"), TEXT("既然它们追过来，就先给大家找一条退路吧。"))),
				Line(LOCTEXT("FoxSign", "狐之印"), LOCTEXT("FloorTwoClue", "狐印指向内殿，碑文写着：把村民留在结界之外。"))
			};
		}
		else if (FloorIndex == 2)
		{
			Result.Title = LOCTEXT("FloorThreeTitle", "第三幕・結界の内側");
			Result.Objective = LOCTEXT("FloorThreeObjective", "抵达最后的鸟居，收集足够的愿钱去面对守护者。");
			Result.Lines = {
				Line(LOCTEXT("MikoRecord", "古老记录"), LOCTEXT("FloorThreeLine", "愿望变成毒物后，玉藻封锁了本殿。正是这道封印保护了村子。")),
				Line(HeroText, HeroVoice(Hero, TEXT("原来玉藻一直在守护大家？那我们去听她解释吧。"), TEXT("既然她有封锁这里的理由，我就亲自看到最后。"), TEXT("既然是在保护大家，就该早点说嘛。我肚子也饿了！"))),
				Line(LOCTEXT("NightFestival", "夜祭的记忆"), LOCTEXT("FloorThreeClue", "最后的鸟居只会为愿意把愿望送回去、而不是据为己有的人打开。"))
			};
		}
		else
		{
			Result.Title = LOCTEXT("BossTitle", "终幕・本殿・月下");
			Result.Objective = LOCTEXT("BossObjective", "通过玉藻的弹幕试炼，与她一起修复夜祭结界。");
			Result.Lines = {
				Line(LOCTEXT("Tamamo", "九尾狐 玉藻"), LOCTEXT("BossLine", "我封锁本殿，是为了保护村民。若你能不被愿望吞噬，把它们送回原处，我就会打开通往夜祭的路。")),
				Line(HeroText, HeroVoice(Hero, TEXT("我接受试炼。愿望和村子，我会一起守住。"), TEXT("无论是弹幕还是对话，我都会走到你面前。"), TEXT("打赢之后，就一起把夜祭办回来吧！"))),
				Line(LOCTEXT("TamamoPromise", "玉藻"), LOCTEXT("BossPromise", "那么就让月亮见证吧。这些弹幕考验的是守护之心，不是惩罚。"))
			};
		}
		return Result;
	}
}

bool Matsuri::Story::IsEnabled()
{
	return CVarStory.GetValueOnGameThread();
}

FMatsuriStoryChapter Matsuri::Story::ChapterFor(int32 FloorIndex, EMatsuriHero Hero)
{
	return Chapter(FMath::Clamp(FloorIndex, 0, 3), Hero);
}

FMatsuriStoryChapter Matsuri::Story::EndingFor(EMatsuriHero Hero)
{
	FMatsuriStoryChapter Result;
	Result.Title = LOCTEXT("EndingTitle", "终幕・愿火重燃");
	Result.Objective = LOCTEXT("EndingObjective", "把愿钱送回祭坛，与玉藻共同修复结界。");
	const FText HeroText = HeroName(Hero);
	Result.Lines = {
		Line(LOCTEXT("TamamoEnding", "玉藻"), LOCTEXT("EndingLine", "愿望终于安静下来。我守住了村民，却无法独自让夜祭重新点燃。")),
		Line(HeroText, HeroVoice(Hero, TEXT("愿火由大家一起守护。明年，我们再来。"), TEXT("结界不该由一个人独自支撑。下次，我们一起守护。"), TEXT("愿望回来了，那摊位也会回来吧？说好了！"))),
		Line(LOCTEXT("NarratorEnding", "祭り的声音"), LOCTEXT("EndingLastLine", "鸟居再次亮起，夜祭重新开始。这一次，守护它的不再只有一个人。"))
	};
	return Result;
}

#undef LOCTEXT_NAMESPACE
