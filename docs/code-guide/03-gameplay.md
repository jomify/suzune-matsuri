# 玩法代码：攻击、技能、妖怪、拾取物与关卡

本篇按当前源码解释 `Combat/`、`Enemies/`、`Pickups/`、`Level/` 共 40 个 `.h/.cpp` 文件。每对头文件和实现文件一起读：`.h` 定义数据、接口和保存的状态；`.cpp` 实现行为。表中的短函数名属于所在小节的类或命名空间；同名函数分属不同类时分别说明。UE 生成的反射函数、引擎继承而未重写的函数不逐个列出。

先记住三条调用路线：

1. 鼠标输入 → `UMatsuriCombatComponent` → 生成 `AMatsuriProjectile` → 碰撞/重叠 → `TakeDamage` → 妖怪死亡、掉落 → 拾取物把分数或弹药交给运行子系统。
2. Shift/E/Q → `UMatsuriSkillComponent::TryUseSkill` → 角色专属技能 → 成功才进入冷却/消耗大招；Q 成功后可播放角色专属短片。
3. 楼层参数 `Floors` → 纯数据布局 `FloorLayout` / `Maze` → `Builder` 生成碰撞和 Actor → `Decor` 添加模型、灯笼和装饰。

## 一、Combat：角色如何攻击

### MatsuriCombat.h / MatsuriCombat.cpp：公共战斗入口

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriCombat.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriCombat.cpp)。

这里没有角色类，而是公共函数。`ProjectileChannel` 对应配置中的投射物碰撞通道；`YokaiTag` 是可以被玩家攻击识别的妖怪标签。

| 函数 | 作用 |
|---|---|
| `Matsuri::Combat::IsPlayerPawn` | 把 Actor 尝试当作 Pawn，确认它由玩家控制，防止怪物、装饰或其他物体触发玩家专属行为。 |
| `Matsuri::Combat::HurtPlayer` | 从世界查找楼层 GameMode，把受伤来源交给它统一处理 HP、无敌和死亡；不在楼层 GameMode 中则记警告并忽略。头文件中关于旧生命/复活的注释不能代替当前实现。 |

### MatsuriCombatComponent.h / MatsuriCombatComponent.cpp：武器、蓄力和近战

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriCombatComponent.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriCombatComponent.cpp)。

`UMatsuriCombatComponent` 挂在玩家身上，保存上次开火/近战时间、短暂的点击缓冲、弓蓄力和忍者大招状态。武器数值读取运行状态中的英雄定义；`OnShot`、`OnSwing`、`OnAttack` 等委托负责通知武器动画和 HUD，并非另一套攻击判定。

| 函数 | 作用 |
|---|---|
| 内部 `LookFor` | 根据英雄选择箭、手里剑或光球式铃铛外观。 |
| `UMatsuriCombatComponent` | 开启组件逐帧更新，供点击缓冲使用。 |
| `BeginPlay` | 组件开始运行时绑定输入。 |
| `EndPlay` | 离开关卡/销毁时解绑输入，避免残留绑定。 |
| `BindInput` | 创建 Enhanced Input Action 和映射，连接鼠标左键/手柄扳机、V/肩键与处理函数。 |
| `UnbindInput` | 按保存的句柄删除事件，并移除本组件的输入映射。 |
| `TickComponent` | 清除过期点击；冷却刚结束且点击仍在缓冲期内时，补一次开火尝试。 |
| `HandleFirePressed` | 被大招动画锁住就忽略；弓开始蓄力、降低移速并播放拉弓声，其他武器记录冷却期间的早按输入。 |
| `HandleFireHeld` | 非蓄力武器持续调用 `TryFire`，真正射速由冷却判断控制。 |
| `HandleFireReleased` | 松弓时取当前蓄力比例，恢复移速/停止拉弓声；未被锁定才发射。 |
| `HandleMeleeInput`（inline） | 将近战按键转发到 `TryMelee`。 |
| `StartFiring` / `StopFiring`（inline） | 对外提供“按下/松开开火”的调用入口，分别转发输入处理函数。 |
| `IsCharging`（inline） | 读取是否正在蓄力。 |
| `SetUltimateShowcaseLock` | 锁住/释放战斗；清理缓冲，并在锁定时取消蓄力、恢复移速和收掉拉弓声，防止动画结束后误射。 |
| `SlowForDraw` | 开始蓄力时保存原走速并乘减速系数；结束时恢复保存值。 |
| `GetChargeAlpha` | 按蓄力经过时间/英雄最大蓄力时间返回 0～1。 |
| `ActivateShadowStitch` | 记录忍者“影缝”效果的结束时间。 |
| `IsShadowStitchActive` | 比较世界时间与结束时间，判断免费追踪射击是否生效。 |
| `IsReady` | 将基础攻击间隔乘护身符攻速修正，再判断距离上次攻击是否足够久。 |
| `ScaledDamage` | 将基础伤害乘当前局伤害倍率。 |
| `GetView` | 从拥有者控制器取得眼睛位置和朝向；没有控制器则返回失败。 |
| `GetMuzzle` | 优先取第一人称武器模型的真实发射点；没有模型时使用视角前方的备用偏移。 |
| `GetAimPoint` | 从准星沿视线做射线检测，返回击中点或最远目标点。 |
| `FindHomingTarget` | 在前方锥形范围内寻找最近的 `YokaiTag` Actor，返回其根组件。 |
| `SpawnShot` | 延迟生成投射物，先写入射击参数，再完成生成，保证出生时已配置正确。 |
| `TryFire` | 检查动画锁、控制器、冷却和弹药；计算蓄力速度/伤害、穿透/弹跳、追踪和扇形方向，生成一组子弹，播放角色音效并广播射击/攻击事件。返回是否真正开火。 |
| `ThrowZeni` | 生成猫又技能使用的高伤金币弹，使用金币专属外观/速度并广播攻击事件；分数消耗由技能组件负责。 |
| `InSwing` | 判断目标是否在近战距离、垂直范围和扇形角度内；极近目标放宽角度要求。 |
| `TryMelee` | 检查锁与冷却；先收集范围内妖怪再逐个伤害、击退/眩晕，并销毁范围内敌对普通投射物；生成挥击特效和反馈。它不是 Boss 弹幕的统一反弹入口。 |
| `ConfirmHit` / `ConfirmKill`（inline） | 分别广播命中/击杀事件，让界面即时给出确认反馈。 |

