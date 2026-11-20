# Core：一局游戏的状态、流程和公共服务

本章对应 `SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/` 的全部 **37 个源文件**。按同名 `.h`（声明、数据形状、可调用接口）和 `.cpp`（执行步骤）合并讲解；函数名保留原样，方便搜索。这里解释的是当前源码，不代表本次重新运行了游戏。

先区分三个层次：`FMatsuriRunState` 是“这一局的数据和计算规则”；`UMatsuriRunSubsystem` 是跨地图仍然存在的管理者，保存这些数据并发通知；`AMatsuriFloorGameMode` 是每张关卡的现场导演，创建场景、倒计时、接收伤害和安排结算。角色组件、拾取物和 UI 都会与这三层协作。

## 1. MatsuriTypes.h / MatsuriTypes.cpp：公共种类和基础分值

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriTypes.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriTypes.cpp)。

`.h` 定义 `EMatsuriCollectible`：小判金币、银币、勾玉；定义基础分值 1、2、5，以及标题地图路径 `Matsuri::Maps::Title`。`.cpp` 负责将种类转换为分值。

| 函数 | 作用 |
|---|---|
| `Matsuri::PointsFor(Kind)` | 返回该收集物的基础分值。这里不计算角色或御守加成；非法枚举会触发开发期检查，兜底返回 0。 |

## 2. MatsuriHeroes.h / MatsuriHeroes.cpp：三个角色的配置表

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriHeroes.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriHeroes.cpp)。

`.h` 定义角色枚举 `EMatsuriHero`、技能槽 `EMatsuriSkillSlot`，以及三个配置结构：`FMatsuriWeaponDef` 放弹容量、射速、伤害、弹速、散射、蓄力、穿透、弹跳、颜色；`FMatsuriSkillDef` 放名字、说明、冷却；`FMatsuriHeroDef` 把武器、被动和三个技能组合起来。这里是配置，不直接生成子弹或执行技能。大招每分充能 `0.02`，每点造成伤害充能 `0.04`。

| 函数 | 作用 |
|---|---|
| `FMatsuriHeroDef::Skill(Slot)` | 用 Shift、E、Q 对应的数组下标取技能定义，返回引用。 |
| `MakeSakura()` | 组装巫女配置：额外 50 HP、12 发箭容量、蓄力穿透弓，以及结界、驱邪、大祓的说明和冷却。 |
| `MakeSuzune()` | 组装忍者配置：移速加 15%、二段跳、30 发容量的三发扇形投射物，以及瞬身、分身、影缝。 |
| `MakeMio()` | 组装猫又配置：银币加分、20 发容量的弹跳铃弹，以及招财、投钱、吸取掉落的大招。 |
| `Matsuri::Heroes::Get(Hero)` | 首次调用时构建静态角色表，此后返回其中一项；把枚举下标限制在合法范围。 |

## 3. MatsuriOmamori.h / MatsuriOmamori.cpp：关间御守升级

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriOmamori.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriOmamori.cpp)。

`.h` 的 `EMatsuriOmamori` 定义八种御守；`FMatsuriModifiers` 保存累计加成：金币分值、伤害、弹容量、攻击间隔、移速、额外时间、每关免伤次数、最大生命加值。常量定义具体倍率，例如伤害 `1.25`、攻击间隔 `0.8`、健康御守 `50 HP`。`.cpp` 负责叠加和抽选。

| 函数 | 作用 |
|---|---|
| `Matsuri::Omamori::Apply(Base, Charm)` | 复制原加成并叠加一种御守，返回新结构；健康御守在这里增加最大 HP，实际回血由 RunState 执行。 |
| `RollOffer(RunSeed, FloorIndex, Size)` | 用局种子和楼层构造确定性随机流，洗牌八种御守，返回指定数量且互不重复的选项；相同种子和楼层得到相同结果。 |
| `DisplayName(Charm)` | 返回御守在界面显示的名字。 |
| `Effect(Charm)` | 返回御守效果说明，不执行效果。 |

## 4. MatsuriScoreState.h / MatsuriScoreState.cpp：本关分数与整局分数

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriScoreState.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriScoreState.cpp)。

`FMatsuriScoreState` 保存累计 `RunScore`、本层 `FloorScore`、目标 `FloorTarget`、是否已过关 `bFloorCleared`。它只是数据计算，不加载下一关。

