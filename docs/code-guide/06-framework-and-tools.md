# 基础框架、模板代码与制作工具

本章按当前源码解释游戏模块、角色输入底座、UE 随项目保留的两个模板变体，以及制作资源的脚本。每个 `.h` 负责声明类、成员与接口；对应 `.cpp` 负责下表中的实现。蓝图事件只有声明，实际视觉行为需要查看对应蓝图，不能把声明当成已有 C++ 实现。

夜祭主流程由 `Matsuri/Core` 和 `Matsuri/Boss` 的 GameMode 驱动。`MatsuriFloorGameMode` 加载 `BP_FirstPersonCharacter` 和 `BP_FirstPersonPlayerController`，根目录的第一人称底座仍然有用；`Variant_Horror` 与 `Variant_Shooter` 是其他模板玩法代码，不能把它们的生命值、重生、队伍分数当成夜祭当前规则。二进制蓝图内部引用没有在本章逐节点审阅，因此这里不声称这些保留文件已经完全无引用。

## 1. 模块与编译配置

| 文件 | 内容、入口与作用 |
|---|---|
| [SuzuneMatsuri.h](../../SuzuneMatsuri/Source/SuzuneMatsuri/SuzuneMatsuri.h) | 声明 `LogSuzuneMatsuri` 日志类别，没有普通函数。 |
| [SuzuneMatsuri.cpp](../../SuzuneMatsuri/Source/SuzuneMatsuri/SuzuneMatsuri.cpp) | `IMPLEMENT_PRIMARY_GAME_MODULE` 将本模块注册为游戏主模块；`DEFINE_LOG_CATEGORY` 定义日志类别。宏生成入口，不是手写的 `main()`。 |
| [SuzuneMatsuri.Build.cs](../../SuzuneMatsuri/Source/SuzuneMatsuri/SuzuneMatsuri.Build.cs) | `SuzuneMatsuri(Target)` 构造模块编译规则：启用共享/显式预编译头，声明引擎、增强输入、Niagara、AI、StateTree、UMG、JSON、MediaAssets 等依赖；私有加入 HTTP 和渲染计时依赖；编辑器构建额外加入 UnrealEd；设置各源码目录的头文件搜索路径。 |
| [SuzuneMatsuri.Target.cs](../../SuzuneMatsuri/Source/SuzuneMatsuri.Target.cs) | `SuzuneMatsuriTarget(Target)` 设置独立游戏构建目标、UE5.8 头文件顺序与 V7 构建设置，并加入主模块。 |
| [SuzuneMatsuriEditor.Target.cs](../../SuzuneMatsuri/Source/SuzuneMatsuriEditor.Target.cs) | `SuzuneMatsuriEditorTarget(Target)` 设置编辑器构建目标，其余版本与模块设置和游戏目标对应。 |

`.Build.cs` 回答“模块要依赖哪些库”；`.Target.cs` 回答“这次要编译游戏还是编辑器”。这些 C# 文件用于构建，不是运行时玩法脚本。

## 2. 根目录第一人称底座

### SuzuneMatsuriCharacter.h / SuzuneMatsuriCharacter.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/SuzuneMatsuriCharacter.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/SuzuneMatsuriCharacter.cpp)。类 `ASuzuneMatsuriCharacter` 继承引擎 `ACharacter`，提供胶囊、第一人称网格、摄像机、移动与增强输入绑定。`matsuri.LookSensitivity` 控制视角灵敏度。

| 函数 | 作用 |
|---|---|
| `ASuzuneMatsuriCharacter()` | 创建第一人称网格和摄像机，设置自见/他见方式、视野、碰撞胶囊，并调整加速度、刹车和空中控制。 |
| `SetupPlayerInputComponent` | 取得增强输入组件，将跳跃开始/结束、移动、手柄和鼠标视角绑定到处理函数；组件不合适则写错误日志。 |
| `MoveInput` | 从输入值取二维向量，传给 `DoMove`。 |
| `LookInput` | 从输入值取二维向量，传给 `DoAim`。 |
| `DoAim` | 有控制器时，将水平和垂直输入乘灵敏度，修改控制器朝向。 |
| `DoMove` | 有控制器时，沿角色右向和前向添加移动输入。 |
| `DoJumpStart` | 调用引擎 `Jump()` 开始跳跃。 |
| `DoJumpEnd` | 调用 `StopJumping()` 结束持续跳跃输入。 |
| `GetFirstPersonMesh` | 内联返回第一人称网格指针。 |
| `GetFirstPersonCameraComponent` | 内联返回第一人称摄像机指针。 |

### SuzuneMatsuriPlayerController.h / SuzuneMatsuriPlayerController.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/SuzuneMatsuriPlayerController.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/SuzuneMatsuriPlayerController.cpp)。`ASuzuneMatsuriPlayerController` 管理输入映射和可选触控界面。

| 函数 | 作用 |
|---|---|
| `ASuzuneMatsuriPlayerController()` | 选择项目自定义摄像机管理器。 |
| `BeginPlay` | 本地玩家需要触控时，创建并显示移动端操作界面；失败写日志。 |
| `SetupInputComponent` | 为本地玩家的增强输入子系统注册默认映射；非触控模式再注册桌面专用映射。 |
| `ShouldUseTouchControls` | 判断引擎是否要求虚拟摇杆，或配置是否强制触控。 |