### MatsuriProjectile.h / MatsuriProjectile.cpp：一发投射物的一生

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriProjectile.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriProjectile.cpp)。

`FMatsuriShot` 保存本发子弹的方向、速度、伤害、穿透次数、弹跳次数、敌我属性和追踪目标；`FMatsuriBallistics` 保存重力、自转、拖尾、碰墙行为等物理/表现参数。`EMatsuriShotLook` 区分箭、苦无、铃铛、金币、黄瓜；`EMatsuriShotImpact` 区分消失、插墙、反弹；`EMatsuriSpinAxis` 区分自转方式。`AMatsuriProjectile` 是真实 Actor，使用 UE 投射物运动组件；Boss 大量光球使用另一套 `Boss/MatsuriDanmaku*` 数据模拟。

| 函数 | 作用 |
|---|---|
| `Matsuri::Shots::BallisticsFor` | 根据弹种返回重力、自转、拖尾和碰墙设置：箭/苦无插入墙面，铃铛/金币反弹，黄瓜抛射后消失。 |
| `Matsuri::Shots::LobDirection` | 根据起点、目标、速度和重力求较低的抛物线发射方向；理论不可达时用约 45° 的备用发射角。 |
| `AMatsuriProjectile` | 建立碰撞球、模型、自转父节点、拖尾、点光和运动组件，设置寿命及世界坐标发射速度。 |
| `Configure` | 保存射击参数，应用弹道/追踪，设置玩家弹的更宽命中球、穿越妖怪重叠和忽略玩家碰撞，然后设置外观。 |
| `ApplyLook` | 优先使用导入模型；缺模型时按弹种缩放基本形状；设置光色和箭尖相对碰撞点的位置。 |
| `BeginPlay` | 上材质、保存出生点、绑定命中/重叠/停止事件、忽略发射者，播放枪口特效，并补处理出生瞬间已发生的重叠。 |
| `Tick` | 更新弹体自转、闪光微粒，以及随飞行距离增长的拖尾。实际位移由运动组件负责。 |
| `IsHostileToPlayer`（inline） | 返回敌我属性，近战用它筛选可拍掉的敌弹。 |
| `Deflect` | 生成被拍散的火花并销毁子弹；此处“deflect”是消除，并不重新瞄准反射。 |
| `HandleHit` | 处理阻挡碰撞：敌弹命中玩家时转交统一伤害入口，玩家弹命中妖怪则伤害；碰墙按规则反弹、留插墙模型或消失。 |
| `HandleStop` | 投射物运动停止时销毁它，避免铃铛滚停后永久留场。 |
| `StickInto` | 按撞击点和飞行方向生成一个不带碰撞的静态模型，模拟箭/苦无插墙，几秒后清理。 |
| `SpawnGlint` | 生成飞行过程中的小闪光微粒。 |
| `HandleOverlap` | 玩家弹首次重叠妖怪时命中；随后优先寻找弹跳目标，其次消耗穿透次数，否则销毁。已命中的对象不重复伤害。 |
| `StrikeYokai` | 记录命中过的对象，调用 UE 伤害接口，生成数字/声音/特效，存活妖怪轻微击退，并通知武器命中确认。 |
| `FindBounceTarget` | 在弹跳半径内寻找最近且尚未命中的妖怪标签 Actor。 |

### MatsuriSkillComponent.h / MatsuriSkillComponent.cpp：三位英雄的九个技能

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriSkillComponent.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriSkillComponent.cpp)。

`UMatsuriSkillComponent` 保存三个槽的可用时间、拾取吸引半径/速度/结束时间和短片锁。角色/技能的名字与冷却数值来自 `Core/MatsuriHeroes`；这里实现实际效果。

| 函数 | 作用 |
|---|---|
| 内部 `Feet` | 用 Actor 位置减固定高度，得到放置地面特效的近似脚底点。 |
| 内部模板 `ForEachWithin` | 遍历某类型 Actor，按二维距离筛选后执行传入操作；用于范围控制、伤害和吸引拾取物。 |
| `UMatsuriSkillComponent` | 开启逐帧更新，支持持续吸引拾取物。 |
| `BeginPlay` / `EndPlay` | 开始时绑定输入；结束时解除技能和战斗锁并解绑，避免关卡切换遗留锁定。 |
| `BindInput` / `UnbindInput` | 创建并绑定 Shift/E/Q 和对应手柄键；结束时移除映射及保存的绑定句柄。 |
| `HandleShift` / `HandleE` / `HandleQ`（inline） | 分别把按键转发为对应槽位的 `TryUseSkill`。 |
| `SetUltimateShowcaseLock`（inline） | 设置技能输入是否被大招短片锁住。 |
| `GetCooldownRemaining` | 返回对应槽距离可用时刻还有几秒，最小为 0；Q 是否可用需另查大招充能。 |
| `TryUseSkill` | 要求关卡运行中且未锁定；普通技能检查冷却，Q 检查充能；效果执行成功才设置冷却/消耗充能。Q 随后尝试播最多 2.5 秒专属动画，暂停由电影控件处理；结束/跳过/失败回调解除战斗与技能锁，缺文件也会立即解锁。 |
| `Perform` | 根据英雄和槽位分派到九个具体技能函数。 |
| `GetView` | 读取玩家眼睛位置和朝向，供定向净化和瞬移使用。 |
| `SakuraBarrier` | 给玩家 2 秒无敌，生成结界特效，击退并短暂眩晕附近妖怪。 |
| `SakuraPurifyCone` | 向视线前方扇区抛出纸符特效，并眩晕范围内妖怪。 |
| `SakuraGreatPurge` | 收集附近妖怪，逐个施加大额伤害并生成净化光柱；先收集再伤害，避免遍历时销毁对象影响迭代。 |
| `SuzuneBlink` | 沿水平视线做角色胶囊扫掠，瞬移到墙前或最大距离；留下残影/轨迹，并在 Boss 战重置玩家扫掠起点，防止把瞬移路径当成真实穿弹移动。 |
| `SuzuneDecoy` | 生成烟雾与短命分身 Actor，返回生成是否成功。 |
| `SuzuneShadowStitch` | 让战斗组件开启 6 秒免费追踪射击，并生成紫色环。 |
| `MioBeckoningCat` | 开启较长时间、较小范围的金币/弹药吸引，并显示金色环。 |
| `MioZeni` | 尝试扣除规定分数后发射金币弹；不足分数返回失败，不进入冷却。 |
| `MioFullHouse` | 开启更大更快的短暂吸引，同时给楼层设置 8 秒双倍掉落。 |
| `StartMagnet` | 写入吸引半径、移动速度和结束时间。 |
| `IsMagnetActive` | 判断吸引效果是否尚未结束。 |
| `TickComponent` | 吸引效果生效时调用 `TickMagnet`。 |
| `TickMagnet` | 将范围内金币和当前可用的弹药符向玩家移动，到达时由拾取物自己的重叠逻辑完成领取。 |