| 函数 | 作用 |
|---|---|
| `ResetRun()` | 赋回默认结构，清空整局和本关计分状态。 |
| `BeginFloor(Target)` | 保留整局分数，清空本关分数与过关标记；目标至少为 1。 |
| `AddPoints(Points)` | 正分数同时加到两种分数；仅第一次达到本关目标时返回 `true`，避免反复触发过关。 |
| `RemovePoints(Points)` | 从整局、本关扣分，各自不低于 0，返回整局实际扣除量；已经过关的标记不会撤销。 |
| `GetFloorProgress()` | 返回本关分数除以目标，并限制在 0～1，供进度显示使用。 |

## 5. MatsuriRunState.h / MatsuriRunState.cpp：一局游戏的规则

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriRunState.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriRunState.cpp)。

`FMatsuriRunState` 保存角色、种子、楼层、分数、御守与加成、HP、弹药、大招能量和剩余祝福。`EMatsuriHurtResult` 区分无效伤害、祝福吸收、受伤、死亡；`EMatsuriRunEnd` 区分未结束、生命耗尽、超时、胜利。基础生命 200，上限 600，猫又投钱消耗 5 分。

| 函数 | 作用 |
|---|---|
| `Reset(InSeed, InHero)` | 重置整局，设置种子和角色，按该角色最大 HP 满血，弹药从容量的一半开始。 |
| `GetHero()` | 根据当前角色枚举返回角色配置。 |
| `BeginFloor(TargetScore)` | 重置本关分数目标，补充每关狐之祝福，恢复到当前最大 HP。 |
| `AdvanceFloor()` | 楼层索引加 1；本函数不打开地图。 |
| `PointsFor(Kind)` | 计算拾取物真实得分：基础值加金币御守加成或猫又银币被动。 |
| `Collect(Kind)` | 按实际得分给大招充能并记分；返回该次拾取是否首次达到本关目标。 |
| `AddDamageDealt(Damage)` | 用造成的非负伤害增加大招能量，能量限制在 0～1。 |
| `IsUltReady()` | 判断大招能量是否达到 1。 |
| `ConsumeUlt()` | 满能量才成功并归零，否则不改动。 |
| `SpendScore(Amount)` | 金额为正且整局分数足够才扣分，返回是否成功。 |
| `Hurt(Damage)` | 拒绝非有限、非正伤害和已死亡角色；有祝福先消耗一次祝福，否则扣 HP，返回受伤或死亡；不扣分。 |
| `GetMaxHealth()` | `200 + 角色额外生命 + 御守最大生命加成`，最高 600。 |
| `Heal(Amount)` | 活着且未满血时治疗，拒绝无效数值，返回实际回血量；不能复活。 |
| `GetAmmoCap()` | 武器基础容量乘御守容量倍率，再取整。 |
| `AddAmmo(Amount)` | 加弹至容量上限，返回实际增加数。 |
| `ConsumeAmmo(Amount)` | 只有消耗量为正且弹药足够时扣除，返回是否成功。 |
| `TakeCharm(Charm)` | 记录御守并应用叠加；健康御守先增加最大生命，再治疗 50。 |

这层不负责“0.35 秒无敌”“受伤声音”“死亡镜头”，这些依赖世界时间和 Actor，由关卡 GameMode 处理。

## 6. MatsuriRecord.h / MatsuriRecord.cpp：历史成绩的计算

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriRecord.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriRecord.cpp)。

`FMatsuriRecord` 只有最高分、到达最深层、完成局数和胜利次数。它与当前一局的 RunState 不同，也不含 Boss 学习记录。

| 函数 | 作用 |
|---|---|
| `Matsuri::Record::WithRun(Record, RunScore, FloorReached, bVictory)` | 返回纳入一局结果的新记录：取更高分、更深层，局数加 1，胜利时胜场加 1；负分和负楼层按 0 处理。 |
| `IsNewBest(Record, RunScore)` | 严格大于原最高分才算新纪录，平分不算。 |

## 7. MatsuriSaveGame.h：磁盘存档容器

源码：[MatsuriSaveGame.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriSaveGame.h)。

仅定义 `UMatsuriSaveGame : USaveGame`，用一个 `UPROPERTY` 保存 `FMatsuriRecord Record`。**没有项目自定义函数，也没有同名 `.cpp`**；真正读取和写入在 RunSubsystem。`.sav` 是正式来源，旁边 `.json` 仅供人阅读。

## 8. MatsuriRunSubsystem.h / MatsuriRunSubsystem.cpp：跨地图管理和通知

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriRunSubsystem.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriRunSubsystem.cpp)。

`UMatsuriRunSubsystem : UGameInstanceSubsystem` 随 GameInstance 存活，所以从 1F 切换到 2F 时仍持有整局状态；它不是每帧重新创建的关卡对象。四个事件分别通知状态改变、目标达成、受伤结果、整局结束。它拥有 `Run`、`Record`、存档槽名和结束标记。