### SuzuneMatsuriCameraManager.h / SuzuneMatsuriCameraManager.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/SuzuneMatsuriCameraManager.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/SuzuneMatsuriCameraManager.cpp)。类只有构造函数 `ASuzuneMatsuriCameraManager()`：把俯仰范围限制为 −70° 到 80°，避免摄像机无限翻转。

### SuzuneMatsuriGameMode.h / SuzuneMatsuriGameMode.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/SuzuneMatsuriGameMode.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/SuzuneMatsuriGameMode.cpp)。基础模板 GameMode；唯一构造函数 `ASuzuneMatsuriGameMode()` 当前为空。夜祭的关卡规则见 `MatsuriFloorGameMode` 等专用类。

## 3. Horror 恐怖模板变体

### HorrorCharacter.h / HorrorCharacter.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Horror/HorrorCharacter.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Horror/HorrorCharacter.cpp)。`AHorrorCharacter` 扩展第一人称角色，增加手电筒、冲刺体力、耗尽后的慢走恢复，以及体力/冲刺事件。

| 函数 | 作用 |
|---|---|
| `AHorrorCharacter()` | 创建附着于摄像机的聚光灯，设置位置、亮度、照射半径和光锥。 |
| `BeginPlay` | 体力填满，设置步行速度，启动固定周期的体力计时器。 |
| `EndPlay` | 清理冲刺计时器。 |
| `SetupPlayerInputComponent` | 保留基类移动输入，再绑定冲刺按下/松开。 |
| `DoStartSprint` | 记录按住冲刺；未处于强制恢复时加速并广播冲刺开始。 |
| `DoEndSprint` | 记录松开冲刺；未处于恢复时恢复步速并广播结束。 |
| `SprintFixedTick` | 实际高速移动时消耗体力，耗尽后强制慢走；否则恢复体力，满值后解除恢复状态；每次广播体力比例。 |

### HorrorGameMode.h / HorrorGameMode.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Horror/HorrorGameMode.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Horror/HorrorGameMode.cpp)。保存本地玩家数量和出生点分配序号。

| 函数 | 作用 |
|---|---|
| `AHorrorGameMode()` | 空构造函数。 |
| `BeginPlay` | 按配置创建第 2 位起的额外本地玩家。 |
| `ChoosePlayerStart_Implementation` | 优先查找 `Player0`、`Player1` 等标签的出生点并推进分配序号；没有匹配则从全部出生点随机选；全无则返回空。 |

### HorrorPlayerController.h / HorrorPlayerController.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Horror/HorrorPlayerController.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Horror/HorrorPlayerController.cpp)。管理恐怖模式 UI、输入和可选触控。

| 函数 | 作用 |
|---|---|
| `AHorrorPlayerController()` | 指定项目摄像机管理器。 |
| `BeginPlay` | 当前只调用父类初始化。 |
| `OnPossess` | 接管恐怖角色后，本地玩家创建 UI，并让 UI 订阅该角色体力事件。 |
| `SetupInputComponent` | 加入默认输入和桌面专用输入映射；触控模式创建移动端操作界面。 |
| `ShouldUseTouchControls` | 返回平台触控需求或强制触控配置。 |

### UI/HorrorUI.h / HorrorUI.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Horror/UI/HorrorUI.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Horror/UI/HorrorUI.cpp)。`UHorrorUI` 将角色事件转交给蓝图显示。

| 函数 | 作用 |
|---|---|
| `SetupCharacter` | 订阅角色体力变化和冲刺状态变化委托。 |
| `OnSprintMeterUpdated` | 将体力比例转发给蓝图事件。 |
| `OnSprintStateChanged` | 将冲刺布尔状态转发给蓝图事件。 |
| `BP_SprintMeterUpdated` | 蓝图实现接口：更新体力显示；C++ 无函数体。 |
| `BP_SprintStateChanged` | 蓝图实现接口：更新冲刺外观；C++ 无函数体。 |

## 4. Shooter 射击模板变体

### ShooterCharacter.h / ShooterCharacter.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/ShooterCharacter.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/ShooterCharacter.cpp)。`AShooterCharacter` 是带武器背包、队伍、模板 HP 和死亡重生的角色，实现 `IShooterWeaponHolder`。这里的默认 500 HP 与夜祭 `FMatsuriRunState` 的 HP 是两套规则。

