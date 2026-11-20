# 鈴音の夜祭り · Suzune’s Night Festival

选一名女主，在竹林、屋台街和鸟居迷宫中收集祈愿钱、挑选御守，最后到月下本殿迎战九尾狐玉藻。《鈴音の夜祭り》是一个用 Unreal Engine 5.8 和 C++ 制作的单人第一人称动作游戏。

[下载 Windows 试玩版](https://github.com/jomify/suzune-matsuri/releases/latest) · [中文代码导读](docs/code-guide/README.md) · [Boss AI 设计](docs/boss_plan.md)

## 开始游戏

下载 `SuzuneMatsuri-Windows-x64.zip`，完整解压后运行 `SuzuneMatsuri.exe`。试玩版可以离线运行。源代码和试玩版使用程序化布景、占位模型和游戏音效。角色模型、剧情影片、场景配乐与 Fab 素材需要分别取得授权后导入。 [素材与许可](THIRD_PARTY_NOTICES.md)

| 操作 | 按键 |
| --- | --- |
| 移动、转向、跳跃 | `W A S D`、鼠标、`Space` |
| 射击、近战 | 鼠标左键、`V`。樱可按住左键蓄力 |
| 角色技能、大招 | `Shift`、`E`、`Q` |
| 钩索 | 在本殿对准发光挂环按 `F`，按 `Space` 提前松开 |
| 选择女主或御守 | `1`、`2`、`3` |

在倒计时结束之前达到目标分数，再从鸟居前往下一层。樱可以用蓄力弓贯穿目标，铃音擅长扇射和机动，澪的弹跳铃铛可以利用墙面。玉藻从命中、擦弹和自身血量损失获得反馈，使用 LinUCB 在六类弹幕及其参数组合中选招。开发者模式还可接入 DeepSeek 或 Ollama。 [Boss 源码导读](docs/code-guide/02-boss.md)

## 从源码运行

需要 Windows、Unreal Engine 5.8、Visual Studio 2022 的 C++ 游戏开发工具和 Windows SDK，以及 Git LFS。

```bash
git lfs install
git clone https://github.com/jomify/suzune-matsuri.git
cd suzune-matsuri
git lfs pull
```

用 UE 5.8 打开 `SuzuneMatsuri/SuzuneMatsuri.uproject`。打开 `/Game/Matsuri/Maps/L_Title` 并按 Play，可从主菜单进入游戏。以下命令在 Git Bash 中运行，脚本默认使用 `C:/Program Files/Epic Games/UE_5.8`。

```bash
bash tools/build.sh build
bash tools/ue_cmd.sh smoke '-ExecCmds=Automation RunTests SuzuneMatsuri.E2E.CoreLoop' '-TestExit=Automation Test Queue Empty' -UserDir=../logs/smoke-user -nullrhi
bash tools/package.sh Shipping
```

打包输出为 `Package/Windows/`。依赖可选影片、配乐或角色模型的测试需要相应素材。开发者模式可跳到指定关卡或展示弹幕。接入 DeepSeek 时，通过本机环境变量 `DEEPSEEK_API_KEY` 传入密钥。

项目自有代码采用 MIT 许可证，UE 示例和 Kenney 音效沿用各自的许可。 [MIT 许可证](LICENSE) · [素材与许可](THIRD_PARTY_NOTICES.md)