| 函数 | 作用 |
|---|---|
| `RandomSeed()`（内部） | 从当前时间 ticks 哈希生成非零种子。 |
| `Initialize(Collection)` | 调用父类初始化并读取历史记录。 |
| `DefaultHero()` | 读 `matsuri.Hero`，限制为三个合法角色之一；用于直接运行关卡。 |
| `DefaultSeed()` | 读 `matsuri.Seed`；0 表示后续随机生成。 |
| `StartNewRun(Hero, Seed)` | 重置 Run，设置进行中标记、清空上次结束结果并广播；不在此打开地图。 |
| `BeginFloor()` | 按当前关卡规格重置本关状态，并广播。 |
| `GetCurrentFloorSpec()` | 从楼层表获得规格，加入御守额外时间，再应用 `matsuri.Maze` 开关。 |
| `Collect(Kind)` | 委托 RunState 记分，先发状态通知；若本次首次达标，再发过关事件。 |
| `HurtPlayer(Damage)` | 委托 RunState 扣血/消耗祝福；无效伤害直接返回，否则广播状态及受伤结果；不在这里自动结束一局。 |
| `HealPlayer(Amount)` | 委托治疗，只有真的恢复 HP 才广播，返回实际治疗量。 |
| `AddAmmo(Amount)` | 委托补弹，实际增加时广播。 |
| `ConsumeAmmo(Amount)` | 委托扣弹，成功时广播。 |
| `AddDamageDealt(Damage)` | 通过伤害给大招充能，再广播。 |
| `AddUltCharge(Amount)` | 直接增减能量并限制 0～1，供擦弹、调试和测试等调用。 |
| `ConsumeUlt()` | 请求清空满额能量，广播并返回是否使用成功。 |
| `SpendScore(Amount)` | 请求消费分数，广播并返回结果。 |
| `StartOnFloor(FloorIndex)` | 把当前楼层限制到有效范围并设置，供直接运行 Boss 等地图使用；名字中的 Start 不会重新 Reset 一局。 |
| `SetFloorTargetForTesting(Target)` | 调试/测试专用：只改目标值，不清分数。 |
| `GetOmamoriOffer()` | 按本局种子和当前楼层取固定的御守候选。 |
| `ChooseOmamoriAndAdvance(Charm)` | 取得御守，楼层索引加 1，再广播；实际换图由 UI 流程调用。 |
| `EndRun(Reason)` | 只结算一次：关闭进行中标记，记下结束原因，计算是否新高分，更新并保存历史，发结束事件。 |
| `GetRunState()` | 只读返回整局状态引用。 |
| `GetScoreState()` | 只读返回 Run 中的分数状态。 |
| `GetRecord()` | 只读返回历史记录。 |
| `GetLastRunEnd()` | 返回最近结束原因。 |
| `WasNewBest()` | 返回最近一局是否刷新最高分。 |
| `IsRunActive()` | 返回当前是否有未结算的一局。 |
| `UseRecordSlot(SlotName)` | 更换存档槽并重新读取；测试用隔离槽，避免污染玩家成绩。 |
| `Broadcast()`（私有） | 统一发出 `OnRunChanged(Run)`。 |
| `LoadRecord()`（私有） | 先清空 Record；存在可读 `.sav` 就加载，否则保留空记录，并记录日志。 |
| `SaveRecord()`（私有） | 创建 SaveGame 对象并写 `.sav`，成功后再写便于查看的 JSON 副本；写入失败会日志报告。 |
| `Get(WorldContext)`（静态） | 从任意带世界的对象找到 GameInstance，再取得这个子系统；上下文无效时返回空指针。 |

## 9. MatsuriFloorGameMode.h / MatsuriFloorGameMode.cpp：关卡的现场导演

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriFloorGameMode.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriFloorGameMode.cpp)。

`.h` 定义 `AMatsuriFloorGameMode`，持有倒计时、无敌截止时间、双倍掉落截止时间、迷宫、出生点、回放导演和 UI 类型；提供供 Boss 子类覆盖的关卡构建/结束钩子。`.cpp` 将 Level、Combat、RunSubsystem、视频与 UI 连接起来。