| 函数 | 作用 |
|---|---|
| `AShooterCharacter()` | 创建 AI 噪声发射组件，设置转向速度。 |
| `BeginPlay` | HP 设为上限，广播满血。 |
| `EndPlay` | 清除重生计时器。 |
| `SetupPlayerInputComponent` | 在基类输入上增加开火、停止开火和切枪。 |
| `TakeDamage` | 已死则拒绝；否则扣模板 HP，致死时调用 `Die`，再广播剩余血量比例并返回伤害。 |
| `DoAim` | 活着才将视角输入交给基类。 |
| `DoMove` | 活着才移动。 |
| `DoJumpStart` | 活着才开始跳跃。 |
| `DoJumpEnd` | 活着才转发跳跃结束。 |
| `DoStartFiring` | 有武器且活着时要求武器开火。 |
| `DoStopFiring` | 有武器且活着时要求武器停火。 |
| `DoSwitchWeapon` | 至少两把枪且活着时，停用当前枪，循环切到下一把并启用。 |
| `AttachWeaponMeshes` | 将武器 Actor 和第一/第三人称网格挂到角色及手部插槽。 |
| `PlayFiringMontage` | 当前为空，未在 C++ 播放开火蒙太奇。 |
| `AddWeaponRecoil` | 将后坐力转成控制器俯仰输入。 |
| `UpdateWeaponHUD` | 广播弹匣容量和剩余子弹。 |
| `GetWeaponTargetLocation` | 从摄像机向前射线检测，返回障碍命中点或最大射程终点。 |
| `AddWeaponClass` | 检查是否已有该类型；没有则生成、加入背包、停用旧枪并切到新枪。 |
| `OnWeaponActivated` | 刷新子弹 UI，并设置角色第一/第三人称动画实例类。 |
| `OnWeaponDeactivated` | 当前空实现。 |
| `OnSemiWeaponRefire` | 当前空实现，玩家半自动不在此自动续射。 |
| `FindWeaponOfType` | 遍历背包寻找对应武器类型，找不到返回空。 |
| `Die` | 停用武器，通知模板 GameMode 计分，加入死亡标签，停移动/碰撞/输入，清弹药 UI，触发蓝图死亡事件并安排延迟重生。 |
| `BP_OnDeath` | 蓝图死亡扩展接口，C++ 无函数体。 |
| `OnRespawn` | 销毁角色，让控制器的销毁监听负责生成新角色。 |
| `IsDead` | 判断模板 HP 是否小于等于零。 |
| `SetTeam` | 设置角色队伍编号。 |

### ShooterGameMode.h / ShooterGameMode.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/ShooterGameMode.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/ShooterGameMode.cpp)。保存队伍分数、本地玩家数与比分 UI。

| 函数 | 作用 |
|---|---|
| `BeginPlay` | 创建比分界面，创建额外本地玩家并交替设置队伍。 |
| `ChoosePlayerStart_Implementation` | 依序找玩家标签出生点，找不到则在所有出生点中随机选择。 |
| `IncrementTeamScore` | 指定队伍计数加一，并调用蓝图 UI 更新比分。 |
| `ShouldSpawnEnemyNPCs` | 只有本地玩家数少于 2 时允许刷 NPC。 |

### ShooterPlayerController.h / ShooterPlayerController.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/ShooterPlayerController.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/ShooterPlayerController.cpp)。管理射击模板的输入、弹药/血条 UI、队伍和重生。

| 函数 | 作用 |
|---|---|
| `BeginPlay` | 当前仅父类初始化。 |
| `SetupInputComponent` | 注册本地输入映射，必要时创建触控界面，并创建弹药计数 UI。 |
| `OnPossess` | 监听 Pawn 销毁；若是射击角色则加玩家标签、设置队伍、订阅弹药与伤害事件并刷新血条。 |
| `OnPawnDestroyed` | 清空弹药 UI，按队伍标签随机挑出生点，生成并接管新角色。 |
| `OnBulletCountUpdated` | 将弹匣容量/剩余数量转交蓝图计数 UI。 |
| `OnPawnDamaged` | 将生命比例交给蓝图伤害显示。 |
| `ShouldUseTouchControls` | 判断平台触控需求或强制触控配置。 |
| `SetTeam` | 保存控制器队伍，同时同步给已经接管的射击角色。 |

### Weapons/ShooterWeaponHolder.h / ShooterWeaponHolder.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/Weapons/ShooterWeaponHolder.h)、[实现单元](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/Weapons/ShooterWeaponHolder.cpp)。`UShooterWeaponHolder` 是 UE 反射接口壳，`IShooterWeaponHolder` 是 C++ 纯虚接口。`.cpp` 没有函数实现。

| 接口函数 | 要求持枪者提供的能力 |
|---|---|
| `AttachWeaponMeshes` | 挂接武器外观。 |
| `PlayFiringMontage` | 处理开火动画。 |
| `AddWeaponRecoil` | 处理后坐力。 |
| `UpdateWeaponHUD` | 更新弹药显示。 |
| `GetWeaponTargetLocation` | 提供瞄准位置。 |
| `AddWeaponClass` | 接收一类新武器。 |
| `OnWeaponActivated` | 响应武器启用。 |
| `OnWeaponDeactivated` | 响应武器停用。 |
| `OnSemiWeaponRefire` | 响应半自动冷却结束。 |

接口本身不决定动画、伤害或背包；玩家和 NPC 分别实现这些方法，空实现也算履行接口。

### Weapons/ShooterWeapon.h / ShooterWeapon.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/Weapons/ShooterWeapon.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/Weapons/ShooterWeapon.cpp)。`AShooterWeapon` 管两套视角网格、弹药、连射计时、投射物、噪声和与持枪者的交互。