注意：当前 Q 的顺序是“先应用真实技能效果 → 消耗充能 → 播动画”，并非等影片结束后再结算技能。

### MatsuriDecoy.h / MatsuriDecoy.cpp：忍者分身

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriDecoy.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriDecoy.cpp)。

`AMatsuriDecoy` 是维持 4 秒的替身；吸引仇恨由妖怪基类的 `FindTarget` 实现，分身自己不运行 AI。

| 函数 | 作用 |
|---|---|
| `AMatsuriDecoy` | 用基本形状建立躯干和头，设置自动销毁寿命，不开启 Tick。 |
| `BeginPlay` | 将分身涂成发光蓝色，表示幻影。 |

### MatsuriDamageNumber.h / MatsuriDamageNumber.cpp：飘字反馈

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriDamageNumber.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriDamageNumber.cpp)。

`AMatsuriDamageNumber` 只有文本组件、年龄、基础大小和随机漂移；它显示伤害，不计算真实伤害。

| 函数 | 作用 |
|---|---|
| `AMatsuriDamageNumber` | 创建居中的无碰撞文本，开启逐帧动画。 |
| 静态 `Pop` | 在命中位置上方生成数字，选择整数/小数格式、普通/强击/击杀颜色与大小、随机漂移。 |
| `Tick` | 数字上浮、先放大后缩小、朝向玩家摄像机，并在寿命结束时消失。 |

### MatsuriViewModel.h / MatsuriViewModel.cpp：第一人称手持武器动画

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriViewModel.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriViewModel.cpp)。

`UMatsuriViewModelComponent` 跟随摄像机，保存武器、箭、弦段、箭尖灯以及摇摆/后坐/挥击动画状态。移动的是武器而非整个摄像机。箭的发射位置与屏幕中箭尖一致。

| 函数 | 作用 |
|---|---|
| `UMatsuriViewModelComponent` | 开启较晚 Tick，更新手持武器姿势。 |
| 静态 `Equip` | 有现成组件就复用；将相机固定到胶囊眼高，按英雄装上武器；弓额外创建箭、两段弦和箭尖灯；隐藏模板人偶手臂。缺相机或武器资源返回空。 |
| `BeginPlay` | 订阅武器开火和挥击事件，触发后坐/挥击。 |
| `EndPlay` | 解除订阅。 |
| `GetMuzzleLocation` | 弓返回已搭箭箭尖；其他武器把本地发射点变换到世界空间。 |
| `IsBow`（inline） | 返回是否使用弓专属表现。 |
| `GetDrawLocation`（inline） | 返回当前弦中心/箭尾的世界位置。 |
| `IsArrowNocked` | 有箭模型、有弹药且距上次射击超过射击间隔才显示搭箭。 |
| `GetRestOffset`（inline） | 返回相对于摄像机的静止姿势偏移。 |
| `GetWeapon`（inline） | 返回手持武器网格组件。 |
| `PoseBow` | 根据蓄力比例拉回弦和箭，重算两段弦的方向/长度，更新箭尖金光、轻微视角缩放和暗角。 |
| `GetDrawZoom` | 返回相对于默认 FOV 缩小了多少度，供反馈和测试使用。 |
| `Kick` | 记录开火时刻并把后坐强度重置到最大。 |
| `Swing` | 将挥击动画计时从头开始。 |
| `TickComponent` | 每帧调用 `ApplyPose`。 |
| `ApplyPose` | 综合转向滞后、步行摆动、脚步音、后坐恢复、近战弧线、蓄力后拉和满蓄力颤动，写入武器最终位置/旋转，并更新弓。 |

`Equip` 内的 `MakePiece` lambda 负责建立仅拥有者可见的箭/弦组件；`PoseBow` 内的 `Span` lambda 把一段圆柱形弦拉伸到两个端点之间。

### MatsuriFx.h / MatsuriFx.cpp：可复用的小特效

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriFx.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Combat/MatsuriFx.cpp)。

`FMatsuriFxSpec` 是单个特效的配方：形状、颜色、发光、开始/结束大小、速度、重力、自转、寿命、点光。`AMatsuriFx` 执行配方；`Matsuri::Fx` 提供组合特效工厂；`ETemplate` 指定引擎自带三种 Niagara 模板。所有这些是表现，没有碰撞和伤害。