| 函数 | 作用 |
|---|---|
| `AMatsuriFloorGameMode()` | 开启非暂停 Tick，找到第一人称角色与控制器蓝图，设置 HUD、过关和失败界面类型。 |
| `ChoosePlayerStart_Implementation(Player)` | 先确保场景已创建，再优先返回生成的出生点。 |
| `SetPlayerDefaults(PlayerPawn)` | 按需给玩家安装开发、战斗、技能组件；根据角色与御守设置移速、跳跃次数，并装备视角武器。 |
| `BeginPlay()` | 应用画质上限、确保建关、订阅达标事件、开始本关、设置计时、生成回放导演；恢复游戏输入、显示 HUD 和入场卡，前三关按开关播放对应剧情视频。 |
| `EndPlay(EndPlayReason)` | 解除对跨地图 Subsystem 的事件绑定，防止旧关卡仍被回调。 |
| `Tick(DeltaSeconds)` | 只在关卡活跃时扣倒计时；归零进入超时失败流程。 |
| `EnsureFloorBuilt()` | 用标记保证只建一次；预加载美术、音效和特效池；若直接运行地图且没有活动局，创建新局并定位楼层，随后调用可覆盖的构建函数。 |
| `BuildFloorContent(World, FloorIndex, Spec, Theme)` | 生成布局，保存迷宫，再创建灯光环境、墙地板、装饰、收集物、敌人/陷阱/补给和出生点。 |
| `DirectPlayFloor()` | 基类默认返回 0（第一关）；Boss 子类可返回 Boss 层索引。 |
| `OnFloorEnded(bPlayerWon)` | 基类空钩子，供 Boss 子类在过关/失败时结算战斗学习等逻辑。 |
| `GetReplayDirector()` | 返回当前关卡的死亡回放导演。 |
| `GetRemainingSeconds()` | 返回剩余时间。 |
| `IsFloorActive()` | 判断关卡是否仍在正常游玩。 |
| `IsPlayerInvulnerable()` | 用世界时间判断是否仍处于临时无敌窗口。 |
| `GrantInvulnerability(Seconds)` | 延长无敌截止时间，不缩短已有更长的保护。 |
| `SetDoubleDrops(Seconds)` | 将双倍掉落截止时间设为当前世界时间加持续秒数。 |
| `IsDoubleDropActive()` | 查询双倍掉落是否尚未结束。 |
| `GetMaze()` | 有有效迷宫时返回只读指针，开放测试场地返回空指针。 |
| `GetCheckpoint()` | 返回出生点位置，没有出生点则返回零向量；普通受伤当前不会据此传送。 |
| `DevClearFloor()` | 开发快捷入口，直接调用过关处理。 |
| `SetRemainingSeconds(Seconds)` | 调试修改倒计时，最低 0。 |
| `HurtPlayer(Source, Damage, Cause, Skill, OnDamageAccepted)` | 伤害总入口：拒绝非活跃、无敌、上帝模式及无效数值；委托 Subsystem 计算 HP。祝福吸收则保护 1 秒且返回 false；真实掉血先执行命中统计回调，非致命保护 0.35 秒；致命则停止正常流程并尝试回放，回放完成后才正式失败。返回值表示是否真正损失 HP。 |
| `EndFloorWithGameOver(Reason)` | 防止重复或销毁中的结算，调用结束钩子、EndRun，再尝试失败 CG，完成/跳过/无法播放后显示失败界面。 |
| `ShowGameOverScreen()` | 世界仍有效时创建失败 UI 并作为模态显示。 |
| `HandleFloorCleared()` | 防止重复过关，停止本关、通知子类、播放音效；最终层还会 EndRun(Victory) 并尝试胜利 CG，随后显示过关/胜利 UI。 |
| `ShowVictoryScreen()` | 世界尚未销毁时创建胜利 UI。 |
| `ShowModal(Widget)` | 将结算界面加到屏幕，切 UI 输入、显示鼠标并暂停世界。 |
| `AttachViewWeapon(Hero, Heroine)` | 转交 ViewModel 的装备函数，把该角色的武器放到第一人称视角。 |

**HP 流程例子：** Boss 子弹传入自己的伤害 → `FloorGameMode::HurtPlayer` 检查保护 → `RunSubsystem::HurtPlayer` 发通知 → `RunState::Hurt` 真正扣血 → 非致命继续；HP 为 0 时先回放 → `EndFloorWithGameOver` 更新存档 → 失败 CG → 失败菜单。命中统计回调放在致命结算之前，因此致命一击能被 Boss 的战斗记录计入。

## 10. MatsuriMenuGameMode.h / MatsuriMenuGameMode.cpp：标题界面入口

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriMenuGameMode.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriMenuGameMode.cpp)。

`AMatsuriMenuGameMode` 管理标题场景，不负责正常战斗。