| 函数 | 作用 |
|---|---|
| `AShooterWeapon()` | 创建根节点与第一/第三人称武器网格，设置各自可见性和无碰撞。 |
| `BeginPlay` | 监听持有者销毁，取得持枪接口/Pawn，填满弹匣，请持有者挂接网格。 |
| `EndPlay` | 清除再次开火计时器。 |
| `OnOwnerDestroyed` | 持有者销毁时也销毁武器。 |
| `ActivateWeapon` | 保存噪声阵营标签、显示枪并通知持有者。 |
| `DeactivateWeapon` | 停火、隐藏枪并通知持有者。 |
| `StartFiring` | 设置开火意图，根据上次开火时间立即发射或为全自动安排定时发射。 |
| `StopFiring` | 清开火意图及定时器。 |
| `Fire` | 仍有开火意图才发射；读取持有者瞄准点，记录开火时间/噪声，按全自动或半自动设置下一次回调。 |
| `FireCooldownExpired` | 通知持有者半自动冷却结束。 |
| `FireProjectile` | 生成模板弹丸、设置噪声标签，要求持有者动画/后坐力，扣一发弹药，空匣立即补满并更新 UI。 |
| `CalculateProjectileSpawnTransform` | 按枪口插槽、前推距离和目标随机偏差计算弹丸位置与朝向。 |
| `GetFirstPersonMesh` | 内联返回第一人称枪网格。 |
| `GetThirdPersonMesh` | 内联返回第三人称枪网格。 |
| `GetFirstPersonAnimInstanceClass` | 返回对应角色第一人称动画类。 |
| `GetThirdPersonAnimInstanceClass` | 返回对应角色第三人称动画类。 |
| `GetMagazineSize` | 内联返回容量。 |
| `GetBulletCount` | 内联返回剩余子弹。 |

### Weapons/ShooterProjectile.h / ShooterProjectile.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/Weapons/ShooterProjectile.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/Weapons/ShooterProjectile.cpp)。模板弹丸使用引擎 `UProjectileMovementComponent`，可单体命中或爆炸；与 Boss 批量弹幕实现不同。

| 函数 | 作用 |
|---|---|
| `AShooterProjectile()` | 创建碰撞球与运动组件，设速度/弹跳和默认伤害类型。 |
| `BeginPlay` | 移动检测忽略发射者。 |
| `EndPlay` | 清延迟销毁计时器。 |
| `NotifyHit` | 首次命中时关闭碰撞、制造 AI 噪声，执行单体或范围命中，触发蓝图特效，再立即或延迟销毁。 |
| `ExplosionCheck` | 球形查询附近 Pawn/动态/物理对象，忽略不允许伤害的发射者，同一 Actor 只处理一次。 |
| `ProcessHit` | 对角色调用引擎伤害接口，对模拟物理的组件施加冲量。 |
| `BP_OnProjectileHit` | 蓝图命中特效接口，无 C++ 实现。 |
| `OnDeferredDestruction` | 定时销毁弹丸。 |
| `SetNoiseTag` | 保存感知噪声标签。 |

### Weapons/ShooterPickup.h / ShooterPickup.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/Weapons/ShooterPickup.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/Weapons/ShooterPickup.cpp)。`FWeaponTableRow` 保存显示网格的软引用与武器类；`AShooterPickup` 是可重生武器拾取物。

| 函数 | 作用 |
|---|---|
| `AShooterPickup()` | 创建根、玩家重叠球与显示网格，绑定重叠事件。 |
| `OnConstruction` | 编辑器构造阶段读取数据表设置显示网格。 |
| `BeginPlay` | 从数据表取得实际要赠送的武器类。 |
| `EndPlay` | 清重生计时器。 |
| `OnOverlap` | 对实现持枪接口的 Actor 赠送武器，隐藏并禁用拾取物，安排重生。 |
| `RespawnPickup` | 恢复可见并触发蓝图重生动画。 |
| `BP_OnRespawn` | 蓝图动画接口；应在动画完成后调用 `FinishRespawn`。 |
| `FinishRespawn` | 恢复拾取碰撞与 Tick。 |

## 5. 射击模板 AI 与 UI

### AI/ShooterAIController.h / ShooterAIController.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/AI/ShooterAIController.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/AI/ShooterAIController.cpp)。持有 StateTree、AI 感知组件和当前敌人；感知事件交给状态树任务处理。

| 函数 | 作用 |
|---|---|
| `AShooterAIController()` | 创建状态树/感知组件，禁止状态树自动启动，绑定感知更新与遗忘事件。 |
| `OnPossess` | 接管模板 NPC 时赋阵营标签、监听死亡并启动状态树。 |
| `OnPawnDeath` | 中止寻路、停状态树、解除接管并销毁控制器。 |
| `SetCurrentTarget` | 保存目标 Actor。 |
| `ClearCurrentTarget` | 清空目标。 |
| `GetCurrentTarget` | 内联返回目标。 |
| `OnPerceptionUpdated` | 将 Actor 和刺激信息转交已绑定的状态树委托。 |
| `OnPerceptionForgotten` | 将遗忘 Actor 转交委托。 |

### AI/ShooterNPC.h / ShooterNPC.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/AI/ShooterNPC.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/AI/ShooterNPC.cpp)。模板持枪敌人，实现同一持枪接口，拥有 HP、瞄准偏差、死亡广播和布娃娃物理。