| 函数 | 作用 |
|---|---|
| 内部 `Pool` | 返回按世界区分的特效回收池，用弱引用保存空闲 Actor。 |
| 内部 `TakeRetired` | 从对应世界取出一个仍有效且已退休的特效，没有则返回空。 |
| 内部 `Release` | 池未满就退休并保存特效，达到上限则销毁它。 |
| `AMatsuriFx` | 创建无碰撞形状和点光，禁止受伤。 |
| `Configure` | 保存配方、限制最短寿命，设置形状/尺寸/旋转/灯光。 |
| `BeginPlay` | 首次生成时创建发光材质并保存动态材质引用。 |
| `Reuse` | 重置年龄和配方，移动到新位置并恢复显示/Tick。 |
| `Retire` | 标记不活跃、解除附着、隐藏形状/灯光并停止 Tick。 |
| `IsActive`（inline） | 读取是否正用于某个效果。 |
| `Tick` | 按寿命插值尺寸、渐弱发光/点光，更新速度/重力/自转，到期归还对象池。 |
| `Matsuri::Fx::Spawn` | 优先复用回收对象，否则生成新对象；可让特效附着到角色。 |
| `Burst` | 从一点抛出一组发光微粒，可偏向指定方向。 |
| `Ring` | 用多个微粒组成平面环，可向外扩张、旋转和跟随目标。 |
| `Dome` | 生成结界/光罩式的球形效果。 |
| `Pillar` | 生成向上伸展并收细的光柱。 |
| `AfterImage` | 用躯干和头的形状组成渐消残影。 |
| `Streak` | 在两点之间生成细长光迹。 |
| `Smoke` | 生成一组灰色烟雾形状。 |
| `SwingArc` | 在视线前方生成近战月牙弧线。 |
| `StunHalo` | 在目标头顶生成旋转的眩晕星环。 |
| `MuzzleFlash` | 在发射点生成短促亮点和火舌。 |
| `Impact` | 生成撞击闪光、碎片和 Niagara；墙面可额外保留短暂焦痕，妖怪命中使用不同规模。 |
| 内部 `TemplatePath` | 把 `ETemplate` 映射到引擎 Niagara 资源路径。 |
| 内部 `LoadTemplate` | 加载并缓存对应模板，缓存为弱引用。 |
| `Prewarm` | 在加载时调用生成和回收，尝试预热该世界的对象池；它的循环实际会复用空闲对象，不应把请求次数理解为保证生成同样数量的独立 Actor。 |
| `PreloadTemplates` | 提前加载 Niagara 模板，在编辑器中等待相关编译，减少首次战斗时卡顿。 |
| `Template` | 开关允许时按指定位置/方向/缩放生成 Niagara，并增加计数。 |
| `TemplateCount` | 返回本进程生成 Niagara 模板次数，供测试检查。 |

## 二、Enemies：妖怪怎样行动、受伤和掉落

### MatsuriYokai.h / MatsuriYokai.cpp：可击杀妖怪基类

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Enemies/MatsuriYokai.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Enemies/MatsuriYokai.cpp)。

`AMatsuriYokai` 管碰撞、血量、眩晕、目标查找、视线、迷宫寻路、位移和掉落。这里的移动用碰撞扫掠和迷宫格子路线，不依赖运行时导航网格。普通妖怪是本地规则 AI；Boss 的模型决策在 `Boss/` 中。

| 函数 | 作用 |
|---|---|
| `Matsuri::Yokai::AroundObstacle` | 输入原步长、碰撞法线和相对障碍中心位置，计算沿墙滑动；正面堵住时选择更近边缘方向绕行。 |
| `AMatsuriYokai` | 添加妖怪标签，创建碰撞球和可独立摇摆的外观节点，设置与墙、玩家、准星射线和投射物的碰撞响应。 |
| `BeginPlay` | 血量恢复为最大值，绑定接触玩家事件。 |
| `Tick` | 恢复被击中后短暂放大的外观。 |
| `SetArenaHalfExtent`（inline） | 设置活动场地的半边长。 |
| `GetHealth`（inline） | 返回当前妖怪血量。 |
| `TakeDamage` | 忽略已死/无效伤害；减血、播放受击膨胀、为玩家大招计入伤害；死亡时通知击杀、播特效和声音、按是否双倍掉落调用 `SpawnDrops`，最后销毁。 |
| `SpawnDrops`（基类 inline） | 默认空操作，由具体妖怪重写掉落规则。 |
| `GetFxColor`（基类 inline） | 默认白色，具体妖怪重写受击/死亡特效颜色。 |
| `HandleContact` | 未死亡且接触玩家时，转交楼层统一受伤入口。 |
| `Stun` | 延长眩晕截止时间，延长足够明显时生成头顶星环。 |
| `IsStunned` | 判断眩晕是否仍生效。 |
| `Knockback` | 将击退限制为水平方向后调用扫掠位移。 |
| `FindPlayer` | 返回第一个玩家控制器拥有的 Pawn。 |
| `FindTarget` | 范围内存在忍者分身时优先返回遍历遇到的分身，否则返回玩家；并非寻找最近分身。 |
| `CanSee` | 从怪物眼高对目标射线检测，判断有无遮挡。 |
| `IsWatchedBy` | 要求目标是玩家，再联合玩家视锥与无遮挡条件，判断怪物是否正被玩家看着。 |
| `IsVisibleFrom` | 判断目标眼睛到候选地点是否无遮挡，给河童选掩体使用。 |
| `NextStepToward` | 在迷宫中返回通向目标的下一个门口路点，并可输出通道步数；开放场地直接返回目标。 |
| `ShouldHunt` | 迷宫按通道格数判断感知范围，开放场地按直线距离判断。 |
| `PickWanderGoal` | 迷宫随机选一个相通邻格；开放场地随机选附近、场地内的一点。 |
| `MoveSwept` | 先执行带碰撞位移，撞墙后将剩余步长沿障碍侧面滑动/绕行。 |
| `FaceTowards` | 只在水平面转向目标。 |
| `ClampToArena` | 限制 X/Y 保持在边界内，保留原高度。 |
| `DropCollectible` | 在附近分散位置生成指定金币/勾玉拾取物。 |
| `DropAmmo` | 生成指定数量的非重生弹药掉落物，交给其自身寿命清理。 |