| 函数 | 作用 |
|---|---|
| `AMatsuriMenuGameMode()` | 以旁观者开始，指定主菜单 Widget。 |
| `BeginPlay()` | 应用游戏画质上限，创建标题角色舞台并切换镜头；创建主菜单、入场淡入遮罩，切到 UI 输入并显示鼠标。缺控制器/Widget 时写错误日志。 |

## 11. MatsuriReplay.h / MatsuriReplay.cpp：真正记录历史状态的死亡回放

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriReplay.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriReplay.cpp)。

`.h` 定义每个历史 Actor 的 ID/变换 `FMatsuriReplayActorFrame`、播放前现场备份 `FMatsuriReplaySavedActor`、一帧玩家/Boss/子弹/其他 Actor 状态 `FMatsuriReplayFrame`；`FMatsuriReplayBuffer` 保留最多 90 帧且不超过 6 秒；`AMatsuriReplayDirector` 负责采样、暂停、回放镜头、代理模型和清理。约每秒 15 次采样，不是录制视频，也不重新模拟伤害。

| 函数 | 作用 |
|---|---|
| `FMatsuriReplayBuffer::Reset()` | 清空历史帧。 |
| `SetMaxFrames(InCapacity)` | 容量至少 1，立即裁去超容量或超过六秒窗口的旧帧。 |
| `Add(Frame)` | 拒绝时间倒退的帧，加入最新帧并裁剪历史窗口。 |
| `Num()` | 返回当前帧数。 |
| `MaxFrames()` | 返回设置的容量。 |
| `Duration()` | 返回末帧时间减首帧时间，不足两帧返回 0。 |
| `At(Index)` | 安全按下标取只读帧，越界返回空。 |
| `Sample(Time)` | 返回时间上最近的历史帧；早于/晚于范围取端点；不会插值。 |
| `AMatsuriReplayDirector()` | 开启正常 Tick、关闭碰撞；暂停中的播放由独立 ticker 推进。 |
| `IsEnabled()` | 读取 `matsuri.Replay` 开关。 |
| `BeginPlay()` | 开始记录并让首次采样尽快发生。 |
| `Tick(DeltaSeconds)` | 仅在开关开启、关卡活跃且没有播放回放时累计采样时间，到间隔就 CaptureFrame。 |
| `CaptureFrame(bForce)` | 保存玩家和 Boss 变换、Boss 子弹数组、妖怪/玩家投射物/伤害来源的 ID 与变换；强制模式允许捕捉致命帧。 |
| `Start(Source, Cause, Skill, OnFinished)` | 校验开关、历史和玩家，强制补最后一帧；备份现场和视角，创建代理、UI、镜头，隐藏玩家、暂停世界，再安装 CoreTicker 推动历史播放。不能开始时返回 false，调用者直接结算。 |
| `BuildProxy()` | 创建当前角色的回放骨骼模型与 Idle 动画，缺资源时用几何体代替；非 Boss 伤害来源另建红色代理。 |
| `SaveVisualActors()` | 保存仍存在的妖怪和投射物现场状态，临时关闭碰撞，并建可供回放的静态网格代理。内部 `SaveActor` lambda 对单个 Actor 执行这一套操作并去重。 |
| `EnsureHistoricalActors()` | 扫描历史中出现但现场已不存在的 Actor，为其补简化代理；内部 `MakeProxy` lambda 负责创建代理、禁碰撞和必要的球体兜底。 |
| `BuildOverlay(Cause)` | 创建回放标题、原因、进度与跳过 UI，设定导演引用，切为游戏加 UI 输入并显示鼠标。 |
| `AdvanceReplay(DeltaSeconds)` | 用独立时间推进播放年龄，取历史帧、更新帧索引/画面/UI；到末尾调用 FinishReplay。 |
| `GetPlaybackProgress()` | 返回 0～1 的播放进度，零时长视为完成。 |
| `ApplyFrame(Frame)` | 把玩家代理、Boss、子弹和其他视觉对象放到历史位置；已知击杀者时从其附近看向玩家，否则从玩家后侧看向玩家。 |
| `ApplyVisualActors(Frame)` | 根据帧内是否出现某 Actor 决定显示、隐藏或移动真对象/代理；击杀来源另由击杀者视觉处理。 |
| `DestroyReplayCamera()` | 安全销毁临时相机并清空指针。 |
| `Complete(bNotify)` | 移除 ticker、清理 UI/代理/镜头；正常完成时恢复暂停状态、视角、位置、碰撞和可见性，并确保完成回调最多发一次；销毁场景时不恢复旧世界或触发结算回调。 |
| `FinishReplay()` | 正常结束或跳过的统一入口，调用 `Complete(true)`。 |
| `EndPlay(EndPlayReason)` | 场景卸载时调用 `Complete(false)`，做清理而不再通知旧关卡。 |
| `IsReplaying()` | 查询是否正在播放。 |
| `IsRecording()` | 查询是否仍处于记录状态。 |
| `GetHistory()` | 返回只读历史缓冲。 |
| `GetPlaybackFrameIndex()` | 返回当前使用的历史帧下标，便于测试/调试。 |
| `GetPlaybackBulletCount()` | 返回当前回放帧的 Boss 子弹数，便于测试/调试。 |

