# Boss 模块：玉藻怎样战斗、选技能和学习

这章覆盖 `SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/` 下的 **28 个源码文件（14 组 .h/.cpp）**。`.h` 声明数据、类和可以调用的函数，`.cpp` 实现实际行为。下文按功能分组，列出具名函数；引擎反射宏、常量和普通成员变量按所属类型解释，局部 lambda 回调放在调用它的函数里说明。

先记住这一条调用链：

```text
BossGameMode 建场景并启动战斗
  → Boss 每帧观察玩家，推进战斗状态
  → Brain 选择下一招（模型 → 本地学习器 → 规则）
  → BossSkills 把动作翻译成发弹时间表
  → Danmaku 生成、移动和检测每颗弹
  → DanmakuField 统一绘制、处理碰撞与玩家伤害
  → Brain 结算招式奖励，Memory 保存，Learner 更新
```

## 1. MatsuriBossGameMode：Boss 关卡的总协调者

源码：[MatsuriBossGameMode.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossGameMode.h)、[MatsuriBossGameMode.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossGameMode.cpp)。

`AMatsuriBossGameMode` 继承普通关卡的 `AMatsuriFloorGameMode`，沿用生命值、倒计时、死亡回放和结算流程，把通关条件换成击败玉藻。头文件保存 Boss、弹幕场、血条控件和庭院数据；实现文件负责把这些对象接起来。

| 函数 | 作用 |
|---|---|
| `BuildFloorContent` | 取得固定庭院布局并生成场景，再创建玩家出生点、弹幕场和玉藻；把庭院的掩体资料交给弹幕场。 |
| `SetPlayerDefaults` | 先执行普通关卡的玩家设置，再给角色添加钩绳组件；已有组件则不重复添加。 |
| `BeginPlay` | 执行父类开局流程，创建 Boss 血条，订阅 Boss 被击败事件；`matsuri.Threats` 允许时启动战斗。 |
| `StartBossFight` | 检查 Boss 和弹幕场有效、战斗尚未开始，调用 `Boss->Setup`；重复调用不会再次开局。 |
| `Tick` | 执行父类每帧逻辑，并把 Boss、钩绳状态交给血条界面刷新。 |
| `DirectPlayFloor` | 返回 Boss 层编号，供直接打开该地图的流程使用。 |
| `HandleBossDefeated` | 把“玉藻被击败”转换成父类的通关事件，进入最终胜利流程。 |
| `OnFloorEnded` | 玩家失败时让玉藻记录胜利；无论结果如何都清理弹幕。 |
| `GetBoss` | 读取当前玉藻对象。 |
| `GetField` | 读取当前弹幕场。 |
| `GetArena` | 只读访问庭院布局数据。 |
| `GetBossBar` | 读取 Boss 血条控件，主要也供测试检查。 |
| `HasBossFightStarted` | 查询是否已经执行战斗初始化。 |

## 2. MatsuriBoss：玉藻本人的状态机

源码：[MatsuriBoss.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBoss.h)、[MatsuriBoss.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBoss.cpp)。

`AMatsuriBoss` 是场景里的玉藻 Actor。它持有受击球体、视觉模型、九尾占位模型、光环、Brain 和当前技能程序。`bIntro`、`bShifting`、`bRunningSkill`、`bDefeated` 区分开场、换阶段、施法和战败状态。三条血实际上是同一个总生命值按三等分显示；默认总生命值 150，可用控制台变量调整。