### MatsuriYokaiBrain.h / MatsuriYokaiBrain.cpp：普通妖怪的纯决策

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Enemies/MatsuriYokaiBrain.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Enemies/MatsuriYokaiBrain.cpp)。

该命名空间只根据感知输入返回决定，不直接生成 Actor。`EChochinMove` 是缓慢靠近/潜行/突袭；`EKasaMove` 是寻路/埋伏/扑跳；`EKappaMove` 是躲藏/探头/投掷/等待/接近。`FCoverSpot` 保存候选点位置、是否可达、是否不被玩家看到。

| 函数 | 作用 |
|---|---|
| `IsInViewCone` | 用方向点积判断一个点是否落在视锥中；不负责遮挡检测。 |
| `ChochinMove` | 被看见则缓慢靠近；未被看见且近距离则突袭，否则潜行。 |
| `ChochinSpeed` | 把三种动作映射到速度。 |
| `KasaMove` | 看见且接近玩家时扑跳；在拐角外且近时埋伏；其他情况沿路径追击。 |
| 内部 `NearestReachable` | 从候选点筛出可达且符合隐藏要求的点，返回距离最近的索引。 |
| `PickCover` | 选最近的可达隐藏点。 |
| `PickPeek` | 选最近的可达暴露点，作为探头投掷位置。 |
| `KappaMove` | 超出投掷距离先接近；准备好攻击时看得到就投掷、看不到就探头；冷却中被看到则躲藏，否则等待。 |

### MatsuriYokaiKinds.h / MatsuriYokaiKinds.cpp：三种具体妖怪

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Enemies/MatsuriYokaiKinds.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Enemies/MatsuriYokaiKinds.cpp)。

三个派生类共享基类血量/伤害体系，但有自己的 Tick 和掉落。文件中的 `ChochinTuning`、`KasaTuning`、`KappaTuning` 命名空间保存距离、速度、概率、时间等常量。

| 类 / 函数 | 作用 |
|---|---|
| `AMatsuriChochin::AMatsuriChochin` | 设置提灯怪最大血量 1、碰撞半径和备用提灯形状。 |
| `AMatsuriChochin::BeginPlay` | 优先挂导入模型，否则上发光材质；移至悬浮高度并随机摆动起点。 |
| `AMatsuriChochin::Tick` | 外观上下浮动；未眩晕时在游荡与追击间选择，再根据是否被注视决定爬近/潜行/突袭速度，沿迷宫通道移动。 |
| `AMatsuriChochin::SpawnDrops` | 掉一枚小判金币。 |
| `AMatsuriChochin::GetFxColor`（inline） | 返回橙色特效。 |
| `AMatsuriKasa::AMatsuriKasa` | 设置伞怪最大血量 3、碰撞球、伞面和伞柄备用形状。 |
| `AMatsuriKasa::BeginPlay` | 挂导入模型或涂备用外观，设置接地高度。 |
| `AMatsuriKasa::Tick` | 推进当前跳跃的位移和视觉抛物线；落地等待后判断埋伏、扑跳或短步寻路，远离玩家时小范围跳动。 |
| `AMatsuriKasa::SpawnDrops` | 按概率掉银币，否则掉 5 弹药。 |
| `AMatsuriKasa::GetFxColor`（inline） | 返回紫色特效。 |
| `AMatsuriKappa::AMatsuriKappa` | 设置河童最大血量 4、碰撞球、龟壳和头盘备用形状。 |
| `AMatsuriKappa::BeginPlay` | 挂导入模型或使用绿色备用外观，调整接地高度。 |
| `AMatsuriKappa::Tick` | 更新投掷冷却与感知；按 `KappaMove` 接近、投掷、探头、躲藏或等待，定期重算掩体。 |
| `AMatsuriKappa::FindSpot` | 在三个半径、多个方向采样位置，扫掠检查可达性并检查玩家视线，再选隐藏/暴露点，成功时写入输出坐标。 |
| `AMatsuriKappa::Throw` | 按目标速度预测提前量，再求抛物线方向，生成对玩家敌对的黄瓜投射物。 |
| `AMatsuriKappa::SpawnDrops` | 固定掉 6 弹药，小概率额外掉勾玉。 |
| `AMatsuriKappa::GetFxColor`（inline） | 返回绿色特效。 |

河童 Tick 中 `MoveTo` lambda 只是“向候选点走一步”，最终仍通过基类 `MoveSwept` 处理碰撞。

### MatsuriHazards.h / MatsuriHazards.cpp：狐火和地刺

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Enemies/MatsuriHazards.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Enemies/MatsuriHazards.cpp)。

`AMatsuriHazard` 是危险物基类，仅管理触发器和是否处于伤害状态；`AMatsuriFoxfire` 按正弦移动；`AMatsuriSpikeTrap` 按周期预警/升起。这些危险物不走可击杀妖怪的血量流程。

| 函数 | 作用 |
|---|---|
| 内部 `ConfigureTrigger` | 把触发器配置成只与 Pawn 重叠、不阻挡子弹、不影响导航。 |
| `AMatsuriHazard::AMatsuriHazard` | 开启 Tick 供派生类使用。 |
| `AMatsuriHazard::BeginPlay` | 绑定重叠事件。 |
| `AMatsuriHazard::IsArmed`（inline） | 返回危险物当前是否会伤害玩家。 |
| `AMatsuriHazard::SetArmed` | 切换危险状态；刚开启时检查已站在触发区中的玩家，防止其因没有新重叠事件而免伤。 |
| `AMatsuriHazard::HandleOverlap` | 只有 armed 且接触玩家才发送统一受伤请求。 |
| `AMatsuriFoxfire::AMatsuriFoxfire` | 建立球形触发器和狐火外观。 |
| `AMatsuriFoxfire::BeginPlay` | 设置紫蓝发光、原点，并依据位置种子决定移动轴和初始相位。 |
| `AMatsuriFoxfire::Tick` | 沿固定轴往返扫动并上下轻微漂浮。 |
| `AMatsuriSpikeTrap::AMatsuriSpikeTrap` | 建立方触发器、底板、四根地刺，初始不伤害。 |
| `AMatsuriSpikeTrap::BeginPlay` | 调整高度、设置材质，并按位置确定周期偏移。 |
| `AMatsuriSpikeTrap::Tick` | 推进三秒周期：底板先变红，随后升刺并 armed，周期归零则收刺/解除。 |