这里的“OW 方法”体现在击杀者视角与死亡前历史重现；角色仅有简化 Idle/代理，历史对象丢失后也可能退化为球体，不能当作逐帧完整动画录像。

## 12. MatsuriStory.h / MatsuriStory.cpp：保留的文字剧情数据和演出开关

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriStory.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriStory.cpp)。

`FMatsuriStoryLine` 保存说话者与台词；`FMatsuriStoryChapter` 保存标题、目标和台词列表。**当前正常入关、失败和胜利走视频 Widget，未调用 `ChapterFor`/`EndingFor` 展示文字卡片。** 文字数据和对应测试仍保留；`IsEnabled` 则仍直接控制正常剧情视频。

| 函数 | 作用 |
|---|---|
| `FMatsuriStoryChapter::IsValid()` | 检查标题、目标非空且至少有一条台词。 |
| `Line(Speaker, Text)`（内部） | 将说话者和台词打包为一条剧情数据。 |
| `HeroName(Hero)`（内部） | 从角色表取姓名。 |
| `HeroVoice(Hero, Sakura, Suzune, Mio)`（内部） | 从三段角色语气版本中选一段文本。 |
| `Chapter(FloorIndex, Hero)`（内部） | 构造对应楼层的完整文字章节，按角色选台词。 |
| `Matsuri::Story::IsEnabled()` | 读取 `matsuri.Story`，现在用来决定是否播放剧情视频。 |
| `ChapterFor(FloorIndex, Hero)` | 将楼层限制到 0～3，再取文字章节；当前主要由文字数据单测调用。 |
| `EndingFor(Hero)` | 构造共同修复结界的文字结尾；当前正常胜利演出使用 MP4。 |

## 13. MatsuriArtTable.h / MatsuriArtTable.cpp：美术资源摆放表

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriArtTable.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriArtTable.cpp)。

`FMatsuriArtPose` 是模型名、相对旋转、缩放和偏移。它让“这个对象用哪个模型、怎么拿/摆”集中管理。

| 函数 | 作用 |
|---|---|
| `Matsuri::ArtTable::Collectible(Kind)` | 返回金币、银币或勾玉的模型姿态。 |
| `AmmoTalisman()` | 返回补弹符模型。 |
| `Chochin()` | 返回提灯妖模型。 |
| `Kasa()` | 返回伞妖模型及缩放，缩小以适配碰撞体和走廊。 |
| `Kappa()` | 返回河童模型及缩放。 |
| `ViewWeapon(Hero)` | 返回各角色第一人称武器的模型、偏移、旋转、缩放，兼顾居中视野和贴墙穿模。 |
| `IsBow(Hero)` | 判断是否为使用弓弦和搭箭表现的 Sakura。 |
| `Shot(Hero)` | 返回箭、苦无、铃弹的飞行模型姿态。 |
| `ZeniCoin()` | 返回猫又投钱技能的重金币模型姿态。 |

## 14. MatsuriArt.h / MatsuriArt.cpp：统一美术加载与缺失兜底

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriArt.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriArt.cpp)。

统一约定 `/Game/Matsuri/Tripo/` 的模型、骨骼、动画、画像、材质贴图路径，以及音效路径；缺资源或关闭 `matsuri.Art` 时返回空，让调用方继续用灰盒。常量 `HeroineScale` 把角色放大为约 1.6 米，两个 UV 矩形用于裁切头像。