| 函数 | 作用 |
|---|---|
| `AMatsuriBoss`（构造） | 创建碰撞球、视觉根节点、灰盒身体/尾巴/光环和大脑组件；添加妖怪标签，让玩家武器可以把她识别为敌人。 |
| `BeginPlay` | 读取配置生命值，初始化随机流，给灰盒模型涂色，再尝试换成导入的玉藻模型。 |
| `DressInArt` | 加载玉藻骨骼模型和 Idle 动画；成功后隐藏灰盒身体和尾巴，保留用来预警技能的光环。找不到资源就继续使用灰盒。 |
| `Setup` | 接收弹幕场和庭院布局，放到初始悬浮点，记住玩家开战时 HP，初始化大脑并标记战斗开启。 |
| `EndPlay` | 关卡离开且战斗未结算时，仍结算当前招式；中途离开的战斗不强行判胜负。 |
| `MakeView` | 组织技能生成所需的空间数据：Boss/玩家位置、地面高度、庭院大小、区域中心、悬浮点和阶段。 |
| `IsShielded` | 判断玉藻与玩家之间是否被固定实体遮挡。这里的“遮挡”是玩家的掩护状态，不是玉藻获得无敌护盾。 |
| `Observe` | 读取玩家英雄、HP、弹药、所在区域、高低位置、空中状态、距离和移动速度，形成一份原始观察。 |
| `SetTaunt` | 有非空台词时更新台词，并重置显示计时。 |
| `GapSeconds` | 根据阶段返回两招之间的喘息：第 1/2/3 阶段分别为 1、0.7、0.45 秒。 |
| `PickAnchor` | 有合法指定位置则直接使用；否则优先选与玩家距离适合且不同于当前位置的悬浮点，没有合适候选时选最远点。 |
| `StartNextSkill` | 补全观察并让 Brain 决策；开启奖励统计，按技能决定移动或瞬移，初始化技能程序和预警特效，同时提前请求下一招的模型答案。 |
| `BeginPhase` | 更新阶段，进入短暂无敌的换阶段状态，停止当前施法、清屏，并播放音效和大范围特效。 |
| `TakeDamage` | 开场、换阶段、战败或非正数伤害时不扣血；其余情况扣除实际伤害、增加玩家大招充能，并检查换阶段或战败。返回实际扣除的 HP。 |
| `Defeat` | 标记玉藻战败，清弹并结算最后一招，写入玩家获胜的战斗记录；播放特效、生成勾玉、关闭碰撞并广播 `OnDefeated`。守护者玉藻保留在场景中。 |
| `DevNextBar` | 开发快捷键推进到下一条血；已到最后阶段则直接触发战败。 |
| `DevDefeat` | 开发工具把生命值清零并走正常战败流程。 |
| `EndFightAsWinner` | 玩家失败时停止施法、结算最后一招，把本局写成 Boss 获胜，避免重复结算。 |
| `MoveAndPose` | 向悬浮点移动、上下浮动、朝向玩家，摆动灰盒尾巴；处理受击缩放和光环颜色/亮度。 |
| `Tick` | 驱动整个状态机：更新表现与习惯观察，等待开场/换阶段，推进当前技能并提交新弹，间歇结束后开启下一招。模型仍在思考时允许短暂额外宽限，不阻塞游戏线程。 |
| `PhaseThreshold` | 返回进入后续阶段的总血量阈值：约 2/3 和 1/3。 |
| `PhaseFor` | 根据当前总血量比例判断应处于第几阶段。 |
| `GetHealthFraction` | 返回当前 HP / 最大 HP。 |
| `GetHealth` | 返回当前 HP 数值。 |
| `GetPhase` | 返回当前阶段。 |
| `IsDefeated` | 查询是否已战败。 |
| `GetProgram` | 正在施法时返回当前技能程序指针，否则返回空。 |
| `GetTaunt` | 返回当前台词。 |
| `GetTauntAge` | 返回台词已经显示多久。 |
| `GetBrain` | 返回决策组件。 |
| `GetField` | 返回弹幕场。 |
| 匿名命名空间 `HeroId` | 把英雄枚举转换成供记录/提示词使用的英雄与武器描述。 |
| 匿名命名空间 `PhaseLine` | 根据阶段取得换阶段台词。 |

## 3. MatsuriBossSkills：六种技能的参数与发弹程序

源码：[MatsuriBossSkills.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossSkills.h)、[MatsuriBossSkills.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossSkills.cpp)。

头文件的核心数据：

| 类型 | 保存什么 |
|---|---|
| `EMatsuriBossSkill` | 六招编号：环、螺旋、追踪、扫尾、雨、幻影；`Count` 是数量哨兵。 |
| `EMatsuriBossSource` | 这次动作来自规则、学习器还是模型，用于显示和日志。 |
| `FMatsuriBossAction` | 一次决策：技能、4 个 0～1 参数、目标、台词、理由、来源。 |
| `FMatsuriBossView` | 技能当前看到的空间环境。 |
| `FMatsuriSkillProgram` | 一招执行到哪里：时间、持续时长、下一次发弹时间、已发轮次、雨点中心、扫尾方向、幻影位置等。 |