## 三、Pickups：拾取如何改变当前局

### MatsuriCollectible.h / MatsuriCollectible.cpp：小判、银币、勾玉

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Pickups/MatsuriCollectible.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Pickups/MatsuriCollectible.cpp)。

`AMatsuriCollectible` 保存种类和是否已领取，用旋转组件让物品展示旋转；触发器忽略投射物，避免黄瓜撞到金币就消失。内部 `FCollectibleLook` 保存备用形状、尺寸和颜色。

| 函数 | 作用 |
|---|---|
| 内部 `LookFor` | 按拾取物种类返回备用模型尺寸/颜色，勾玉用球体、硬币用竖立圆柱。 |
| `AMatsuriCollectible` | 创建触发球、显示模型、旋转组件，并加载基本形状供缺资源时备用。 |
| `SetKind` | 保存种类并立即刷新外观，可在延迟生成完成前调用。 |
| `GetKind`（inline） | 返回物品种类。 |
| `OnConstruction` | 编辑器构造/刷新时应用外观，便于预览。 |
| `BeginPlay` | 绑定玩家重叠事件。 |
| `ApplyVisuals` | 优先使用导入模型，否则选择基础形状和发光材质。 |
| `HandleOverlap` | 只允许玩家且只领一次；取得运行子系统后播声/特效、调用 `Collect` 改变分数/充能等状态，然后销毁物品。 |

### MatsuriAmmoPickup.h / MatsuriAmmoPickup.cpp：弹药符

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Pickups/MatsuriAmmoPickup.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Pickups/MatsuriAmmoPickup.cpp)。

`AMatsuriAmmoPickup` 保存弹药量、是否重生、是否可领取和重生计时器。固定弹药点可以重生，敌人掉落则会过期。

| 函数 | 作用 |
|---|---|
| `AMatsuriAmmoPickup` | 创建触发球、御札备用外观和旋转组件，忽略投射物碰撞。 |
| `Configure` | 设置至少 1 发弹药；固定点不自动过期，临时掉落设置寿命。 |
| `BeginPlay` | 导入御札模型或使用蓝色发光备用形状，绑定重叠。 |
| `IsAvailable`（inline） | 返回当前能否拾取，吸引技能用它排除已领走等待重生的点。 |
| `HandleOverlap` | 玩家接触时调用 `AddAmmo`；已满不消耗物品；成功则播反馈，临时物品销毁，固定点隐藏并启动重生计时。 |
| `SetAvailable` | 同步设置可领取标记、显示和碰撞状态。 |
| `Respawn` | 重生计时结束后重新启用物品。 |

## 四、Level：地图如何从数据变成可玩的场景

### MatsuriFloors.h / MatsuriFloors.cpp：楼层参数表

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Level/MatsuriFloors.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Level/MatsuriFloors.cpp)。

`FMatsuriFloorTheme` 只描述标题、日/月光、雾、颜色、曝光和灯笼。`Count=3` 是三层迷宫，`BossFloor=3`、`Total=4` 包括最终 Boss。代码楼层编号从 0 开始；UI 显示从 1 开始。

| 函数 | 作用 |
|---|---|
| 内部 `ClampFloor` | 将索引夹到四个有效楼层范围内。 |
| `Matsuri::Floors::IsBossFloor`（inline） | 判断索引是否等于 Boss 楼层。 |
| `SpecFor` | 生成指定楼层的玩法参数并混合本局种子；三层分别配置迷宫大小、目标分数、时限、怪物/陷阱/弹药数量。Boss 使用固定场地，只使用相关时限/目标配置。 |
| `ThemeFor` | 返回黄昏竹林、夜店街、千本鸟居或月下本殿的灯光/雾/颜色。 |
| `MapFor` | 返回该楼层 `.umap` 的包路径。 |
| `IsLastFloor` | 判断当前是否已到最后 Boss 层，供胜利/转关流程使用。 |

### MatsuriMaze.h / MatsuriMaze.cpp：迷宫生成和通道寻路

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Level/MatsuriMaze.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Level/MatsuriMaze.cpp)。

`FMatsuriMaze` 是纯数据，没有 Actor。每个格子用四个位记录东/北/西/南墙；`FDirection` 保存方向偏移、该侧墙和对面墙；方向表帮助两侧同步开门。

| 函数 | 作用 |
|---|---|
| 内部 `Open` | 同时清除相邻两格之间相对的墙位，打开通道。 |
| 内部 `CarvePerfectMaze` | 用栈和访问标记做随机回溯，生成所有格子连通的基本迷宫。 |
| 内部 `AddLoops` | 按概率敲掉额外内墙，形成可绕行的环路。 |
| 内部 `AddPlazas` | 打通指定数量的 2×2 小广场，供战斗腾出空间。 |
| `Matsuri::Maze::Generate` | 建立全封墙网格，再依次挖基本迷宫、加环路、加广场，返回完整数据。相同参数/种子得到相同结果。 |
| `FMatsuriMaze::CellSize`（inline） | 计算每格对应多少厘米。 |
| `CellCount`（inline） | 返回总格子数。 |
| `IsValid`（inline） | 检查尺寸与墙数组长度是否匹配。 |
| `InBounds`（inline） | 检查格坐标是否在范围内。 |
| `Index`（inline） | 二维格坐标转一维数组索引。 |
| `CellOf`（inline） | 数组索引转二维格坐标。 |
| `CellAt` | 世界 XY 坐标转所在格，超边界时夹回网格。 |
| `CellCenter` | 格坐标转世界中心点。 |
| `HasWall`（inline） | 读取一格某方向是否仍封墙。 |
| `OpenNeighbours` | 返回一步可直接穿过的相邻格。 |
| `CountPassages` | 统计内部连通通道，每道门只计一次。 |
| `StepsTo` | 从目标格做广度优先搜索，得到所有格到目标的最少步数，不可达为 `INDEX_NONE`。 |
| `NextWaypoint` | 按最短步数找更接近目标的邻格，返回略越过门口中点的路点；同格直接返回目标，可额外输出剩余步数。 |