| 函数 | 作用 |
|---|---|
| `HeroAssetName(Hero)`（内部） | 角色枚举转资源名前缀。 |
| `FoundArt()`（内部） | 返回已查到资源的弱引用缓存表，避免每次发射/生成对象都读磁盘。 |
| `MissingArt()`（内部） | 返回缺失包名集合，避免反复检查同一个不存在的资源。 |
| `LoadIfPresent<T>(Package)`（内部模板） | 检查开关/缺失缓存，优先取已加载资源；必要时确认包存在、加载指定类型，并缓存结果。 |
| `Matsuri::Art::IsEnabled()` | 查询美术开关。 |
| `Mesh(Name)` | 按静态模型名加载，空名字返回空。 |
| `CharacterMesh(Asset, Clip)` | 加载角色某动作对应的骨骼网格。 |
| `CharacterAnim(Asset, Clip)` | 加载对应动画序列。 |
| `HeroMesh(Hero, Clip)` | 角色枚举转前缀后调用 CharacterMesh。 |
| `HeroAnim(Hero, Clip)` | 角色枚举转前缀后调用 CharacterAnim。 |
| `Sound(Name)` | 加载带 `SFX_` 前缀的音效。 |
| `Texture(Name)` | 加载平铺表面贴图。 |
| `Portrait(Hero)` | 加载对应角色的概念画像。 |
| `Dress(Part, Pose)` | 用美术模型替换灰盒组件，清理染色材质覆盖、应用姿态；找不到模型时保持原状并返回 false。 |
| `Attach(Parent, Pose)` | 动态添加无碰撞、非导航的美术子组件，注册并返回；资源/父对象无效时返回空。 |
| `Preload()` | 提前加载关卡常用静态模型；编辑器中等待其编译完成，减少战斗首次出现资源的卡顿。 |

## 15. MatsuriGraybox.h / MatsuriGraybox.cpp：几何体占位和表面材质

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriGraybox.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriGraybox.cpp)。

`EShape` 定义方块、球、圆柱、圆锥；`ESurface` 定义木材、石块、铺路石；内部 `FSurfaceLook` 存纹理名、平铺尺寸和亮度。许多 Actor 先有能工作的灰盒，再被 Art 替换，因此没有生成模型也能运行。

| 函数 | 作用 |
|---|---|
| `ShapePath(Shape)`（内部） | 形状枚举转引擎基础模型路径。 |
| `LookFor(Surface)`（内部） | 表面类型转贴图名、平铺尺寸和亮度配置。 |
| `Matsuri::Graybox::Mesh(Shape)` | 加载引擎自带基础形状。 |
| `MakePart(Owner, Parent, Name, Shape, Offset, Scale, Rotation)` | 构造阶段创建并挂接静态网格，设置姿态，默认禁碰撞/重叠，返回组件。 |
| `Paint(Part, Color, bGlow, GlowStrength)` | 建动态材质实例并设置颜色，可选择发光；找不到发光材质退回基础形状材质。 |
| `PaintSurface(Part, Surface, Tint)` | 设置世界空间平铺材质与贴图、色调、亮度；缺少资源时退回纯色 Paint。 |

## 16. MatsuriAudio.h / MatsuriAudio.cpp：音效入口

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriAudio.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriAudio.cpp)。

`EMatsuriSfx` 列出脚步、拾取、三种武器、近战、命中、受伤、UI、Boss、钩索等音效。播放函数即使资源缺失也能安全保持静音；请求次数用于测试，并不等于实际听见的次数。

| 函数 | 作用 |
|---|---|
| `Jitters(Sfx)`（内部） | 判断脚步、命中、拾取等重复音效是否需要轻微随机音高。 |
| `WorldFalloff()`（内部） | 懒创建且保持存活的球形距离衰减设置，内圈 300 cm、衰减距离 2600 cm，启用空间化。 |
| `Pitch(Sfx)`（内部） | 对需要变化的音效随机给出约 0.94～1.06 的音高，否则为 1。 |
| `Matsuri::Audio::Name(Sfx)` | 枚举转不带 `SFX_` 的资源名。 |
| `Sound(Sfx)` | 取资源，脚步从四个编号变体中随机选一项。 |
| `Play2D(WorldContext, Sfx, Volume)` | 增加请求计数，播放不依赖位置的声音，适合自身武器、UI和命中确认。 |
| `Spawn2D(WorldContext, Sfx, Volume)` | 类似 Play2D，但返回 AudioComponent，便于弓松手时提前停止蓄力声。 |
| `Preload()` | 提前加载所有普通音效和全部脚步变体，减少首次播放时读盘。 |
| `PlayAt(WorldContext, Sfx, Location, Volume)` | 在世界位置播放带距离衰减的声音，并计数。 |
| `PlayCount(Sfx)` | 返回编辑器进程本次启动以来某音效的请求数，供测试核对事件。 |

## 17. MatsuriQuality.h / MatsuriQuality.cpp：游玩期间的画质上限

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriQuality.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriQuality.cpp)。

这是性能保护：避免编辑器保留的 Cinematic 档被直接拿来游玩。普通分组默认最高 Epic=3，GI 默认最高 High=2；不会把原本更低的档位升高。当前不是三渲二系统。