| 函数 | 作用 |
|---|---|
| `Id` | 把技能枚举转成稳定机器名，例如 `onibi_pursuit`，用于 JSON 和提示词。 |
| `FromId` | 不区分大小写地把机器名转回技能枚举，失败返回 `false`。 |
| `DisplayName` | 返回界面显示的日文技能名。 |
| `ParamName` | 查询某技能第几个参数的机器名；同一个参数位置在不同技能中的含义不同。 |
| `ParamMeaning` | 返回参数的实际范围和用途说明，供模型理解。 |
| `Description` | 返回该技能如何攻击、要求玩家怎样躲避的说明。 |
| `AllowedIn` | 检查阶段解锁：阶段 1 环/螺旋/追踪，阶段 2 增加扫尾/雨，阶段 3 增加幻影。 |
| `Clamp` | 检查技能合法性；把参数限制到 0～1、非法数替换为 0.5，修正目标下标并截短台词和理由。 |
| `Preset` | 从六招各三个参数预设中取一个动作，供本地学习器的 18 个选项使用。 |
| `BulletDamageFor` | 根据整招预计总弹量计算每颗弹的 HP 伤害：弹多单发较低，弹少单发较高；处理非法参数，返回非负结果。环的缺口使用比例估算，因此是密度模型，不是逐颗预生成后计数。 |
| `Duration` | 计算一招从开始到最后发弹的时长，包含预警；已经发出的弹还可以继续飞。 |
| `Color` | 返回技能对应的弹幕/特效颜色。 |
| `Begin` | 从动作初始化程序：设置首发预警时间、总时长、扫尾方向、雨区固定中心或幻影生成位置。 |
| `Advance` | 累加技能时间，按技能分支生成该时刻到期的弹，追加进输出数组；返回技能是否还在继续。轮数、速度、追踪、阵型在这里实际生效。 |
| 辅助 `Param` | 把 0～1 参数线性映射到实际浮点范围。 |
| 辅助 `ParamCountOf` | 映射到实际范围后四舍五入成数量。 |
| 辅助 `PhaseSpeed` | 返回阶段弹速倍率：1、1.05、1.15。 |
| 辅助 `Template` | 创建一颗带外观、半径、寿命、技能编号和单发伤害的子弹模板。 |
| 辅助 `SweepTravel` | 估算横穿庭院需要的距离，用于扫尾弹寿命。 |
| 模板辅助 `FireDue` | 循环补发所有到期轮次，防止帧率低时漏掉应发的攻击；具体发弹由传入的 lambda 完成。 |

注意 **“选技能的收益估计”与“子弹伤害”是两条独立逻辑**。前者在 Learner/Brain 里决定用哪招，后者在这里决定命中扣多少 HP。当前单发范围：环 10～14、螺旋 8～12、追踪 18～24、扫尾一墙 36/双墙 28、雨 6～10、幻影 22～32。伤害写进每颗 `FMatsuriBullet`，下一招开始后旧弹不会改成新招的伤害。

## 4. MatsuriDanmaku：子弹数据、运动和几何算法

源码：[MatsuriDanmaku.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriDanmaku.h)、[MatsuriDanmaku.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriDanmaku.cpp)。

`EMatsuriBulletStyle` 区分圆球、米粒和大型弹。`FMatsuriBullet` 是普通结构体，保存前后位置、速度、半径、年龄/寿命、延迟、加速度、短时追踪、独立伤害、技能和“已擦弹/已接触/已死亡”等标志。每颗弹不是独立 Actor，减少大量弹幕的对象开销。

| 函数 | 作用 |
|---|---|
| `Step` | 推进一颗弹：记录上一位置、处理延迟和寿命、限制转向追踪角速度、更新速度与位置。仅计算这帧真正激活的时间。 |
| `Touches` | 判断最新运动段是否碰到静止球体，是 `PlayerImpact` 的便捷包装。 |
| `PlayerImpact` | 用子弹与玩家的相对运动求最早球体接触时刻，输出 0～1 的路径比例；能检测高速弹和玩家相向移动，避免只看帧末位置导致穿透。 |
| `BoxImpact` | 将静态盒子按弹半径扩大，再求运动线段最早进入盒子的时刻，用于掩体碰撞。 |
| `ConeDirection` | 求给定水平角下、能覆盖目标高度的发射方向。 |
| `Ring` | 在输出数组追加一圈子弹，按目标高度倾斜，并按角度跳过缺口内的弹。 |
| `Fan` | 以目标方向为中心追加扇形弹。 |
| `Wall` | 按水平列和垂直行生成一堵向指定方向运动的弹墙。 |
| `Rain` | 在圆盘内均匀随机选出生点，从指定高度向下发弹，附加少量水平漂移。 |

## 5. MatsuriDanmakuField：统一管理场上所有 Boss 子弹

源码：[MatsuriDanmakuField.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriDanmakuField.h)、[MatsuriDanmakuField.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriDanmakuField.cpp)。

`AMatsuriDanmakuField` 持有子弹数组和按“外观 × 技能”分组的实例化网格。`FMatsuriDanmakuStats` 累计发弹、接触、真实命中、擦弹和掩体挡弹数。实现内的 `FPendingImpact` 暂存待处理命中，避免遍历子弹时致死回调清空同一个数组。