| 函数 | 作用 |
|---|---|
| `BeginPlay` | 以自身为 Owner/Instigator 生成配置武器。 |
| `EndPlay` | 清死亡后销毁计时器。 |
| `TakeDamage` | 已死则忽略；否则扣 HP，耗尽时死亡。 |
| `AttachWeaponMeshes` | 将武器和两套网格挂到角色对应插槽。 |
| `PlayFiringMontage` | 空实现。 |
| `AddWeaponRecoil` | 空实现。 |
| `UpdateWeaponHUD` | 空实现，NPC 不更新玩家弹药界面。 |
| `GetWeaponTargetLocation` | 从摄像机瞄准当前目标，加入高度随机偏移和锥形散布，射线检测后返回实际瞄准点；无目标则沿自身视角。 |
| `AddWeaponClass` | 空实现，NPC 不维护玩家式拾枪背包。 |
| `OnWeaponActivated` | 空实现。 |
| `OnWeaponDeactivated` | 空实现。 |
| `OnSemiWeaponRefire` | 若仍处于射击状态，再次要求武器开火，让 NPC 能持续使用半自动。 |
| `Die` | 防重复死亡，加入死亡标签并广播，通知模板比分，停胶囊和移动，启用第三人称布娃娃，安排销毁。 |
| `DeferredDestruction` | 销毁 NPC。 |
| `StartShooting` | 保存目标、设置射击状态并要求武器开火。 |
| `StopShooting` | 清射击状态并停火。 |

### AI/ShooterNPCSpawner.h / ShooterNPCSpawner.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/AI/ShooterNPCSpawner.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/AI/ShooterNPCSpawner.cpp)。按“生一只，等它死亡，再生下一只”的方式运行。

| 函数 | 作用 |
|---|---|
| `AShooterNPCSpawner()` | 创建出生位置参考胶囊与方向箭头，关闭 Tick。 |
| `BeginPlay` | 检查模板 GameMode 是否允许敌人；数量大于零时安排首只 NPC。 |
| `EndPlay` | 清刷怪计时器。 |
| `SpawnNPC` | 在参考胶囊变换处生成配置 NPC，并监听它死亡。 |
| `OnNPCDied` | 剩余数量减一，仍有数量则延迟生成下一只。 |

### AI/EnvQueryContext_Target.h / EnvQueryContext_Target.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/AI/EnvQueryContext_Target.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/AI/EnvQueryContext_Target.cpp)。`UEnvQueryContext_Target::ProvideContext` 给 EQS 环境查询提供 AI 当前目标；目标无效时改用控制器自身作为上下文。

### AI/ShooterStateTreeUtility.h / ShooterStateTreeUtility.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/AI/ShooterStateTreeUtility.h)、[实现](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/AI/ShooterStateTreeUtility.cpp)。这里不是 Boss 的语言模型学习器，而是 UE 模板 StateTree 可调用的一组条件/任务。

每组 `...InstanceData`/`...Data` 结构保存该任务的输入、输出与运行状态：NPC、控制器、目标、朝向位置、随机数范围、感知标签、调查位置、感知强度和事件分发器。

| 所属结构 / 函数 | 作用 |
|---|---|
| `FStateTreeLineOfSightToTargetCondition()` | 默认构造，无额外初始化逻辑。 |
| `FStateTreeLineOfSightToTargetCondition::GetInstanceDataType` | 返回视线条件的数据结构类型，供状态树绑定字段。 |
| `FStateTreeLineOfSightToTargetCondition::TestCondition` | 检查目标有效、位于前方角度内，再对目标不同高度发射可见性射线；任一通畅算看到，并按“必须看到/必须看不到”的配置返回真假。 |
| `FStateTreeLineOfSightToTargetCondition::GetDescription` | 编辑器中显示视线条件名称。 |
| `FStateTreeFaceActorTask()` | 关闭 Tick，持续状态重新选择时不反复进入/退出。 |
| `FStateTreeFaceActorTask::GetInstanceDataType` | 返回面向 Actor 任务数据类型。 |
| `FStateTreeFaceActorTask::EnterState` | 将控制器焦点设为目标 Actor，返回运行中。 |
| `FStateTreeFaceActorTask::ExitState` | 清除 Gameplay 优先级焦点。 |
| `FStateTreeFaceActorTask::GetDescription` | 编辑器显示任务名称。 |
| `FStateTreeFaceLocationTask()` | 同样关闭 Tick 与重复重选状态变化。 |
| `FStateTreeFaceLocationTask::GetInstanceDataType` | 返回面向位置任务数据类型。 |
| `FStateTreeFaceLocationTask::EnterState` | 设置控制器注视的世界坐标。 |
| `FStateTreeFaceLocationTask::ExitState` | 清除 Gameplay 焦点。 |
| `FStateTreeFaceLocationTask::GetDescription` | 编辑器显示面向位置名称。 |
| `FStateTreeSetRandomFloatTask()` | 配置无需每帧更新、无需持续重选事件。 |
| `FStateTreeSetRandomFloatTask::GetInstanceDataType` | 返回随机数数据结构类型。 |
| `FStateTreeSetRandomFloatTask::EnterState` | 在上下限之间生成随机浮点数，写入输出字段。 |
| `FStateTreeSetRandomFloatTask::GetDescription` | 编辑器显示生成随机数名称。 |
| `FStateTreeShootAtTargetTask()` | 配置无需 Tick 和持续重选事件。 |
| `FStateTreeShootAtTargetTask::GetInstanceDataType` | 返回射击任务数据类型。 |
| `FStateTreeShootAtTargetTask::EnterState` | 让 NPC 开始向指定 Actor 射击。 |
| `FStateTreeShootAtTargetTask::ExitState` | 让 NPC 停火。 |
| `FStateTreeShootAtTargetTask::GetDescription` | 编辑器显示射击任务名称。 |
| `FStateTreeSenseEnemiesTask()` | 配置通过事件运行，无逐帧 Tick。 |
| `FStateTreeSenseEnemiesTask::GetInstanceDataType` | 返回感知任务数据类型。 |
| `FStateTreeSenseEnemiesTask::EnterState` | 绑定两个感知回调：直接发现敌人则设置目标并广播；只听到/感到刺激时检查视线，否则按强度选择调查点；遗忘时清目标、焦点并广播遗忘。回调使用弱状态树上下文，执行时重新取得有效实例数据。 |
| `FStateTreeSenseEnemiesTask::ExitState` | 解绑感知更新与遗忘回调。 |
| `FStateTreeSenseEnemiesTask::GetDescription` | 编辑器显示感知敌人名称。 |

