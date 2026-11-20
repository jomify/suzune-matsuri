# Tests：每个测试文件在验证什么

本章覆盖 `Source/SuzuneMatsuri/Matsuri/Tests/` 的全部 **38 个文件**。这里是代码职责导读，未在本次文档工作中重新运行测试；“测试检查某行为”不等于“本次已验证通过”。

`IMPLEMENT_SIMPLE_AUTOMATION_TEST` 注册一个测试类及其可运行名称，紧接着的 `类名::RunTest` 是测试入口。下表对每个注册类分别解释检查内容；E2E 的 RunTest 通常先把一组逐帧 lambda 排入队列，实际过程随后由 `FStepRunner::Update` 执行，所以入口返回 true 并不意味着异步步骤已全部通过。

普通 `SuzuneMatsuri.*` 包括纯逻辑和编辑器 PIE 测试。`MatsuriLive.*` 是单独的真实模型调用测试，需要网络/服务；`MatsuriPerf.*` 是需要渲染器的性能测量，不属于普通功能套件。截图类测试需要人工看画面，空渲染的通过不能证明视觉质量。普通 Boss E2E 使用测试记忆文件夹，许多会话使用 `MatsuriRecord_Test` 隔离纪录。

## 1. MatsuriTestKit.h：测试基础设施

源码：[MatsuriTestKit.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriTestKit.h)。仅在开发自动化测试且为编辑器构建时编译。

- `EStep`：`Wait` 等待前置条件且不加步骤帧数；`Continue` 继续执行并加帧；`Done` 进入下一步。
- `FCoinCollector`：保存当前钱币、拾取前分数和已拾取数，用真实重叠逐枚收集。
- `FStepRunner`：保存命名步骤、索引、帧数、超时和三个演出开关的共享恢复状态。

| 函数 | 作用 |
|---|---|
| `DoneIf` | 把布尔结果换成 Done 或 Continue。 |
| `PIEWorld` | 返回编辑器当前正在运行的 PlayWorld。 |
| `IsMap` | 根据地图包短名称判断世界属于哪张地图，兼容 PIE 前缀。 |
| `FindWidget<TWidget>` | 查找指定世界且正在视口中的控件实例。 |
| `FindButton` | 遍历控件树，按按钮内部文字找到对应按钮。 |
| `Hero` | 从第一玩家控制器取得 Character。 |
| `Run` | 从指定世界取得运行子系统。 |
| `TeleportHero` | 瞬移玩家，并重置所有 Boss 弹幕场的玩家扫掠起点，避免把瞬移路径算成碰弹。 |
| `ClearDirection` | 用球体扫掠检查 24 个水平方向，找一条指定距离内没有静态障碍的方向。 |
| `AimAt` | 从玩家视点算到目标的方向，设置控制器旋转；相机下一帧跟随。 |
| `CountFx` | 只统计正在活动的特效，不把池中闲置对象算进去。 |
| `CountCollectibles` | 统计当前钱币收集物 Actor。 |
| `SettleAndShoot` | 等待画面稳定；有渲染器时请求截图，再留数帧让截图写出，返回是否结束。 |
| `SendKey` | 通过游戏视口发送按键/鼠标事件，模拟真实输入入口，并返回视口是否接受。 |
| `ViewportAcceptsGameInput` | 检查视口存在且没有忽略游戏输入。 |
| `SetIntCVar` | 找到控制台变量后以代码方式设置整数。 |
| `SetStringCVar` | 找到控制台变量后设置字符串。 |
| `SetCVar` | 找到控制台变量后设置布尔值。 |
| `TestBossMemoryFolder` | 返回 Saved/BossMemory_Test。 |
| `UseTestBossBrain` | 把模型关掉、记忆目录设到测试文件夹，使普通测试离线且不污染玩家经验。 |
| `RestoreBossBrain` | 将提供者恢复为 deepseek、目录恢复默认空配置；这是代码预定值，非任意旧配置快照。 |
| `FCoinCollector::Tick` | 等待上枚钱币被销毁，核对实际加分；没达标则找下枚并瞬移过去，没币可收时记录错误。 |
| `FStepRunner::FStepRunner` | 接收测试/步骤和剧情、回放、大招电影开关；第一个 runner 保存旧值，按本次配置设置开关。 |
| `FStepRunner::~FStepRunner` | 最后一个 runner 销毁时恢复演出开关，避免多会话互相抢先恢复。 |
| `FStepRunner::Update` | 每次轮询执行当前步骤，处理 Wait/Continue/Done；40 秒超时或 PIE 意外结束时报告错误并停止，拒绝访问正在销毁的世界。 |

## 2. MatsuriArtGalleryTest.cpp