| 函数 | 作用 |
|---|---|
| `AMatsuriDanmakuField`（构造） | 创建根组件，设置每帧更新发生在玩家物理移动之后。 |
| `BeginPlay` | 初始化绘制分组和临时变换数组，为子弹预留容量。 |
| `SetArena` | 保存庭院资料，收集固定掩体盒与现场可破坏栅栏，重置玩家运动历史。 |
| `Spawn` | 接收新弹，最多保留容量允许的数量，累加真正接收的发弹数，并清空调用方暂存数组。上限为 1400。 |
| `ClearAll` | 清空所有活弹，间隔生成少量消散特效，然后刷新绘制。 |
| `GetPlayerHitSphere` | 找到玩家，在角色中心上方取得胸口受击球中心；无玩家返回 `false`。 |
| `FindCoverImpact` | 遍历固定掩体和仍站立的栅栏，选出这颗弹最早碰到的掩体及时间比例。 |
| `Tick` | 先统一移动子弹并比较掩体/玩家接触先后；统计擦弹和接触，随后集中调用 `HurtPlayer`。只有接受的伤害才计入 Hits；最后删死弹并更新实例。致死后立即退出，避免回放/清屏重入破坏数组。 |
| `MeshFor` | 按外观和技能查找或懒创建实例化球体组件，配置技能颜色和发光，无逐弹物理碰撞。 |
| `Draw()` | 绘制当前实战子弹数组。 |
| `Draw(VisibleBullets)` | 按分组建立变换：米粒拉长并朝向速度方向；批量增减和更新实例数量与位置。 |
| `ShowReplayBullets` | 绘制录制的弹幕画面，但不改变实战子弹、碰撞和统计。 |
| `ResetPlayerSweep` | 忍者瞬移等情况下清掉上一帧玩家位置，避免把瞬移途中虚构成一条受击路径。 |
| `Num` | 返回活弹数组数量。 |
| `GetBullets` | 只读访问子弹数组，供回放记录等使用。 |
| `GetStats` | 只读访问本局累计统计，供奖励结算。 |

玩家实际受击球半径 22 cm，擦弹范围 90 cm。`Touches` 表示弹到达过受击球，包括无敌期间；`Hits` 表示真正被接受、造成 HP 伤害的命中。无敌时拒绝一次接触，不会永久把这颗弹变成无伤害装饰。

## 6. MatsuriBossBrainComponent：决策、习惯跟踪与学习流程

源码：[MatsuriBossBrainComponent.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossBrainComponent.h)、[MatsuriBossBrainComponent.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossBrainComponent.cpp)。

`UMatsuriBossBrainComponent` 不自己 Tick，由玉藻驱动。它持有记忆 Store、本地 Learner、网络 Client、本局已完成步骤、正在统计的步骤、模型请求序号和预取答案。私有 `FHabitSample` 记录采样时间、遮挡/高处状态、区域、累计钩绳使用和速度；每 0.25 秒采样，保留约 20 秒窗口。

| 函数 | 作用 |
|---|---|
| 构造 `UMatsuriBossBrainComponent` | 关闭独立组件 Tick，避免与 Boss 的驱动重复执行。 |
| `BeginFight` | 建立战斗 ID，加载磁盘记忆；加载学习器失败时从步骤记录重建；读取模型设置并清空本局步骤/习惯。 |
| `Observe` | 累积时间并按间隔记录玩家习惯，删除窗口外旧样本。 |
| `Complete` | 在原始观察上补充掩体占比、高处占比、平均速度、每分钟钩绳次数和最常驻区域。 |
| `ZoneNames` | 从庭院区域表生成名字数组，供模型目标和提示词使用。 |
| `LearnerHints` | 给当前阶段允许且有经验的选项估计收益，排序取前 5 个，生成“预设/收益/尝试次数”提示。 |
| `Prepare` | 模型可用且未思考、非展示模式时，检索相似案例，附上本局轨迹、笔记、区域和统计提示，异步请求下一招。回调用弱对象与请求序号过滤失效/过期答案；合法答案写进 `Ready`。 |
| `FromLearner` | 调用本地学习器选预设，设置雨的常驻区域目标与解释；没有可选项时回退规则。 |
| `FromRules` | 轮流遍历当前阶段允许的技能，取中间参数预设并加上规则理由。 |
| `IsShowcase` | 读取 `matsuri.BossShowcase`。 |
| `Decide` | 展示模式无视阶段轮播六招并取消待用模型结果；普通模式优先使用及时返回且阶段合法的模型动作，否则使用学习器。修正参数，统计来源，并使旧请求序号失效。 |
| `StepStarted` | 若上一步还开着，先结算它；建立新步骤，记下观察、动作、开招时的弹幕统计、Boss 血量比例和战斗时间。 |
| `StepFinished` | 用前后统计差计算本招结果和奖励；加入本局轨迹。非展示模式才追加磁盘步骤、更新本地学习器并保存参数。 |
| `EndFight` | 生成胜负、时长、平均奖励、玩家失血和 Boss 剩余血量摘要，追加战斗记录，启动复盘；展示模式直接跳过。 |
| `Reflect` | 请求模型依据整局轨迹和旧笔记改写最多 10 条经验；回调捕获共享 Store/Client，所以离开关卡后仍可保存复盘结果。失败记录日志，不阻止游戏结算。 |
| `GetProvider` | 返回可用的“供应商:模型名”，否则 `off`；显示配置不等于已经发送请求。 |
| `GetLastAction` | 读取最后采用的动作。 |
| `IsThinking` | 查询是否有模型答案尚在等待。 |
| `GetLastModelSeconds` | 读取最近模型回复耗时。 |
| `GetLastModelError` | 读取最近模型错误。 |
| `GetDecisions` | 查询某来源实际采用的决策次数。 |
| `GetModelRequests` | 查询已发起决策请求数。 |
| `GetThisFight` | 只读读取本局已完成步骤。 |
| `GetStore` | 返回共享记忆存储对象。 |
| `GetLearner` | 只读访问本地学习器。 |
| 辅助 `TauntFor` | 根据技能提供本地回退动作的日文台词。 |