### UI/ShooterUI.h / ShooterUI.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/UI/ShooterUI.h)、[实现单元](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/UI/ShooterUI.cpp)。`UShooterUI` 只有蓝图事件 `BP_UpdateScore(TeamByte, Score)`，让蓝图更新某队比分；`.cpp` 只有 include，没有手写函数。

### UI/ShooterBulletCounterUI.h / ShooterBulletCounterUI.cpp

源码：[声明](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/UI/ShooterBulletCounterUI.h)、[实现单元](../../SuzuneMatsuri/Source/SuzuneMatsuri/Variant_Shooter/UI/ShooterBulletCounterUI.cpp)。`UShooterBulletCounterUI` 有两个蓝图事件：`BP_UpdateBulletCounter` 更新弹药数量；`BP_Damaged` 更新生命比例及受伤表现。`.cpp` 没有具体函数实现。

## 6. 构建与项目创建脚本

这些脚本是开发时工具，打包后的游戏不会逐帧执行它们。这里只讲源码行为，没有运行生成、导入或收费任务。

### tools/build.sh

源码：[build.sh](../../tools/build.sh)。无自定义 Shell 函数，全部为顺序入口代码：定位工作区、UE5.8 自带 .NET 和 UBT，创建日志目录，编译 `SuzuneMatsuriEditor Win64 Development`，从日志抽取错误/警告和结果，原样返回构建退出码。默认使用固定本机引擎路径，需要 Git Bash 的 `cygpath`。

### tools/ue_cmd.sh

源码：[ue_cmd.sh](../../tools/ue_cmd.sh)。无具名函数：第一个参数作为日志名，其余传给 `UnrealEditor-Cmd`；附加无人值守、无启动图、无声音参数；记录日志并返回编辑器命令退出码。

### tools/ue_py.sh

源码：[ue_py.sh](../../tools/ue_py.sh)。无具名函数：把指定 Python 文件复制到无空格的临时路径，避免 UE `-script=` 对含空格路径的处理问题；调用 `ue_cmd.sh` 以 Python commandlet、NullRHI 执行；删除本次临时副本并保留退出码。

### tools/scaffold_project.py

源码：[scaffold_project.py](../../tools/scaffold_project.py)。把本机 UE5.8 第一人称模板复制并改名为新项目的初始化工具，不是当前游戏的启动脚本。

| 函数 | 作用 |
|---|---|
| `replace_template_name` | 将文本中的模板名按大小写形式替换为项目名。 |
| `copy_source` | 遍历模板 Source，替换路径和源码中的模块名，其他文件按原样复制。 |
| `copy_config` | 复制非模板专用配置并替换名称，向 DefaultEngine.ini 追加旧游戏名到新模块的重定向。 |
| `copy_content` | 复制模板 Content 及 Characters、Input、Weapons 等共享资源包；缺包时报错。 |
| `write_uproject` | 修改模板项目描述中的引擎版本、描述与模块名，写出新 `.uproject`。 |
| `main` | 解析项目名/目标目录，拒绝覆盖非空目标，依次调用上述步骤并返回退出码。 |
| `__main__` 入口 | 调用 `main()` 并把结果交给进程退出码。 |

## 7. UE 资源导入与材质制作脚本

### tools/ue/create_maps.py

源码：[create_maps.py](../../tools/ue/create_maps.py)。顶层列出标题、三层迷宫、Boss 五张地图，并逐一调用 `create_map`。地图是空壳，运行时由 GameMode 搭建内容。

| 函数 | 作用 |
|---|---|
| `create_map(name)` | 若资源存在则跳过；否则用 WorldFactory 创建指定路径地图并保存，失败抛异常。 |

### tools/ue/create_materials.py

源码：[create_materials.py](../../tools/ue/create_materials.py)。创建发光材质与世界坐标投影表面材质；当前代码不是三渲二 Shader。

| 函数 | 作用 |
|---|---|
| `create_glow` | 不存在时创建 `M_MatsuriGlow`：Color 接基础颜色，Color×Glow 接自发光，编译并保存。 |
| `expression` | 创建一个材质表达式节点，批量赋编辑器属性并返回节点。 |
| `create_surface` | 创建 `M_MatsuriSurface`：世界坐标除平铺尺寸，按 XYZ 三个方向采样贴图，再按法线权重混合、乘 Tint/亮度，设粗糙度并保存。 |
| `ensure_glow_usage` | 为发光材质开启实例化静态网格用途，避免 Boss 弹幕实例显示成默认灰材质。 |
| 顶层入口 | 顺序创建发光材质、修正实例网格用途、创建表面材质；已存在资源通常跳过创建。 |

### tools/ue/import_audio.py