### MatsuriFloorLayout.h / MatsuriFloorLayout.cpp：放墙、金币和怪物的位置

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Level/MatsuriFloorLayout.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Level/MatsuriFloorLayout.cpp)。

`FMatsuriFloorSpec` 是生成参数；`EMatsuriBlock` 区分地面、墙、低掩体和柱子，`EMatsuriSpawn` 区分怪物、陷阱、弹药；各 `Placement` 结构保存类型/位置/尺寸；`FMatsuriFloorLayout` 汇总位置列表、出生点、朝向、迷宫和起终点。内部 `FSpotSampler` 保存已占用位置，避免物品互相重叠。这个文件只算数据，不创建场景 Actor。

| 函数 | 作用 |
|---|---|
| 内部 `Overlaps` | 检查两个二维矩形是否有真正重叠。 |
| 内部 `OverlapsAny` | 检查某矩形是否与列表任一矩形重叠。 |
| 内部 `MakeFootprint` | 由三维中心和尺寸计算落地占用矩形。 |
| 内部 `AddBoundary` | 向布局写入地面和四周围墙。 |
| 内部 `RollObstacle` | 用种子随机流生成一根柱子或低掩体候选，尺寸/位置保持在场地内。 |
| 内部 `AddObstacles` | 多次采样候选，避开出生安全区和其他障碍，留足通行间距后记录。 |
| `FSpotSampler::IsFree` | 检查候选点距出生点、障碍占地和已放物品是否够远。 |
| `FSpotSampler::Take` | 在整个可用区域中调用 `TakeIn` 找点。 |
| `FSpotSampler::TakeIn` | 在指定矩形中有限次数采样，找到空位就记录并返回；失败返回空 Optional。 |
| 内部 `AddCoins` | 稀有勾玉优先，其次银币/小判，逐个寻找安全点。 |
| 内部 `AddSpawns` | 按设定数量放怪物、陷阱、弹药；危险物要求更大的出生距离。 |
| `FMatsuriBlockPlacement::GetFootprint` | 返回自身落地矩形。 |
| `FMatsuriFloorLayout::CountCoins` | 统计指定金币种类数量。 |
| `CountSpawns` | 统计指定生成物种类数量。 |
| `TotalPoints` | 累加布局里可拾取物的基础分值。 |
| 内部 `BuildArenaLayout` | 开放场地路线：出生点→边界→随机障碍→金币→怪物/陷阱/弹药。 |
| 内部 `CellInterior` | 返回迷宫格内避开墙边后的可用矩形。 |
| 内部 `AddMazeWalls` | 把迷宫墙位转换成尺寸明确的墙块数据，只为共享墙记一份。 |
| 内部 `AddMazeCover` | 远离出生处随机放格内低掩体，并在部分墙交汇处放柱子，保留门口通道。 |
| 内部 `ShuffledCells` | 挑出距起点足够多步的格，洗牌后作为放置候选。 |
| 内部 `TakeInCells` | 从候选格循环尝试，在下一个有空位的格内放一个点，并推进游标。 |
| 内部 `AddMazeItems` | 将金币/弹药分散到非起始格，把怪物/陷阱分散到更远的格，并继续检查安全距离。 |
| 内部 `BuildMazeLayout` | 生成迷宫，指定起终点和面向开放门口的出生朝向，添加边界/迷宫墙/掩体/物品。 |
| `Matsuri::Layout::BuildFloorLayout` | 根据 `bMaze` 选择迷宫或开放场地生成器。 |

### MatsuriFloorBuilder.h / MatsuriFloorBuilder.cpp：把位置数据生成 Actor

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Level/MatsuriFloorBuilder.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Level/MatsuriFloorBuilder.cpp)。

`Matsuri::Builder` 将纯布局转成世界里的灯光、碰撞体和可交互对象。它不决定胜负，也不负责迷宫算法。

| 函数 | 作用 |
|---|---|
| 内部 `BlockColor` | 按墙/低掩体/柱子读取主题颜色。 |
| 内部 `SurfaceFor` | 决定块体使用木材还是石材，灰柱按石灯处理。 |
| 内部 `ClassFor` | 把生成物枚举映射到具体妖怪、陷阱或弹药类。 |
| `SpawnEnvironment` | 按主题创建太阳/月光、天空/天光、雾和全局后期；设置曝光、泛光、饱和度等，并关闭移动模糊。 |
| `SpawnBlocks` | 将每个块转成带碰撞的基本立方体，按地面/墙柱选择贴图或备用材质。 |
| `SpawnCollectibles` | 按布局延迟生成拾取 Actor，在完成生成前设置物品种类。 |
| `SpawnSpawns` | 生成怪物/陷阱/弹药并配置怪物活动范围和固定弹药重生；关闭 threats 时保留弹药、略过危险物。 |

### MatsuriFloorDecor.h / MatsuriFloorDecor.cpp：神社装饰及防穿模

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Level/MatsuriFloorDecor.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Level/MatsuriFloorDecor.cpp)。

这是场景“好看”的部分：鸟居、石灯、竹林、摊位、墙帽。游戏碰撞主要继续使用 Builder 的块体。内部 `FShape` 是基本形状配方；`FOccupancy` 保存块体、金币/出生点、已放装饰的包围盒；`FFitRule` 指定可忽略哪个被替代块；`EArt` 区分放置成功、缺资源和放不下。装饰放置失败不能等同于资源缺失。