**这里的 ICL 与 ICRL 分别是什么？** ICL 是把历史上相似局面的“观察→动作→结果/奖励”放进这一次提示词；ICRL 是把本局前面已完成招式及奖励继续放进提示词。模型据此改变回答，不在本机更新大模型权重。真正更新数值参数的是下面的 LinUCB。由于 `Prepare` 提前发请求，模型看到的是预取时的观察和已结算轨迹，可能比最终执行时早一招。

**展示模式为何出现 model 0 asked？** `Prepare` 在展示模式直接返回，`Decide` 固定轮播；`StepFinished` 仍能在内存构造轨迹/打印日志，但不写磁盘、不更新学习器，`EndFight` 也不保存战斗或复盘。

## 7. MatsuriBossMemory：观察、奖励、检索与磁盘记录

源码：[MatsuriBossMemory.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossMemory.h)、[MatsuriBossMemory.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossMemory.cpp)。

| 类型 | 保存什么 |
|---|---|
| `FMatsuriBossObservation` | 选择技能时的阶段、双方状态、玩家英雄/HP/弹药/区域、空中/高处/掩体情况，以及最近习惯；旧 lives 字段仅解释历史记录。 |
| `FMatsuriBossOutcome` | 一招期间的 Touches、Hits、Grazes 和 Boss 总血量损失比例。 |
| `FMatsuriBossStep` | 一条“观察、动作、结果、奖励”，附战斗 ID、步骤编号和发生时间。 |
| `FMatsuriBossFight` | 一局摘要：胜负、时长、步数、平均奖励、玩家失血、Boss 剩余血量、日期。 |
| `FMatsuriBossStore` | 管理内存记录和文件目录，并用临界区保护加载/写入。 |

| 函数 | 作用 |
|---|---|
| `FMatsuriBossObservation::Features` | 生成 9 个归一化特征：阶段 2 标志、阶段 3 标志、遮挡、高处、空中、距离/40m、弹药比例、掩体时间比例、高处时间比例。不是观察的所有字段都会进入数值模型。 |
| `BossMemory::Reward` | 计算 `min(Touches,3) - 10×BossDamage + min(0.02×Grazes,0.5)`。BossDamage 是总生命值损失比例；例如丢 2% 血扣 0.2 分。奖励读取 Touches 而非真实扣血 Hits。 |
| `Retrieve` | 按特征距离找近邻，不同阶段额外惩罚；先取最多 3K 个近邻，再从中取奖励最好和最差的两部分，让提示词同时看到成功和失败经验。返回原步骤数组下标。 |
| `ToJson(Step)` | 序列化观察、动作、结果和奖励，保留 HP 或兼容旧 lives 格式。 |
| `FromJson(Json, Step)` | 解析步骤，识别技能和来源，恢复参数/结果，兼容旧命数记录；明显缺少关键对象或未知技能时返回失败。 |
| `ToJson(Fight)` | 把一局摘要转成 JSON。 |
| `FromJson(Json, Fight)` | 解析一局摘要，兼容历史失去命数和现行失去 HP 两种字段。 |
| `Describe(Observation)` | 把当前观察压缩成提示词可读的英文一句话，包含 HP、弹药、位置及习惯。 |
| `Describe(Step, ZoneNames)` | 把一条记录压缩成“情境→技能/参数→结果→奖励”，补充雨区/幻影目标。 |
| `FMatsuriBossStore`（构造） | 保存工作目录路径。 |
| `DefaultFolder` | 取得 `matsuri.BossMemory` 覆盖目录，否则默认 `Saved/BossMemory`。 |
| `LearnerPath` | 返回该目录下 `learner.json` 路径。 |
| `Load` | 加锁清空内存，再逐行读取步骤和战斗 JSONL，读取笔记 JSON；能解析的记录进入内存。 |
| `AppendStep` | 加锁追加内存步骤，并把一行紧凑 JSON 追加到 `steps.jsonl`。 |
| `AppendFight` | 加锁追加内存摘要，并追加到 `fights.jsonl`。 |
| `SetLessons` | 加锁替换笔记，限制最多 10 条，覆盖保存 `notebook.json`；它不是历次笔记的版本历史。 |
| `Wipe` | 删除记忆目录并清空内存；源码注释标作测试用途，不应拿玩家真实目录随便调用。 |
| `GetSteps` | 只读访问步骤数组。 |
| `GetFights` | 只读访问战斗摘要数组。 |
| `GetLessons` | 只读访问当前笔记数组。 |
| `GetFolder` | 读取工作目录。 |
| 辅助 `Condensed` | 把 JSON 对象变成单行字符串，适合 JSONL 追加。 |
| 辅助 `Parse` | 把字符串解析为 JSON 对象，失败返回空。 |
| 辅助 `SourceName` | 把动作来源枚举转换为 `rules/learner/model`。 |
| 辅助 `SourceFrom` | 将来源字符串转回枚举。 |
| 辅助 `Distance` | 计算两组特征的欧氏距离，供历史检索。 |