源码：[import_audio.py](../../tools/ue/import_audio.py)。导入 `SourceArt/Audio/Sfx` 的 WAV 为 `/Game/Matsuri/Audio` SoundWave。

| 函数 | 作用 |
|---|---|
| `log` | 输出带音频导入前缀的 UE 日志。 |
| `main` | 收集 WAV，建立自动替换并保存的 AssetImportTask，批量导入，再逐个检查是否得到 SoundWave。 |
| 顶层入口 | 直接调用 `main()`，需在 UE Python 环境执行。 |

### tools/ue/import_tripo.py

源码：[import_tripo.py](../../tools/ue/import_tripo.py)。将处理后的 GLB、英雄动作、贴图、立绘导入固定 UE 路径。辅助材质、骨骼等留在 Parts 子目录供引用；视觉网格和玩法碰撞分离。`MATSURI_REIMPORT=1` 可要求替换资源。

| 函数 | 作用 |
|---|---|
| `log` | 输出带 Tripo 导入前缀的日志。 |
| `wanted` | 判断资源是否需要导入；已存在且未要求重导则返回假。 |
| `import_file` | 清理本资源的 staging 导入目录，运行自动导入，加载并返回得到的全部资源。 |
| `move` | 必要时删除目标旧资源，再把新资源重命名/移动到固定路径，失败抛异常。 |
| `import_prop` | 导入静态道具，选择网格、移到固定位置、关闭 Nanite、调整碰撞查询方式并保存尺寸日志。 |
| `apply_glow` | 对支持相应参数的导入材质实例，以基础色贴图作为发光贴图并设置发光颜色因子。 |
| `disable_nanite_everywhere` | 遍历已有静态道具，为旧导入结果补做关闭 Nanite。 |
| `import_hero_clip` | 导入英雄某个动作 GLB，移动骨骼网格与可用动画到统一命名位置。 |
| `import_textures` | 导入平铺 PNG，移到 Textures 并设置 sRGB。 |
| `import_portraits` | 导入立绘 PNG，设置 UI 贴图组和禁止流式加载。 |
| `main` | 依次导入立绘、表面贴图、道具，调整发光/Nanite，再导入英雄 idle/run/jump；部分资源失败记日志后继续。 |
| 顶层入口 | 直接调用 `main()`，需要 UE Python。 |

`import_file` 和 `move` 含删除/覆盖资源行为；它们属于制作工具能力说明，本次导读没有执行这些操作。

## 8. 模型、贴图与音效离线制作

### tools/blender/normalize_matsuri.py

源码：[normalize_matsuri.py](../../tools/blender/normalize_matsuri.py)。运行于 Blender，将生成道具合并、减面、修正朝向/尺寸/原点，再导出规范 GLB 与尺寸记录。

| 函数 | 作用 |
|---|---|
| `parse_args` | 读取 Blender `--` 后的资源规格、源目录、目标目录、可选名单。 |
| `import_as_single_mesh` | 清空 Blender 场景、导入 GLB、合并全部网格、应用变换、去除其他对象并统一命名。 |
| `bounds` | 从顶点坐标计算最小/最大边界。 |
| `decimate` | 超面数预算时使用 Decimate 修改器减少面数并应用。 |
| `align_long_axis` | 用顶点主轴估计箭等细长道具方向，识别较长尖端并朝 +X 对齐。 |
| `align_long_axis` 内 `thin_run` | 从某端分段测量横向宽度，用于判断哪端更像箭头。 |
| `align_bow` | 用长轴和把手凸出方向将弓长轴朝 +Z、把手凸出朝 +X，使手持视图可预测弓弦位置。 |
| `scale_and_pivot` | 按规格的米制尺寸缩放，原点放到底部或中心，返回最终尺寸。 |
| `export_glb` | 仅选中目标对象导出 GLB，并应用变换。 |
| `main` | 遍历规格与筛选项，导入→减面→朝向修正→缩放原点→导出；每个资源完成即写 `norm_meta.json`。 |
| 顶层入口 | 直接调用 `main()`，需要 Blender 的 `bpy`。 |

### tools/tripo/make_tileable.py

课程素材生产脚本 `make_tileable.py` 将生成的木材和石材图片制作成 1024 平铺贴图，供 UE 导入。

| 函数 | 作用 |
|---|---|
| `flatten_lighting` | 原图除以大范围模糊亮度并恢复平均色，减轻生成图片中烘焙的明暗条带。 |
| `mirror_tile` | 将半尺寸图以左右/上下镜像拼成四格，获得对边吻合的贴图。 |
| `make_tileable` | 把原图与半宽/半高偏移副本按窗口权重混合，改善接缝。 |
| `main` | 按 CROPS 裁图、缩放、去光照，再按 METHOD 选择镜像或混合并写入 SourceArt。当前 METHOD 中三种表面均选 blend。 |
| `__main__` 入口 | 执行 `main()`。 |

### tools/audio/build_sfx.py

源码：[build_sfx.py](../../tools/audio/build_sfx.py)。将 Kenney 包选定音频转 WAV，并用 NumPy 合成弓弦、铃铛、命中、UI、Boss 等音效；固定随机种子便于复现。最终写单声道 44.1kHz/16bit WAV 及音频来源说明。