| 函数 | 作用 |
|---|---|
| `Cap(Level, Max)`（内部） | Max 为负不限制，否则取两者较小值。 |
| `RestoreWhenPlayStops()`（内部，仅编辑器） | 只注册一次 EndPIE 回调；停止 Play 后恢复第一次施加上限前的画质。 |
| `Matsuri::Quality::Capped(Current, MaxGroup, MaxGI)` | 纯计算：复制当前画质，对各分组应用总上限，GI 再受专门上限；不修改引擎。 |
| `ApplyForPlay()` | 读取两个控制台上限，若需降低则备份原值、注册恢复回调、应用新值并日志记录。 |

## 18. MatsuriDev.h / MatsuriDev.cpp：开发快捷操作

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriDev.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriDev.cpp)。

这里是被开发面板和快捷键复用的操作函数，`matsuri.Dev` 默认为关。

| 函数 | 作用 |
|---|---|
| `Find(Name)`（内部） | 查找控制台变量，不存在时发警告并返回空。 |
| `Matsuri::Dev::IsEnabled()` | 读开发模式开关。 |
| `SetEnabled(bEnabled)` | 写开发模式开关。 |
| `GetBool(Name)` | 安全读取布尔控制台变量，不存在按 false。 |
| `SetBool(Name, bValue)` | 找到变量时以代码来源设置布尔值。 |
| `GetString(Name)` | 读取变量的字符串形式，不存在为空串。 |
| `SetString(Name, Value)` | 设置字符串形式的变量值。 |
| `Cycle(Name, Values)` | 在给定候选值间循环，末尾回到开头；数字使用数值比较以兼容 `150` 和 `150.000000`。 |
| `ClearFloor(World)` | 活跃 Boss 层优先直接击败 Boss，否则调用普通关卡过关入口。 |
| `Restock(World)` | 弹药补满、大招充满。 |
| `NextBossPhase(World)` | 找到未败 Boss 并让其进入下一条血，最后一条则击败；没有 Boss 返回 false。 |
| `ToggleGod()` | 反转上帝模式变量并返回新值。 |

## 19. MatsuriDevComponent.h / MatsuriDevComponent.cpp：把 F5～F8 接到开发操作

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriDevComponent.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Core/MatsuriDevComponent.cpp)。

一个文件内有两个类：`UMatsuriDevLineWidget` 显示底部按键提示；`UMatsuriDevComponent` 装在玩家上，创建 Enhanced Input 动作与映射并调用 Dev 工具。

| 函数 | 作用 |
|---|---|
| `UMatsuriDevLineWidget::NativeOnInitialized()` | 用代码构建覆盖层和底部提示文字，不拦截鼠标。 |
| `UMatsuriDevLineWidget::SetText(Text)` | 标签有效时更新文字。 |
| `UMatsuriDevComponent()` | 禁止组件 Tick；该组件只需要事件响应。 |
| `BeginPlay()` | 取得玩家输入子系统，动态创建 F5～F8 动作/映射，绑定处理函数，添加较高优先级输入上下文并显示提示。 |
| `EndPlay(EndPlayReason)` | 删除自己注册的动作绑定、映射和提示 UI，防止跨地图残留。 |
| `GetLastAction()` | 返回最后一次开发操作的说明文字。 |
| `Show(Done)` | 保存操作文字，并拼到按键提示后面。 |
| `HandleClear()` | 调用 Dev::ClearFloor，对应 F5，并更新提示。 |
| `HandleRestock()` | 调用 Dev::Restock，对应 F6，并更新提示。 |
| `HandleBossBar()` | 调用 Dev::NextBossPhase，对应 F7；提示成功或当前没有 Boss。 |
| `HandleGod()` | 调用 Dev::ToggleGod，对应 F8；提示开启/关闭。 |

## 把这些文件串起来读

推荐先读 `Types → Heroes → ScoreState → RunState → RunSubsystem → FloorGameMode`，再按兴趣读回放、资源、音效、开发工具。读函数时先确认它属于“纯规则”还是“操作世界”：例如 `RunState::AdvanceFloor` 只加下标，而 UI 流程才真正换图；`RunState::Hurt` 只扣 HP，而 GameMode 决定保护、回放和失败演出。

本章逐项列出了 Core 中项目手写的具名函数、头文件内联函数与内部辅助函数；UE 宏自动生成的反射/序列化函数不是这些文件里手写的函数，因此没有展开。关键 lambda 已在所属函数说明中交代。文档只解释源码，没有修改游戏运行代码。