## 8. MatsuriBossLearner：本地 LinUCB 学习器

源码：[MatsuriBossLearner.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossLearner.h)、[MatsuriBossLearner.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossLearner.cpp)。

`FMatsuriBossLearner` 是普通 C++ 类。六招各三个预设，共 18 个选项（arm）。每个选项保存一个 10×10 矩阵 A、10 维向量 B 和尝试数 Counts；10 维来自前述 9 个特征加常数偏置。它估计“这个局面用这个预设，大约能拿多少奖励”，不是给每招固定一个全局权重。

| 函数 | 作用 |
|---|---|
| 构造 `FMatsuriBossLearner` | 记住探索系数 Alpha（默认 0.6），各 A 初始化为单位矩阵、B 为零。 |
| `ArmFor` | 将技能和预设编号编码成 0～17 的选项下标。 |
| `SkillOf` | 从选项下标解码技能。 |
| `PresetOf` | 从选项下标解码预设编号。 |
| `Context` | 将观察特征加上常数 1，得到 10 维回归输入 X。 |
| `Inverse` | 使用带选主元的 Gauss-Jordan 消元求 10×10 矩阵逆。 |
| `Estimate` | 计算 `θ=A⁻¹B`，返回 `θ·X`，即当前局面的预计收益，不含探索奖励。 |
| `Choose` | 遍历阶段允许的选项，比较 `预计收益 + Alpha×sqrt(XᵀA⁻¹X)`；后项鼓励尝试不确定的选项。同分时按确定顺序优先轮到不同技能。 |
| `NearestArm` | 模型可自由给 4 个连续参数；本函数找同一技能里距离最近的预设，把经验归入本地 18 个选项之一。 |
| `Update` | 用一次观察和奖励更新该选项：`A+=XXᵀ`、`B+=reward×X`，次数加一；跳过非法选项或非有限奖励。 |
| `Rebuild` | 清空学习器，再重放全部历史步骤，恢复本地学习参数。 |
| `Pulls` | 返回某选项累计学习次数。 |
| `Save` | 把 Alpha 和全部选项的矩阵、向量、次数保存为 JSON，返回保存结果。 |
| `Load` | 读取 JSON，校验选项数量与矩阵/向量尺寸，成功后替换当前学习器。 |

## 9. MatsuriBossPrompt：模型输入与输出协议

源码：[MatsuriBossPrompt.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossPrompt.h)、[MatsuriBossPrompt.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossPrompt.cpp)。

`FMatsuriBossPromptInput` 收集当前观察、检索案例、本局轨迹、笔记、区域信息和学习器提示。`FMatsuriBossPrompt` 只有 System 与 User 两段文本。本文件只负责构造/解析内容，不发 HTTP。

| 函数 | 作用 |
|---|---|
| `Decision` | 生成决策提示词：系统段说明玉藻身份、奖励、场地、招式和 JSON 格式；用户段放笔记、最多 8 个历史例子、最近 10 个本局步骤、统计估计与当前观察。 |
| `ParseDecision` | 从回复中读出技能、命名或数组参数、目标区域名/下标、台词和理由；调用 `Clamp` 校验当前阶段，成功输出动作并标记来源 Model。 |
| `Reflection` | 把胜负摘要、本局所有步骤和旧笔记打包，请模型重写最多 10 条有证据的短经验。 |
| `ParseLessons` | 提取 `lessons` 数组，去掉空项、去首尾空格，每条限 140 字符、总量限 10；无有效经验返回失败。 |
| `ExtractJson` | 从夹杂解释或代码围栏的回复中找到第一个完整 `{...}`，识别字符串和转义，避免把字符串内花括号算作结构。 |
| 辅助 `CatalogFor` | 生成全部技能及参数说明，标记当前阶段不可用的技能。 |
| 辅助 `ZonesText` | 把区域名、下标、高处标志写成提示词。 |
| 辅助 `ParseObject` | 先提取 JSON，再反序列化为对象；解析失败返回空。 |

## 10. MatsuriBossModelClient：异步连接 DeepSeek / Ollama

源码：[MatsuriBossModelClient.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossModelClient.h)、[MatsuriBossModelClient.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossModelClient.cpp)。