| 函数 | 作用 |
|---|---|
| `FOccupancy::Fits` | 检查模型包围盒是否会穿进块体、物品或已放装饰；允许贴靠的微小容差以及按规则忽略被替换块。 |
| 内部 `Spawn` | 按基本形状配方生成静态网格 Actor，设置尺寸、材质、碰撞和可见性。 |
| 内部 `ArtBounds` | 计算导入模型在指定变换下占用的世界包围盒。 |
| 内部 `SpawnArt` | 查资源、检查占用、放模型并记录占地，返回 `Placed/Missing/Blocked`；模型本身无碰撞。 |
| 内部 `HideBlockAt` | 找到被装饰替代的灰盒块并隐藏，仅保留其碰撞。 |
| 内部 `SpawnLight` | 生成不投影的可移动点光，设置色彩、强度和半径。 |
| 内部 `SpawnTorii` | 放鸟居并按通道宽度适配；导入模型缺失时用形状拼；鸟居柱保留对应碰撞，放不下则省略。 |
| 内部 `SpawnLantern` | 放纸灯笼、缺资源时用球/圆柱替代；指定强度时附带真实点光。 |
| 内部 `SpawnApproachLantern` | 在入口/出口旁放小石灯、对应不可见碰撞及灯火。 |
| 内部 `DressFence` | 沿外围添加灯笼串、花岗岩基脚和深色墙帽。 |
| 内部 `DressPillarArt` | 尝试用尺寸合适的石灯替代柱子并加灯；太矮/放不下就返回失败。 |
| 内部 `DressCoverArt` | 二层尝试用摊位替代近方形掩体，三层尝试在掩体台座上摆狐狸像；返回值表示原掩体外观是否被完整替代。 |
| 内部 `DressBlocks` | 遍历柱子/掩体，优先放模型；成功替代则隐藏灰盒，否则补基本石灯或摊位遮阳篷。 |
| 内部 `TrimWall` | 给内墙加深色顶部梁和石质基脚，保持原碰撞。 |
| 内部 `DressMazeWalls` | 装饰迷宫墙段，隔段加灯笼并为部分灯笼开点光。 |
| 内部 `DressApproach` | 在起终点靠围墙的角落放石灯，避开怪物穿格路径。 |
| 内部 `FenceOuter` | 从布局计算外围厚墙最外侧位置。 |
| 内部 `DressBambooArt` | 按模型尺寸/旋转把竹丛放在围墙外，避免竹子穿墙。 |
| 内部 `DressBamboo` | 有导入竹丛就使用它，否则用大量细长圆柱模拟竹林。 |
| 内部 `MakeOccupancy` | 从块体、金币、生成点和玩家出生点建立防穿模占用数据。 |
| `Matsuri::Decor::DressFloor` | 临时启用该层占用检查，按迷宫/开放场地摆鸟居，随后装饰墙、块、入口，并只在一层添加竹林。 |
| `PlaceArt` | 对标题舞台等调用方提供简化的模型放置接口，返回是否成功。 |
| `PlaceLantern` | 对外提供纸灯笼与灯光的放置接口。 |

### MatsuriTitleStage.h / MatsuriTitleStage.cpp：标题菜单背后的三人舞台

源码：[头文件](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Level/MatsuriTitleStage.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Matsuri/Level/MatsuriTitleStage.cpp)。

`Matsuri::TitleStage` 创建菜单背景的 3D 场景。导入角色模型后会播放待机动画。干净源码副本也能渲染程序化鸟居和灯笼。

| 函数 | 作用 |
|---|---|
| 内部 `SpawnHeroine` | 在指定点生成该角色骨骼模型，让她面向镜头并循环 idle，随机起播时间以避免三人完全同步。 |
| 内部 `SpawnKeyLight` | 创建照亮角色脸部的主点光，可投影。 |
| 内部 `SpawnFallbackTorii` | 用程序化梁、柱及牌匾组成朱红鸟居，并铺设石台与参道。 |
| `Build` | 建造地面、灯笼和主光，按可用素材放置鸟居、摊位、竹林与角色，然后返回菜单摄像机。 |

## 五、想改什么，先看哪份文件

| 想修改的效果 | 优先入口 |
|---|---|
| 普攻射速、伤害、散射、弹量 | 本篇的 `MatsuriCombatComponent` 实现流程；基础数据在 `Core/MatsuriHeroes`。 |
| 蓄力弓手感、第一人称摆动 | `MatsuriCombatComponent`、`MatsuriViewModel`。 |
| 三位角色 Shift/E/Q 的真实效果 | `MatsuriSkillComponent`。 |
| 大招影片打开/跳过/播放故障处理 | 调用从 `MatsuriSkillComponent::TryUseSkill` 发起；播放器实现见 `UI/MatsuriCinematicWidget`。 |
| 箭插墙、铃铛弹跳、河童黄瓜抛物线 | `MatsuriProjectile`。 |
| 普通怪物更聪明或更凶 | `MatsuriYokaiBrain` 的决定、`MatsuriYokaiKinds` 的实际动作与参数。 |
| 普通怪物接触造成多少玩家 HP 伤害 | `Core/MatsuriFloorGameMode` 的受伤入口；`MatsuriYokai` 只发请求。 |
| 每关数量、时限、迷宫大小 | `MatsuriFloors::SpecFor`。 |
| 灯光颜色与夜晚明暗 | `MatsuriFloors::ThemeFor`、`MatsuriFloorBuilder::SpawnEnvironment`。 |
| 三渲二材质与描边的未来接入 | 当前仅有普通材质/调色；需结合 `Core/MatsuriArt`、`MatsuriGraybox` 与这里的 Builder/Decor，尚不能把它们称为已完成的卡通着色系统。 |

阅读提醒：源码中有些注释沿用早期灰盒、生命次数等说法；理解当前行为以函数体和实际调用为准。本篇是静态代码导读，没有启动游戏或重新验证运行效果。