源码：[MatsuriArtGalleryTest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriArtGalleryTest.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriArtGalleryTest::RunTest` | `SuzuneMatsuri.Art.Gallery` | 将小物件、大物件、三英雄和 Niagara 效果排到镜头前截图，供人工检查比例/贴图/朝向；无资产可跳过，不能当作审美自动通过。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `LineUp` | 将指定静态模型沿相机前方一字排开，返回成功放置数量。 |
| `LineUpHeroes` | 生成三位英雄骨骼模型，调整尺寸并循环 Idle 动画。 |


## 3. MatsuriArtPlacementTest.cpp

源码：[MatsuriArtPlacementTest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriArtPlacementTest.cpp)。

数据结构：`FPiece` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriArtPlacementTest::RunTest` | `SuzuneMatsuri.Art.NoClipping` | 在三个普通楼层和五个种子建立临时世界，检查美术不穿墙/钱币/其他道具，装饰碰撞不堵妖怪通路；竹林和鸟居下钱币有有意例外。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `IsArt` | 判断静态模型是否来自 /Tripo/ 目录。 |
| `IsBamboo` | 识别竹子道具，用于允许竹林有意重叠。 |
| `IsArchOverCoin` | 识别鸟居跨在钱币上方的允许情况。 |
| `Gather` | 收集可见美术、实体障碍和钱币的包围盒。 |
| `DecorColliders` | 提取布局以外的装饰碰撞盒并按怪物半径扩张。 |
| `SegmentHitsBox` | 沿线段采样判断是否穿过二维盒，检测怪物通路被堵。 |
| `YokaiPaths` | 生成迷宫格子的门口到中心、门口到门口的可能怪物路径。 |


## 4. MatsuriAudioTests.cpp

源码：[MatsuriAudioTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriAudioTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriAudioTableTest::RunTest` | `SuzuneMatsuri.Audio.EverySoundIsImported` | 逐一检查音效定义都有对应可加载声音资产。 |
| `FMatsuriBowDrawSoundTest::RunTest` | `SuzuneMatsuri.Audio.BowDrawLastsAFullDraw` | 检查拉弓声音时长覆盖完整蓄力。 |


## 5. MatsuriBossBrainTests.cpp

源码：[MatsuriBossBrainTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriBossBrainTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriBossHealthMemoryTest::RunTest` | `SuzuneMatsuri.Boss.HealthMemoryCompatibility` | 检查新 HP 记录读写，以及旧 lives 格式仍能读取、被正确标成旧格式。 |
| `FMatsuriBossRewardTest::RunTest` | `SuzuneMatsuri.Boss.Reward` | 检查触碰上限 3、Boss 掉血惩罚和擦弹奖励最多 0.5。 |
| `FMatsuriBossRetrieveTest::RunTest` | `SuzuneMatsuri.Boss.RetrieveSimilar` | 检查选出 K 条相似经验，同时展示成功和失败案例，优先相同阶段；空记录不生成经验。 |
| `FMatsuriBossMemoryTest::RunTest` | `SuzuneMatsuri.Boss.MemoryOnDisk` | 在测试目录写回读回技能步骤、战斗摘要、经验笔记，检查参数/来源/原因/奖励未丢失。 |
| `FMatsuriBossLearnerAdaptsTest::RunTest` | `SuzuneMatsuri.Boss.LearnerAdapts` | 用合成奖励训练：面对蹲掩体学会雨、站高台学会高扫；检查阶段合法、保存加载和从记录重建。 |
| `FMatsuriBossPromptTest::RunTest` | `SuzuneMatsuri.Boss.Prompt` | 检查提示词含奖励、技能描述、允许列表、区域、旧笔记、历史案例 ICL、本局奖励 ICRL 和学习器建议。 |
| `FMatsuriBossParseTest::RunTest` | `SuzuneMatsuri.Boss.ParseModelReply` | 检查围栏 JSON、命名/位置参数和区域解析、参数夹紧；拒绝未知/阶段禁用技能和无 JSON 回答；检查经验解析。 |
| `FMatsuriBossRequestTest::RunTest` | `SuzuneMatsuri.Boss.ModelRequest` | 检查请求模型名、JSON 输出与思考开关、响应 token 统计、API 错误和无密钥/off 不请求。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `Situation` | 构造可控阶段、护盾、高处/区域的 Boss 观测。 |
| `StepWith` | 按观测、技能和奖励构造一条历史步骤。 |
| `ArenaZoneNames` | 取庭院所有区域名称供提示词/解析测试。 |


## 6. MatsuriBossCoreTests.cpp

源码：[MatsuriBossCoreTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriBossCoreTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriDanmakuShapesTest::RunTest` | `SuzuneMatsuri.Boss.DanmakuShapes` | 检查圆环缺口、扇形方向、扫墙网格和雨滴生成位置/数量。 |
| `FMatsuriDanmakuMotionTest::RunTest` | `SuzuneMatsuri.Boss.BulletMotion` | 检查延迟、追踪结束后直行、寿命和球形接触判断。 |
| `FMatsuriFastBulletCrossingHitsTest::RunTest` | `SuzuneMatsuri.Boss.FastBulletCrossingHits` | 检查低帧率下飞弹跨过玩家仍可由扫掠判命中。 |
| `FMatsuriDanmakuRelativeMotionTest::RunTest` | `SuzuneMatsuri.Boss.PlayerMotionCrossesBullet` | 检查玩家跨过静止弹也会命中，并取得相对线段的首个交点。 |
| `FMatsuriDanmakuTelegraphTest::RunTest` | `SuzuneMatsuri.Boss.TelegraphDoesNotDamage` | 检查延迟警告阶段不造成伤害，跨过激活时间时只推进活动段。 |
| `FMatsuriDanmakuCoverSweepTest::RunTest` | `SuzuneMatsuri.Boss.SweptCoverOrdering` | 检查薄掩体不被穿透，玩家前方掩体先挡弹、后方掩体不能抹掉已经发生的玩家命中。 |
| `FMatsuriBossSkillTableTest::RunTest` | `SuzuneMatsuri.Boss.SkillTable` | 检查六技能表、三个预设、阶段解锁、非法参数/区域和台词长度保护。 |
| `FMatsuriBossBulletDamageTableTest::RunTest` | `SuzuneMatsuri.Boss.BulletDamageByDensity` | 检查每技能稀疏/密集伤害范围，各数量参数增加时单发下降，单墙 36、双墙 28。 |
| `FMatsuriBossSkillProgramsTest::RunTest` | `SuzuneMatsuri.Boss.SkillPrograms` | 实际推进六技能程序，检查产生有效弹幕、来源技能/伤害保存在子弹中，以及目标区域/扫墙高度。 |
| `FMatsuriBossArenaLayoutTest::RunTest` | `SuzuneMatsuri.Boss.ArenaLayout` | 检查庭院起点、锚点、弹药、钩索环不在障碍内，区域高度识别和石灯掩护。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `TestView` | 构造固定 Boss/玩家/区域视图供弹幕纯逻辑测试。 |
| `RunSkill` | 启动并逐步推进一个技能程序，收集整个技能发出的子弹。 |


## 7. MatsuriBossDamageE2ETest.cpp

源码：[MatsuriBossDamageE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriBossDamageE2ETest.cpp)。

数据结构：`FDamageState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriBossDamageE2ETest::RunTest` | `SuzuneMatsuri.E2E.BossDamage` | 回归逐弹伤害、快弹扫掠、12/30 不同弹伤、非法伤害、仅视觉回放不伤人、预警延迟、无敌后重试、伤后保护、掩体顺序、瞬移不扫过中间空间、致命统计先于结算。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `DamageMode` | 取得伤害测试中的 Boss GameMode。 |
| `TestBullet` | 构造指定位置/速度的测试弹，使用测试设定伤害。 |
| `Chest` | 取得玩家躯干附近的弹幕命中中心。 |


## 8. MatsuriBossE2ETest.cpp

源码：[MatsuriBossE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriBossE2ETest.cpp)。

数据结构：`FBossState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriBossE2ETest::RunTest` | `SuzuneMatsuri.E2E.Boss` | 检查庭院环/栅栏、钩索落地、栅栏挡弹破坏、玩家 HP、离线学习器出招/隔离记录、换阶段清弹/保护和最终胜利。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `Mode` | 取得当前 Boss GameMode。 |
| `Slow` | 构造慢速、便于观测的测试子弹。 |
| `Feet` | 取得玩家脚下位置，用于钩索/落地判断。 |


## 9. MatsuriBossLiveTest.cpp

源码：[MatsuriBossLiveTest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriBossLiveTest.cpp)。

数据结构：`FLiveState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriBossLiveTest::RunTest` | `MatsuriLive.BossModel` | 真正调用模型做一次决策和一次战后反思，验证合法行动/经验 JSON 并记录耗时 token；需要连接和凭证，独立于普通套件。 |
| `FMatsuriBossReflectLatestTest::RunTest` | `MatsuriLive.BossReflectLatest` | 读取真实 BossMemory 最后一战，真正请求反思并在成功后覆盖经验笔记；这是有网络/存档副作用的维护测试。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `Camper` | 构造蹲石灯掩体的玩家场景供真实模型测试。 |
| `Past` | 构造指定技能、奖励、触碰和 Boss 掉血的历史案例。 |


## 10. MatsuriChaseE2ETest.cpp

源码：[MatsuriChaseE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriChaseE2ETest.cpp)。

数据结构：`FChaseState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriChaseE2ETest::RunTest` | `SuzuneMatsuri.E2E.Chase` | 在真实迷宫距玩家五格走廊处依次生成三种怪，检查绕墙接近；河童到投掷射程即可。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `Maze` | 取得当前普通关卡 GameMode 的迷宫。 |
| `StepsBetween` | 将两个世界位置转成迷宫格，查询最短走廊步数。 |
| `SpawnAroundCorners` | 在距玩家五格走廊的格子门附近生成指定妖怪。 |
| `ChaseStep` | 返回一个逐帧追逐测试步骤，统计最短距离；超时输出附近碰撞物帮助诊断。 |


## 11. MatsuriCinematicE2ETest.cpp

源码：[MatsuriCinematicE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriCinematicE2ETest.cpp)。

数据结构：`FState`、`FPlaybackState`、`FMovieState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriCinematicFailureE2ETest::RunTest` | `SuzuneMatsuri.E2E.CinematicFailureAndSkip` | 检查缺电影立即返回 false、不执行异步回调、不暂停；损坏临时文件异步失败与跳过分别只回调一次且恢复暂停。 |
| `FMatsuriCinematicPlaybackE2ETest::RunTest` | `SuzuneMatsuri.E2E.CinematicPlayback` | 有渲染时实际打开第一关电影，检查播放暂停游戏、Skip 按钮存在、一次回调及恢复游戏。 |
| `FMatsuriCinematicFiveMoviesE2ETest::RunTest` | `SuzuneMatsuri.E2E.CinematicFiveMovies` | 有渲染时依次实际打开五段剧情电影并截图、跳过，检查每段都只完成一次和恢复游戏。 |


## 12. MatsuriCinematicTests.cpp

源码：[MatsuriCinematicTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriCinematicTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriCinematicPathsTest::RunTest` | `SuzuneMatsuri.Cinematic.Paths` | 检查五段剧情电影的枚举路径和 Content 绝对路径映射。 |


## 13. MatsuriCombatE2ETest.cpp

源码：[MatsuriCombatE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriCombatE2ETest.cpp)。

数据结构：`FCombatState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriCombatE2ETest::RunTest` | `SuzuneMatsuri.E2E.Combat` | PIE 中检查射击/敌人血量/掉落/弹药、空枪反馈、近战范围击退硬直、黄瓜格挡、贴脸命中、命中/击杀标记和音画效果。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `Combat` | 取得玩家战斗组件。 |
| `SpawnInFront` | 在玩家视线前指定距离生成妖怪。 |
| `SpawnAlong` | 沿指定方向生成妖怪并配置范围。 |
| `ShootUntilDead` | 按武器冷却持续尝试射击，直到目标消失或测试继续等待。 |


## 14. MatsuriCoreLoopE2ETest.cpp

源码：[MatsuriCoreLoopE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriCoreLoopE2ETest.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriCoreLoopE2ETest::RunTest` | `SuzuneMatsuri.E2E.CoreLoop` | PIE 中主菜单开局、碰墙受阻、收集钱币达到目标并进入暂停胜利界面。 |


## 15. MatsuriDevModeE2ETest.cpp

源码：[MatsuriDevModeE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriDevModeE2ETest.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriDevModeE2ETest::RunTest` | `SuzuneMatsuri.E2E.DevMode` | 从主菜单打开开发面板，切无敌、选角色跳 Boss，验证 showcase 顺序且不记忆、真实 F7、F6 补给及 F5 胜利。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `Boss` | 取得当前测试世界里的 Boss。 |


## 16. MatsuriFloorLayoutTests.cpp

源码：[MatsuriFloorLayoutTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriFloorLayoutTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriLayoutDeterministicTest::RunTest` | `SuzuneMatsuri.Layout.SameSeedSameLayout` | 检查同种子钱币数量位置相同，不同种子会改变布局。 |
| `FMatsuriLayoutCountsTest::RunTest` | `SuzuneMatsuri.Layout.PlacesEveryCoinAndCanBeWon` | 检查钱币种类数量与设定一致，总分足够完成关卡。 |
| `FMatsuriLayoutCoinsReachableTest::RunTest` | `SuzuneMatsuri.Layout.CoinsAreClearAndInBounds` | 检查钱币在范围内、不埋入障碍、远离出生点。 |
| `FMatsuriLayoutWalkableTest::RunTest` | `SuzuneMatsuri.Layout.ObstaclesLeaveWalkableGaps` | 检查障碍与外墙/其他障碍之间留出通行间隙。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `ObstacleFootprints` | 把布局中的非地面障碍转换为二维占地盒。 |
| `SpecWithSeed` | 构造给定种子的楼层生成规格。 |


## 17. MatsuriFloorsTests.cpp

源码：[MatsuriFloorsTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriFloorsTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriFloorsEscalateTest::RunTest` | `SuzuneMatsuri.Floors.DifficultyEscalates` | 检查逐层目标提高、时间缩短、敌人增多，以及三层之后接 Boss。 |
| `FMatsuriFloorsBeatableTest::RunTest` | `SuzuneMatsuri.Floors.EveryFloorPlacesEverythingAndIsBeatable` | 检查各层都放足配置对象和可达目标所需分数，并检查不同种子产生变化。 |
| `FMatsuriFloorsFairStartTest::RunTest` | `SuzuneMatsuri.Floors.ThreatsStartAwayFromPlayer` | 检查怪物和陷阱离玩家出生点有安全距离。 |
| `FMatsuriMazeFloorsTest::RunTest` | `SuzuneMatsuri.Floors.MazeFloorsAreSoundAndFair` | 检查迷宫生成、开放门路不被墙堵、钱币在空地、怪物沿走廊离起点至少三格，开局朝向有门。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `IsThreat` | 判断生成类型是否属于怪物或危险物。 |
| `SegmentHitsBox` | 沿线段采样判断是否穿过二维盒，检测怪物通路被堵。 |


## 18. MatsuriHeroesTests.cpp

源码：[MatsuriHeroesTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriHeroesTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriRosterTest::RunTest` | `SuzuneMatsuri.Heroes.EveryHeroIsComplete` | 检查三英雄 ID、名字、武器、被动、弹药及三技能定义完整。 |
| `FMatsuriPassivesTest::RunTest` | `SuzuneMatsuri.Heroes.PassivesShapeTheRun` | 检查樱额外 50 HP、忍者速度/二段跳/弹药、澪银币加分等差异。 |
| `FMatsuriUltTest::RunTest` | `SuzuneMatsuri.Heroes.UltimateChargesAndSpends` | 检查拾取与伤害可充能，满充能才能消费，消费后归零。 |
| `FMatsuriZeniTest::RunTest` | `SuzuneMatsuri.Heroes.ZeniSpendsScore` | 检查投钱技能只能花已有分数，同时扣本层与全局分。 |
| `FMatsuriShotSpeedTest::RunTest` | `SuzuneMatsuri.Heroes.ShotsLandWithoutDelay` | 检查各武器弹速，避免走廊射击等待太久。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `RunWith` | 构造已选择指定英雄的运行状态。 |


## 19. MatsuriInputE2ETest.cpp

源码：[MatsuriInputE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriInputE2ETest.cpp)。

数据结构：`FInputState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriInputE2ETest::RunTest` | `SuzuneMatsuri.E2E.RealInput` | 从主菜单、御守、重试回到游戏后，通过真实视口发送 W 和鼠标键，检查移动与消耗弹药，防止 UIOnly 输入残留。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `FloorReady` | 确认已经切换到新的目标世界，且英雄和 HUD 就绪。 |
| `AddControlChecks` | 追加真实 W 移动、鼠标射击两个步骤，检查切关后的输入恢复。 |


## 20. MatsuriLivesE2ETest.cpp

源码：[MatsuriLivesE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriLivesE2ETest.cpp)。

数据结构：`FHealthState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriLivesE2ETest::RunTest` | `SuzuneMatsuri.E2E.LivesAndTimeUp` | 名字沿用旧 lives：当前检查妖怪扣 30 HP、短暂无敌、HP 归零结算、重试恢复、计时失败和隔离存档/JSON。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `FloorMode` | 取得普通楼层 GameMode。 |
| `TouchYokai` | 在玩家位置制造妖怪接触以触发真实碰撞伤害。 |


## 21. MatsuriMazeTests.cpp

源码：[MatsuriMazeTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriMazeTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriMazeConnectedTest::RunTest` | `SuzuneMatsuri.Maze.EveryCellIsReachable` | 检查所有格子都可通过迷宫通道到达。 |
| `FMatsuriMazeBorderTest::RunTest` | `SuzuneMatsuri.Maze.BorderStaysClosedAndWallsAgree` | 检查外圈封闭，相邻格子对同一面墙的记录一致。 |
| `FMatsuriMazeLoopsTest::RunTest` | `SuzuneMatsuri.Maze.LoopsAddAlternativeRoutes` | 检查无环迷宫是树，启用环路后增加可选通道。 |
| `FMatsuriMazeDeterministicTest::RunTest` | `SuzuneMatsuri.Maze.SameSeedSameMaze` | 检查同种子墙布局相同、不同种子有变化。 |
| `FMatsuriMazeCellMathTest::RunTest` | `SuzuneMatsuri.Maze.CellCoordinatesRoundTrip` | 检查格子中心和格子索引的往返换算、越界点夹紧。 |
| `FMatsuriMazePathTest::RunTest` | `SuzuneMatsuri.Maze.WaypointsLeadToTheTarget` | 检查下一路点会缩短到目标的走廊距离，同格时直接返回目标。 |
| `FMatsuriSteerAroundTest::RunTest` | `SuzuneMatsuri.Maze.YokaiStepAroundCrates` | 检查迎面绕箱、镜像方向选择、擦边滑动以及保持地面高度。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `MakeMaze` | 按种子/环路概率/广场数建立测试迷宫。 |


## 22. MatsuriPerfProbe.cpp

源码：[MatsuriPerfProbe.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriPerfProbe.cpp)。

数据结构：`FFrameTiming`、`FPerfState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriPerfProbe::RunTest` | `MatsuriPerf.Floors` | 性能探针：第一关冷/热两轮自动移动、转向、拉弓和近战；记录帧时均值/分位数/卡顿与 GPU profile，数字依赖机器。 |
| `FMatsuriBossPerfProbe::RunTest` | `MatsuriPerf.Boss` | 性能探针：离线 Boss 战中自动移动攻击，记录 30 秒帧时和最大子弹数；不把某台机器性能当通用断言。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `Percentile` | 排序副本后取指定分位数；空列表返回零。 |
| `Mean` | 计算一组帧时间中指定字段的平均值。 |
| `PassName` | 把轮次 0/1 转成 cold/warm 标签。 |
| `Report` | 输出帧数、分辨率、FPS、平均/分位/最大帧时及线程/GPU 耗时、卡顿数。 |
| `LastFrameTiming` | 读取 UE 上一帧发布的 Game/Render/RHI/GPU 计时。 |
| `Timed` | 包裹一次操作，超过 5 毫秒则记录慢调用。 |
| `Play` | 性能测试每帧执行转向、寻空地行走、拉弓放箭和周期近战。 |


## 23. MatsuriProgressionE2ETest.cpp

源码：[MatsuriProgressionE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriProgressionE2ETest.cpp)。

数据结构：`FRunState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriProgressionE2ETest::RunTest` | `SuzuneMatsuri.E2E.FloorProgression` | 依次通过三层、选择御守并进入 Boss，检查总分继承、层分清零、补满 HP、最终胜利和纪录。 |


## 24. MatsuriQualityTests.cpp

源码：[MatsuriQualityTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriQualityTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriQualityCapTest::RunTest` | `SuzuneMatsuri.Quality.CapsCinematicForPlay` | 检查高画质被限定到配置上限、低设置不被抬高、分辨率不改、-1 关闭上限。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `AllAt` | 构造所有画质组选同一级的测试配置。 |


## 25. MatsuriReplayE2ETest.cpp

源码：[MatsuriReplayE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriReplayE2ETest.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriReplayE2ETest::RunTest` | `SuzuneMatsuri.E2E.Replay` | 累积超过六秒真实历史后致命受伤，检查历史有界、暂停回放、使用历史帧、跳过幂等、分数不变和一次结算。 |
| `FMatsuriBossReplayE2ETest::RunTest` | `SuzuneMatsuri.E2E.BossReplay` | 先积累真实 Boss 弹幕历史再致命受伤，检查历史子弹在回放中应用、渲染截图和自动播完后结算。 |


## 26. MatsuriReplayTests.cpp

源码：[MatsuriReplayTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriReplayTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriReplayBufferTest::RunTest` | `SuzuneMatsuri.Replay.BoundedHistory` | 检查回放帧数/六秒时间窗上限、旧帧淘汰、时间采样和稀疏样本时的时间截断。 |
| `FMatsuriReplayBufferEmptyTest::RunTest` | `SuzuneMatsuri.Replay.EmptyHistory` | 检查空回放历史无帧、时长为零且采样为空。 |


## 27. MatsuriRunStateTests.cpp

源码：[MatsuriRunStateTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriRunStateTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriScoreRemoveTest::RunTest` | `SuzuneMatsuri.Score.RemovePointsClampsAtZero` | 检查扣分不低于零、忽略无效扣分且不撤销已完成状态。 |
| `FMatsuriRunHurtTest::RunTest` | `SuzuneMatsuri.Run.HurtCostsHealth` | 检查普通伤害扣 HP，致命伤害归零并结束，负伤害不生效。 |
| `FMatsuriRunDamagePreservesScoreTest::RunTest` | `SuzuneMatsuri.Run.DamagePreservesCoinScore` | 检查受伤保留已收集的钱币分数。 |
| `FMatsuriHealthRulesTest::RunTest` | `SuzuneMatsuri.Run.HealthRules` | 检查 200/250 初始 HP、治疗上限、御守增加最大 HP/治疗、换层补满；负数、NaN、无穷无效，死亡不能被普通治疗复活。 |
| `FMatsuriRunFoxTest::RunTest` | `SuzuneMatsuri.Run.FoxBlessingAbsorbsOneHitPerFloor` | 检查狐之祝福每层挡一次伤害，换层恢复次数。 |
| `FMatsuriRunAmmoTest::RunTest` | `SuzuneMatsuri.Run.AmmoRespectsCapAndBalance` | 检查初始弹药、补充上限、消耗和弹药御守容量。 |
| `FMatsuriRunCharmsTest::RunTest` | `SuzuneMatsuri.Run.CharmsChangeTheRules` | 检查御守改变钱币分、最大 HP、额外时间等，纯函数不修改输入。 |
| `FMatsuriOmamoriOfferTest::RunTest` | `SuzuneMatsuri.Run.OmamoriOfferIsDistinctAndDeterministic` | 检查每次三个不同候选，同种子同结果，而且有显示名称。 |
| `FMatsuriRecordTest::RunTest` | `SuzuneMatsuri.Run.RecordKeepsBest` | 检查最高分/最深楼层保留、局数胜场累加和新纪录判断。 |


## 28. MatsuriSceneTransitionsE2ETest.cpp

源码：[MatsuriSceneTransitionsE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriSceneTransitionsE2ETest.cpp)。

数据结构：`FTransitionState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriSceneTransitionsE2ETest::RunTest` | `SuzuneMatsuri.E2E.SceneTransitions` | 检查标题进关及楼层切换的离场遮罩、目标标题、到达全遮挡与自动揭开。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `FindLoading` | 找到目标世界中指定离场/到达类型的加载控件。 |
| `ArriveOnFloor` | 返回等待新楼层并检查到达标题/覆盖率/淡出的步骤。 |
| `LeaveFor` | 返回触发跳转并检查离场遮罩名称的步骤。 |


## 29. MatsuriScoreStateTests.cpp

源码：[MatsuriScoreStateTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriScoreStateTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriPointsTest::RunTest` | `SuzuneMatsuri.Score.PointsPerCollectible` | 检查小判、银币、勾玉分别提供 1、2、5 分。 |
| `FMatsuriScoreAccumulatesTest::RunTest` | `SuzuneMatsuri.Score.AccumulatesRunAndFloor` | 检查拾取同时增加本层分和全局分，更新目标进度。 |
| `FMatsuriScoreClearsOnceTest::RunTest` | `SuzuneMatsuri.Score.ReportsClearExactlyOnce` | 检查跨过目标时仅报告一次通关，之后继续加分不会重复结算。 |
| `FMatsuriScoreExactTargetTest::RunTest` | `SuzuneMatsuri.Score.ExactTargetClears` | 检查分数恰好等于目标也算通关。 |
| `FMatsuriScoreIgnoresInvalidTest::RunTest` | `SuzuneMatsuri.Score.IgnoresNonPositivePoints` | 检查零分和负分输入不会改变分数。 |
| `FMatsuriScoreFloorResetTest::RunTest` | `SuzuneMatsuri.Score.NewFloorKeepsRunScore` | 检查新楼层清空本层分、保留累计分并重置完成标记。 |


## 30. MatsuriShotPhysicsE2ETest.cpp

源码：[MatsuriShotPhysicsE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriShotPhysicsE2ETest.cpp)。

数据结构：`FShotState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriShotPhysicsE2ETest::RunTest` | `SuzuneMatsuri.E2E.ShotPhysics` | 三英雄对地射击，检查箭/苦无粘地姿态和铃铛反弹/音效，缺武器美术时部分粘地检查跳过。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `CountStuck` | 统计带 MatsuriStuck 标签的粘住弹，按需输出朝向。 |
| `AddHeroRun` | 为指定英雄建立独立 PIE 会话及该文件所需的测试步骤。 |


## 31. MatsuriShotPhysicsTests.cpp

源码：[MatsuriShotPhysicsTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriShotPhysicsTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriShotKindsTest::RunTest` | `SuzuneMatsuri.Shots.EachKindHasItsOwnPhysics` | 检查箭、苦无、铃铛、钱币、黄瓜各自的重力、转动、粘住/反弹规则。 |
| `FMatsuriShotDropTest::RunTest` | `SuzuneMatsuri.Shots.DropStaysFairAcrossACorridor` | 检查走廊距离中的弹道下坠范围，保证瞄准仍公平。 |
| `FMatsuriLobTest::RunTest` | `SuzuneMatsuri.Shots.LobLandsOnTheTarget` | 检查给定投掷目标的初始方向归一化，而且弹道能落到目标附近。 |


## 32. MatsuriSkillsE2ETest.cpp

源码：[MatsuriSkillsE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriSkillsE2ETest.cpp)。

数据结构：`FSkillState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriSkillsE2ETest::RunTest` | `SuzuneMatsuri.E2E.Skills` | 分别开启三英雄 PIE，检查武器/被动和九项 Shift/E/Q 技能：穿透/结界/祓除、扇形/瞬移/分身/影缝、磁吸/花分投钱/双倍掉落。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `FSkillState::Start` | `FSkillState::Start` 保存当前世界时间，开始技能步骤计时。 |
| `FSkillState::Since` | `FSkillState::Since` 返回自 Start 后经过的世界秒数，避免按固定帧率等待。 |
| `Skills` | 取得玩家技能组件。 |
| `Combat` | 取得玩家战斗组件。 |
| `FloorMode` | 取得普通楼层 GameMode。 |
| `SpawnNear` | 在英雄附近相对偏移处生成测试妖怪。 |
| `UseSkillWithFx` | 尝试释放技能，成功时检查活动特效数量增加。 |
| `CountAlive` | 统计测试目标弱引用中仍有效的对象数。 |
| `ClearYokai` | 清除当前测试中的妖怪；战术测试版本还清除投射物，避免场景互扰。 |
| `CountProjectiles` | 统计世界中玩家/怪物投射物 Actor 数量。 |
| `TargetLocation` | 取目标位置；目标已无效时退回玩家位置，让断言失败而不崩溃。 |
| `AddFloorReady` | 追加楼层就绪、测试存档、足够高目标分及视角初始化步骤。 |
| `Reset` | 返回清怪、回检查点、重置朝向和目标列表的步骤。 |
| `AddSakura` | 追加樱的被动、穿透蓄力箭、结界、祓除和大祓测试。 |
| `AddSuzune` | 追加忍者速度/二段跳、三枚投掷、瞬移挡墙、分身和大招强化测试。 |
| `AddMio` | 追加澪银币加分、铃铛穿透、招财磁吸、花分投钱和双倍掉落测试。 |
| `QueueHeroSession` | 设置英雄变量并排入加载、PIE、步骤执行、退出的会话。 |


## 33. MatsuriStoryE2ETest.cpp

源码：[MatsuriStoryE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriStoryE2ETest.cpp)。

数据结构：`FState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriStoryE2ETest::RunTest` | `SuzuneMatsuri.E2E.Story` | 检查第一关播电影、Boss 入口无文字/无剧情暂停、胜利电影后才显示最终结算。 |
| `FMatsuriDefeatCinematicE2ETest::RunTest` | `SuzuneMatsuri.E2E.DefeatCinematic` | 检查致命伤害后播放战败电影、无文字卡，再只结算一次并停在暂停 GameOver。 |


## 34. MatsuriStoryTests.cpp

源码：[MatsuriStoryTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriStoryTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriStoryChaptersTest::RunTest` | `SuzuneMatsuri.Story.Chapters` | 检查各楼层各角色章节有标题、目标、台词等有效数据。 |
| `FMatsuriStoryEndingTest::RunTest` | `SuzuneMatsuri.Story.GuardianEnding` | 检查结局明确修复结界，并给三位角色不同结尾台词。 |
| `FMatsuriStoryToggleTest::RunTest` | `SuzuneMatsuri.Story.DefaultEnabled` | 检查剧情开关默认开启；这不意味着正常流程使用文字卡。 |


## 35. MatsuriUltimateCinematicTests.cpp

源码：[MatsuriUltimateCinematicTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriUltimateCinematicTests.cpp)。

数据结构：`FState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriUltimateMoviePathsTest::RunTest` | `SuzuneMatsuri.Ultimate.MoviePaths` | 检查三位英雄各自映射到不同大招电影。 |
| `FMatsuriUltimateSakuraE2ETest::RunTest` | `SuzuneMatsuri.E2E.UltimateSakura` | 通过 RunUltimateMovie 检查樱的大招电影及战斗恢复。 |
| `FMatsuriUltimateSuzuneE2ETest::RunTest` | `SuzuneMatsuri.E2E.UltimateSuzune` | 通过 RunUltimateMovie 检查忍者的大招电影及战斗恢复。 |
| `FMatsuriUltimateMioE2ETest::RunTest` | `SuzuneMatsuri.E2E.UltimateMio` | 通过 RunUltimateMovie 检查澪的大招电影及战斗恢复。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `RunUltimateMovie` | 共用三英雄大招测试：未充满不播，满充后锁攻击/技能、不额外耗弹、真实媒体打开、限时结束并恢复战斗。 |


## 36. MatsuriViewModelE2ETest.cpp

源码：[MatsuriViewModelE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriViewModelE2ETest.cpp)。

数据结构：`FViewState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriViewModelE2ETest::RunTest` | `SuzuneMatsuri.E2E.ViewModel` | 对三英雄多视角检查持武器相对相机位置、弹从尖端发出、后坐和模板手臂隐藏；另查拉弓、箭搭弦和移速恢复。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `ViewModel` | 取得第一人称持武器组件。 |
| `OffsetFromCameraOf` | 把给定世界坐标换成相机局部坐标。 |
| `OffsetFromCamera` | 计算武器组件相对相机的局部偏移。 |
| `AddHeroRun` | 为指定英雄建立独立 PIE 会话及该文件所需的测试步骤。 |


## 37. MatsuriYokaiBrainTests.cpp

源码：[MatsuriYokaiBrainTests.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriYokaiBrainTests.cpp)。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriChochinBrainTest::RunTest` | `SuzuneMatsuri.Brain.ChochinStalksFromTheBlindSide` | 检查提灯被看时慢挪、不被看时追踪/突进，以及视角判断。 |
| `FMatsuriKasaBrainTest::RunTest` | `SuzuneMatsuri.Brain.KasaLurksAndPounces` | 检查唐伞在转角伏击、近处可见时扑击、远处或隔多墙时接近。 |
| `FMatsuriKappaBrainTest::RunTest` | `SuzuneMatsuri.Brain.KappaFightsFromCover` | 检查河童接近、探头、投掷、躲藏、等待状态，以及可达掩体/探头点选择。 |


## 38. MatsuriYokaiTacticsE2ETest.cpp

源码：[MatsuriYokaiTacticsE2ETest.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Tests/MatsuriYokaiTacticsE2ETest.cpp)。

数据结构：`FTacticsState` 保存该文件跨帧步骤用到的对象引用、计时、计数或阶段；是测试状态，不是游戏存档。

| 注册类与函数 | 运行名称 | 具体检查内容 |
|---|---|---|
| `FMatsuriYokaiTacticsE2ETest::RunTest` | `SuzuneMatsuri.E2E.YokaiTactics` | 实景人为放墙：检查提灯被看与背视速度差、河童投掷间隙藏身、唐伞墙后伏击和见面扑击。 |

| 本文件辅助函数 | 作用 |
|---|---|
| `SpawnAt` | 在指定世界位置生成战术测试妖怪。 |
| `PlaceBlock` | 生成有碰撞的方块作为可控墙体/掩体。 |
| `PlayerSees` | 从玩家眼睛向目标射线检测，判断是否有实体遮挡。 |
| `ClearYokai` | 清除当前测试中的妖怪；战术测试版本还清除投射物，避免场景互扰。 |

## 阅读测试的顺序

先读 `MatsuriScoreStateTests.cpp`、`MatsuriRunStateTests.cpp`，看最简单的“建立状态 → 调函数 → TestEqual”；再读 `MatsuriTestKit.h` 和 `MatsuriCoreLoopE2ETest.cpp`，理解逐帧步骤；最后读 BossDamage、Replay、Cinematic，它们覆盖异步、暂停和回调顺序等较难的问题。

特别留意：`MatsuriBossLiveTest.cpp` 的 `FMatsuriBossReflectLatestTest::RunTest` 会向当前配置服务发请求，并调用 `Store->SetLessons` 更新真实最新笔记；它和只用合成样例检查解析器的 `MatsuriBossBrainTests.cpp` 不同。性能探针会临时修改部分控制台开关，也应按专门测量流程使用。