`FMatsuriModelSettings` 保存供应商、URL、模型、密钥、超时和思考设置；`FMatsuriModelReply` 保存是否成功、内容、错误、耗时和 token 数；`FMatsuriBossModelClient` 保存配置并封装 OpenAI 兼容的 Chat 请求。密钥从环境读取，本文件不会主动写入磁盘或输出日志。

| 函数 | 作用 |
|---|---|
| `FMatsuriModelSettings::FromConsole` | 从 `matsuri.BossLLM/BossModel/BossLLMTimeout` 读取设置；DeepSeek 密钥取自 `DEEPSEEK_API_KEY`，Ollama 指向本地端口。 |
| `FMatsuriModelSettings::IsUsable` | DeepSeek 需要非空密钥；Ollama 配置被视为可尝试；off 不可用。这里不检查远端服务是否真的在线。 |
| `FMatsuriBossModelClient`（构造） | 保存传入配置。 |
| `GetSettings` | 只读访问客户端配置。 |
| `BuildBody` | 生成包含 system/user、模型名、token 限额、非流式、JSON 输出要求的请求体；DeepSeek 决策默认关闭思考，复盘可开启。局部 `AddMessage` lambda 负责追加消息对象。 |
| `ParseResponse` | 解析 API 返回的错误、第一条回答及 usage；空内容、无 choices 或非 JSON 会返回错误。 |
| `Chat` | 配置可用时发异步 POST，设置授权与超时；完成回调处理连接失败、HTTP 错误或有效内容，记录耗时并调用上层回调。战斗决策使用配置超时，复盘至少给 120 秒。 |

它不负责决定是否采用回答：过期回复过滤、阶段合法性和回退由 Brain 处理。

## 11. MatsuriBossArena：固定庭院的布局与生成

源码：[MatsuriBossArena.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossArena.h)、[MatsuriBossArena.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossArena.cpp)。

| 类型 | 作用 |
|---|---|
| `EMatsuriArenaPiece` | 区分外围墙、平台、固定掩体和可破坏栅栏。 |
| `FMatsuriArenaBox` | 用中心、半尺寸、类型、名字表示一个实体盒。 |
| `FMatsuriArenaZone` | 玩家可驻留/雨可瞄准的命名区域，保存中心、半径和是否高处。 |
| `FMatsuriBossArena` | 整张庭院数据：边界、实体、区域、钩点、弹药、Boss 悬浮点、玩家出生位姿。 |

| 函数 | 作用 |
|---|---|
| `FMatsuriArenaBox::Bounds` | 将中心与半尺寸转成最小/最大点盒子。 |
| `FMatsuriArenaBox::Top` | 返回盒子顶面高度。 |
| `BossArena::Layout` | 构建固定约 60×60 米庭院资料，包括本殿、平台、塔、鸟居、石灯、雕像、栅栏及出生点。 |
| `ZoneAt` | 在水平范围和高度容差内，选择离玩家最近的区域；开放区域返回 -1。 |
| `IsShielded` | 检查 Boss 到玩家的线段是否穿过非栅栏实体；栅栏可破坏，因此不作为这份稳定观察的遮挡来源。 |
| `Build` | 实际生成环境灯光、地板、实体、坡道、钩点、可重生弹药和局部灯光。 |
| 辅助 `Box` | 方便创建带名字和类型的实体盒数据。 |
| 辅助 `Zone` | 方便创建命名区域数据。 |
| 辅助 `SpawnRamp` | 用倾斜盒网格创建平台到地面的可走坡道，设置石材和碰撞。 |
| 辅助 `SpawnSolid` | 栅栏生成可破坏 Actor，其余实体生成静态网格 Actor，设置碰撞和木/石材质。 |
| 辅助 `SpawnLight` | 生成暖色、无阴影的局部点光源。 |

## 12. MatsuriArenaPieces：可破坏栅栏与钩绳目标

源码：[MatsuriArenaPieces.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriArenaPieces.h)、[MatsuriArenaPieces.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriArenaPieces.cpp)。

本文件包含两个 Actor：`AMatsuriBreakableCover` 管木栅栏；`AMatsuriHookPoint` 管发光钩环。

| 函数 | 作用 |
|---|---|
| `AMatsuriBreakableCover`（构造） | 创建有实体碰撞的木板盒网格。 |
| `SetExtent` | 记录半尺寸，并把标准盒子缩放到对应大小；设计上生成完成前调用。 |
| `BeginPlay`（栅栏） | 应用木材质和颜色。 |
| `GetBounds` | 返回栅栏世界空间盒子，供弹幕几何碰撞使用。 |
| `AbsorbBullet` | 栅栏站立时累加挡弹次数，达到 25 次后隐藏并关闭碰撞，播放破碎表现；弱引用定时回调 20 秒后重置次数并长回。 |
| `SetStanding` | 同时切换站立标记、可见性和碰撞。 |
| `IsStanding` | 查询当前是否还能挡弹。 |
| `GetHitsTaken` | 返回累计挡弹次数。 |
| `AMatsuriHookPoint`（构造） | 创建钩环、中心珠和灯光，添加钩点标签，不启用物理碰撞。 |
| `BeginPlay`（钩点） | 设置发光材质，缓存可调亮度的动态材质。 |
| `Tick`（钩点） | 让钩环旋转和脉动；被准星选中时提高材质与灯光亮度。 |
| `SetHighlighted` | 设置是否为当前可用钩绳目标。 |