| 函数 | 作用 |
|---|---|
| `find_ffmpeg` | 按环境变量、PATH、本机备用路径查找 ffmpeg，全部不存在则退出报错。 |
| `write_wav` | 按音效对应峰值归一化，转 16bit PCM 并写目标 WAV。 |
| `t` | 按秒数创建采样时间数组。 |
| `envelope` | 生成短起音、指数衰减的音量包络。 |
| `lowpass` | 一阶低通滤波，削弱高频。 |
| `pluck` | 用 Karplus–Strong 延迟反馈合成拨弦，并施加衰减包络。 |
| `noise_sweep` | 随时间改变截止频率的噪声扫频，制作挥动/呼啸。 |
| `resonate` | 双极点带通共振滤波，可随时间改变共振频率。 |
| `bow_draw` | 混合摩擦噪声、细碎点击和木质共振，模拟拉弓张力逐渐上升。 |
| `thud` | 混合低频主体、闷响和短纸声，制作柔和命中声。 |
| `bell` | 叠加多个非谐波分音及不同衰减，合成铃声。 |
| `mix` | 根据每段的起始秒数，把多个样本数组叠加到一个输出数组。 |
| `synthesise` | 调用上述合成器，返回“游戏音效名→样本数组”的完整合成音效表。 |
| `convert_kenney` | 从 ZIP 提取指定 OGG，在临时目录用 ffmpeg 转换并响度处理后写 WAV。 |
| `main` | 建输出目录，转换资源包音频、生成并写合成音效，再写 CREDITS.md。 |
| `__main__` 入口 | 调用 `main()`。 |

## 9. Tripo 在线生成工具

### tools/tripo/estimate.py

课程素材生产脚本 `estimate.py` 依据当时的固定单价估算资源预算。只读数据类 `Priced` 保存名字、档位、积分和计算说明。

| 函数 | 作用 |
|---|---|
| `price` | 按道具或英雄的概念图/模型/绑骨/动画数量，计算单个资源预算。 |
| `summarize` | 输出每项、每档与累计预算，并计入每位英雄一次重生成缓冲。 |
| `show_balance` | 从邻接项目 `P_game` 的密钥加载器创建客户端，查询账户余额并关闭客户端；是网络读操作。 |
| `main` | 读取计划 JSON，打印估价；未设置 `--no-balance` 时查询在线余额。 |
| `__main__` 入口 | 用 `main()` 返回值退出。 |

### tools/tripo/generate_matsuri.py

课程素材生产脚本 `generate_matsuri.py` 通过 Tripo SDK 生成道具、角色概念图、模型、骨骼和动作。它依赖本地素材生产工具和 Tripo 服务积分。

`Runner` 保存客户端、任务清单与本进程异步锁；任务 ID 在等待前写盘，便于中断后继续。该锁只协调同一个 Runner 的协程，不是跨进程文件锁，不能仅凭注释认定多个独立进程写清单完全安全。

| 函数 | 作用 |
|---|---|
| `load_manifest` | 读取已有 JSON 任务清单，没有则返回空字典。 |
| `save_manifest` | 将清单先写临时文件，再用 `os.replace` 替换正式文件。 |
| `Runner.__init__` | 保存客户端/清单，创建 asyncio 锁。 |
| `Runner.record` | 锁内重读磁盘清单，更新一项，过滤不能 JSON 序列化的 task 对象，再保存。 |
| `Runner.submit_with_backoff` | 提交任务；服务并发限制错误 `[2000]` 时渐增等待重试，其他/最终错误记到清单并返回空。 |
| `Runner.run` | 处理失败重试、已下载跳过、沿用已有任务 ID 或新提交、等待结果、记录积分和状态，返回含临时 task 对象的条目。 |
| `Runner.download_model` | 下载模型和可选预览到原始目录，复制为规范文件名，更新下载清单。 |
| `Runner.download_image` | 从任务输出选择图片 URL，下载到目标位置并记录路径。 |
| `model_args` | 合并默认值和资源可覆盖字段，过滤值为 None 的参数。 |
| `cmd_props` | 按档位/名字筛选 text3d 道具，通过信号量限制并发，提交并下载模型。 |
| `cmd_props` 内 `one` | 处理单个道具的限流、生成与下载。 |
| `hero_asset` | 按名字查找 hero 规格，找不到则终止并解释错误。 |
| `cmd_concepts` | 给角色描述加统一概念图风格，生成指定数量概念图。 |
| `cmd_concepts` 内 `one` | 生成并下载某个编号概念图。 |
| `cmd_hero` | 从选定图片生成模型→检查能否绑骨→按 Unreal 规格绑骨→逐个生成配置动画；某阶段失败则不继续下游。 |
| `cmd_textures` | 为内置的木材/石墙/铺路描述并发生成表面图片。 |
| `cmd_textures` 内 `one` | 生成并下载一种表面原图。 |
| `main` | 解析 props/concepts/hero/textures 子命令，读取规格，打开客户端，记录前后余额，分派对应生成任务。 |
| `__main__` 入口 | 用 `asyncio.run(main())` 执行异步入口并设置退出码。 |

本章覆盖此时 `tools/` 下 13 个源码脚本，以及 `Source` 中 Matsuri 目录之外的 49 个 `.h/.cpp/.cs` 文件。`tools/minimax` 当前只有说明与任务 JSON，没有独立源码脚本，因此不虚构视频生成函数。二进制 `.uasset/.umap`、导出的资源、日志和引擎生成代码不属于这里的手写函数范围。