## 13. MatsuriGrappleComponent：F 钩绳移动

源码：[MatsuriGrappleComponent.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriGrappleComponent.h)、[MatsuriGrappleComponent.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriGrappleComponent.cpp)。

`UMatsuriGrappleComponent` 动态添加到 Boss 关卡角色身上，保存输入映射、视觉绳子、弱引用目标、目的地、冷却和使用次数。它用角色移动组件拉动玩家，绳子是装饰圆柱，不是物理绳索模拟。

| 函数 | 作用 |
|---|---|
| 构造 `UMatsuriGrappleComponent` | 开启组件每帧更新。 |
| `BeginPlay` | 绑定输入，创建无碰撞发光绳子网格，初始隐藏。 |
| `EndPlay` | 解除本组件输入绑定和映射，再走父类销毁流程。 |
| `BindInput` | 找本地 Enhanced Input，创建动作/映射，把 F 和手柄左肩键绑定到钩绳。无本地输入时跳过，测试可直接调用函数。 |
| `UnbindInput` | 按保存的 handle 删除绑定，并移除本组件映射上下文。 |
| `HandleGrappleInput` | 输入入口，直接调用 `TryGrapple`。 |
| `FindTarget` | 在 35 米、准星约 12 度范围内找钩环，射线确认中间无遮挡，选最靠准星的目标。 |
| `TryGrapple` | 检查角色、目标、冷却和是否已在拉动；成功则目的地设在钩环下方，切飞行移动模式、计使用次数、播音并显示绳子。 |
| `Release` | 结束拉动并进入冷却，切回下落；提前跳脱时保留速度并上抬，正常抵达时给少量前向/向上速度。 |
| `UpdateRope` | 计算视角右下方“手”到钩环的中点、方向和长度，更新圆柱绳子；未拉动时隐藏。 |
| `TickComponent` | 未拉动时更新目标高亮；拉动时检查跳脱、抵达和超时，否则持续给角色朝目的地的拉动速度并更新绳子。 |
| `IsPulling` | 查询是否正在被拉动。 |
| `GetTarget` | 返回当前有效钩点。 |
| `GetUses` | 返回累计使用次数，供 Brain 统计玩家钩绳习惯。 |

## 14. MatsuriBossBarWidget：三条血、技能伤害与 AI 调试信息

源码：[MatsuriBossBarWidget.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossBarWidget.h)、[MatsuriBossBarWidget.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Boss/MatsuriBossBarWidget.cpp)。

`UMatsuriBossBarWidget` 是代码搭建的 UMG 控件，保存三条 ProgressBar、名字、技能、台词、钩绳和调试文字。由 GameMode 调用 `Refresh`，不依赖控件自身 Tick，因此无渲染测试也能检查文字。

| 函数 | 作用 |
|---|---|
| `NativeOnInitialized` | 创建控件树和排版：顶部名字/三条血/技能/台词、准星下方钩绳提示、左下 AI 调试文字。 |
| `Refresh` | 把总血量分配到三条显示条；显示当前技能与单发伤害、台词淡出、可钩提示。开启 `matsuri.BossDebug` 时显示模型配置、最后动作来源、请求/采用次数、耗时、错误和理由。 |
| `GetSkillText` | 返回当前技能文字，控件未创建时返回空文本。 |
| `GetDebugText` | 返回当前调试文字，供测试读取。 |
| `GetBarFill` | 返回指定血条填充值，非法下标返回 0。 |
| 辅助 `SourceLabel` | 把决策来源转换成 `model/learner/rules` 标签。 |

## 读代码时最容易混淆的四点

1. **Brain 决定“用什么招”，BossSkills 决定“这招怎样发弹”，DanmakuField 决定“这一颗有没有真正扣血”。** 调伤害优先找 `BulletDamageFor`；调策略优先找 Brain/Learner/Prompt。
2. **Touches、Hits、HP 伤害不同。** 当前学习奖励主要奖励接触压力，且每招最多记 3 次接触的正分；它没有按单发 HP 伤害直接给分。高奖励不自动等于更高胜率。
3. **模型预取与本地学习并存。** 有密钥或 HUD 写着模型名不代表本招来自模型；要看请求数、实际采用来源和日志。模型缺失/超时/输出非法时继续战斗。
4. **本章解释源码行为，不是新的运行验证。** 这里只读代码并编写文档，没有启动游戏、发送模型请求、修改数值或重建项目。
