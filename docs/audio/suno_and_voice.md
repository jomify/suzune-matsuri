# 鈴音の夜祭り：Suno 配乐提示词 + 配音台本

> 游戏目前**没有任何背景音乐**（`Content/Matsuri/Audio` 里只有 SFX_*），所以下面每个场景各需一首。
> 文件放好后告诉我，我来接进游戏（按场景切换、Boss 转阶段切曲、淡入淡出）。

---

## 一、Suno 使用方法

- 用 **Custom** 模式。器乐曲打开 **Instrumental**，把下面的「Style」整段贴进 *Style of Music*，「Exclude」贴进 *Exclude styles*。
- 器乐曲的 *Lyrics* 框里**只放结构标签**（例如 `[Intro] [Main Theme] [Loop]`），能让曲子段落更清楚，方便剪成循环。
- 每首生成 2–3 个版本，挑旋律最好记的那个。
- **做循环**：时长选 2–3 分钟。在 Audacity 里从一个乐句开头剪到下一个相同乐句的开头，两端各加 10 ms 淡入淡出。导出为 `48 kHz / 16-bit WAV`。
- 不要在 Style 里写真实艺人、游戏或作品的名字（Suno 会拒绝）。下面全部用风格描述代替。

| 文件名 | 用在哪里 | 速度 | 长度 |
|---|---|---|---|
| `BGM_Title` | 主菜单 / 角色选择 | 84 | 循环 |
| `BGM_1F_Bamboo` | 1F 竹林の参道 | 92 | 循环 |
| `BGM_2F_Yatai` | 2F 夏祭りの夜店街 | 122 | 循环 |
| `BGM_3F_Torii` | 3F 千本鳥居 | 104 | 循环 |
| `BGM_Boss_P1` | 玉藻 第一阶段 | 168 | 循环 |
| `BGM_Boss_P2` | 玉藻 最终阶段（转阶段时切换） | 176 | 循环 |
| `BGM_Story` | 剧情对白 / 加载 | 70 | 循环 |
| `BGM_Ending` | 结局「愿火重燃」（带人声） | 88 | 完整一首 |
| `STING_Clear` / `STING_Defeat` | 过关 / 失败短乐句 | – | 4–6 秒 |

---

## 二、各曲提示词

### 1. BGM_Title：主题曲「鈴音」

**Style**
```
Japanese traditional fusion, warm and nostalgic, autumn shrine festival at golden hour, shinobue bamboo flute lead melody, koto arpeggios, shamisen plucks, soft taiko and small hand bells (suzu), gentle string pad, light piano, memorable main theme, cinematic game menu music, 84 BPM, D major pentatonic, loopable, no vocals
```
**Exclude**
```
vocals, EDM drops, dubstep, heavy distortion, trap hi-hats
```
**Lyrics 框**
```
[Intro: bells and koto]
[Main Theme: shinobue melody]
[Bridge: strings swell]
[Main Theme reprise]
[Loop point]
```

### 2. BGM_1F_Bamboo：1F 竹林の参道（探索、学操作）

**Style**
```
calm Japanese ambient exploration music, golden afternoon in a bamboo grove on a mountain shrine path, breathy shakuhachi melody, koto ostinato, soft shamisen, wind chimes, light woodblock and frame drum groove, warm low strings, curious and peaceful but quietly mysterious, video game exploration loop, 92 BPM, A minor pentatonic, no vocals
```
**Exclude**
```
vocals, epic choir, rock drums, synth bass
```
**Lyrics 框**
```
[Intro: wind and chimes]
[Verse: shakuhachi melody over koto]
[Groove: woodblock enters]
[Verse 2: shamisen answers]
[Loop point]
```

### 3. BGM_2F_Yatai：2F 夏祭りの夜店街（摊位街，压力上来）

**Style**
```
energetic Japanese matsuri bayashi fusion, festival street with food stalls at sunset, driving taiko ensemble and chappa cymbals, bright shinobue lead, fast shamisen riffs, kakegoe shouts (wordless 'sore! yoisho!'), slap bass and funky drum kit underneath, playful but tense chase energy, action game stage music, 122 BPM, E minor, loopable
```
**Exclude**
```
sung lyrics, ballad, slow tempo, EDM drop
```
**Lyrics 框**
```
[Intro: taiko roll]
[Main Riff: shamisen and flute unison]
[Chant: wordless kakegoe "sore! yoisho!"]
[Chase Section: drums double time]
[Main Riff reprise]
[Loop point]
```

### 4. BGM_3F_Torii：3F 千本鳥居（日落、神秘、接近真相）

**Style**
```
mysterious and tense Japanese dark fantasy music, endless vermilion torii gates at sunset fading to dusk, hypnotic koto ostinato, low taiko heartbeat, eerie ryuteki and shinobue harmonies, sul ponticello strings, soft female wordless choir, shimmering bells, building suspense toward a sacred inner shrine, video game stage music, 104 BPM, D minor, loopable
```
**Exclude**
```
happy, major key pop, rock guitar, EDM
```
**Lyrics 框**
```
[Intro: low taiko heartbeat]
[Ostinato: koto pattern]
[Build: strings and wordless choir]
[Climax: full ensemble]
[Calm down: bells]
[Loop point]
```

### 5. BGM_Boss_P1：九尾狐・玉藻（弹幕 Boss 第一阶段）

**Style**
```
fast melodic bullet hell boss battle theme, Japanese style danmaku shooter, catchy virtuosic lead melody on synth trumpet and piano doubling a shamisen, rapid koto runs, driving rock drums with double kick, slap bass, orchestral strings stabs, taiko hits on downbeats, majestic yet playful nine-tailed fox guardian, intense and heroic, 168 BPM, F# minor, loopable, no vocals
```
**Exclude**
```
vocals, slow intro, ambient, lo-fi, dubstep wobble
```
**Lyrics 框**
```
[Intro: taiko and strings stab]
[Theme A: lead melody]
[Theme B: shamisen solo]
[Theme A: key change up]
[Loop point]
```

### 6. BGM_Boss_P2：玉藻 最终阶段（转阶段后）

**Style**
```
climactic final phase bullet hell boss music, same nine-tailed fox theme but more desperate and grand, full orchestra with choir chanting, distorted electric guitar doubling shamisen, blast of taiko ensemble, soaring synth lead, relentless 176 BPM, half-time breakdown then explosive return, epic Japanese fantasy finale, loopable, no lyrics
```
**Exclude**
```
sung lyrics, calm, ambient, lo-fi
```
**Lyrics 框**
```
[Intro: choir and taiko]
[Theme: guitar and shamisen unison]
[Breakdown: half time]
[Final Theme: everything]
[Loop point]
```

> 想让两段像同一首曲子：先生成 P1，再在 Suno 里对 P1 用 **Extend / Cover**，把上面 P2 的 Style 贴进去。

### 7. BGM_Story：剧情对白 / 加载画面

**Style**
```
soft emotional Japanese ambient underscore for story dialogue, sparse koto and piano, warm string pad, distant shrine bells, gentle and reflective, leaves falling at dusk, very quiet so voice acting stays clear, 70 BPM, G major, loopable, no vocals
```
**Exclude**
```
drums, vocals, loud dynamics
```

### 8. BGM_Ending：结局曲「愿火」（日语人声）

**Style**
```
emotional Japanese anime ending ballad, clear gentle female vocal, piano and koto intro, shinobue countermelody, warm strings, taiko enters at the final chorus, hopeful and bittersweet, autumn festival lanterns relighting, 88 BPM, C major, J-pop ballad with traditional instruments
```
**Exclude**
```
rap, screaming, EDM, autotune heavy
```
**Lyrics 框**（原创歌词，可以直接用）
```
[Intro]

[Verse 1]
消えかけた 提灯の灯り
迷路の奥で 願いが泣いてた
ひとつずつ 拾い集めて
帰る道を 探していたの

[Pre-Chorus]
ひとりで守る 夜はもう
終わりにしよう

[Chorus]
鈴の音が 響くなら
もう一度 祭りを灯そう
願い火よ 空へ還れ
君と見上げる 月の下

[Verse 2]
狐の影 結界の向こう
冷たい雨も 優しさだった
銀杏の葉が 舞い落ちる
約束だけ ここに残して

[Pre-Chorus]
来年もまた 同じ場所で
会えるように

[Chorus]
鈴の音が 響くなら
もう一度 祭りを灯そう
願い火よ 空へ還れ
君と見上げる 月の下

[Bridge]
守ることは ひとりじゃない
手をつなげば 灯りになる

[Final Chorus]
鈴の音が 響く夜に
何度でも 祭りを灯そう
願い火よ 空へ還れ
みんなで歌う 月の下

[Outro: bells fade]
```

### 9. 短乐句（Sting）

- **STING_Clear**：`short triumphant Japanese festival fanfare, taiko roll, shinobue flourish, bright bell hit, 5 seconds, ends cleanly`
- **STING_Defeat**：`short sad Japanese sting, single koto pluck descending, low taiko, temple bell decay, 5 seconds`

（Suno 很难直接出 5 秒的曲子：生成一首完整的，再剪出开头或结尾的乐句。）

---

## 三、配音台本

**录音规格**：48 kHz、16-bit、单声道 WAV，峰值 −3 dB，前后各留 0.2 秒静音，每句一个文件。
**文件名**：按下面的 ID 命名，例如 `VO_F1_Narrator.wav`。

### 剧情对白（和游戏内文字一一对应）

| ID | 角色 | 台词 | 语气 |
|---|---|---|---|
| VO_F1_Narrator | 祭りの声（旁白） | 灯笼一盏接一盏熄灭，原本献给村子的愿望正在迷宫里流失。 | 低沉、缓慢，像讲古老传说 |
| VO_F1_Sakura | 春日 さくら | 愿钱不会自己消失。我们把它们带回去。 | 坚定、温柔 |
| VO_F1_Suzune | 影山 すずね | 是谁偷走了愿望？边走边查清楚吧。 | 冷静、干脆 |
| VO_F1_Mio | 猫宮 みお | 愿望掉得到处都是……捡回来就能开宴会了吧？ | 俏皮、贪吃 |
| VO_F1_Bell | 神社の铃 | 还有三座鸟居亮着微光。沿着愿钱的踪迹走，夜祭或许还能醒来。 | 空灵、带一点回声 |
| VO_F2_Kasa | 唐傘 | 我们不是来追你的，是破碎的愿望在每条走廊里呼唤我们。 | 痛苦、恍惚的妖怪 |
| VO_F2_Sakura | 春日 さくら | 妖怪也在受苦。不能只靠射击结束这一切。 | 同情 |
| VO_F2_Suzune | 影山 すずね | 敌人的动作不对劲。有人把它们困在这里。 | 警觉 |
| VO_F2_Mio | 猫宮 みお | 既然它们追过来，就先给大家找一条退路吧。 | 机灵、稍认真 |
| VO_F2_FoxSign | 狐之印 | 狐印指向内殿，碑文写着：把村民留在结界之外。 | 平静的念碑文 |
| VO_F3_Record | 古老记录 | 愿望变成毒物后，玉藻封锁了本殿。正是这道封印保护了村子。 | 苍老、庄重 |
| VO_F3_Sakura | 春日 さくら | 原来玉藻一直在守护大家？那我们去听她解释吧。 | 恍然、释然 |
| VO_F3_Suzune | 影山 すずね | 既然她有封锁这里的理由，我就亲自看到最后。 | 认真 |
| VO_F3_Mio | 猫宮 みお | 既然是在保护大家，就该早点说嘛。我肚子也饿了！ | 抱怨、可爱 |
| VO_F3_Memory | 夜祭的记忆 | 最后的鸟居只会为愿意把愿望送回去、而不是据为己有的人打开。 | 空灵、轻声 |
| VO_Boss_Tamamo1 | 九尾狐 玉藻 | 我封锁本殿，是为了保护村民。若你能不被愿望吞噬，把它们送回原处，我就会打开通往夜祭的路。 | 威严、高贵，带一点疲惫 |
| VO_Boss_Sakura | 春日 さくら | 我接受试炼。愿望和村子，我会一起守住。 | 郑重 |
| VO_Boss_Suzune | 影山 すずね | 无论是弹幕还是对话，我都会走到你面前。 | 冷静的挑衅 |
| VO_Boss_Mio | 猫宮 みお | 打赢之后，就一起把夜祭办回来吧！ | 元气 |
| VO_Boss_Tamamo2 | 玉藻 | 那么就让月亮见证吧。这些弹幕考验的是守护之心，不是惩罚。 | 微笑、开战宣言 |
| VO_End_Tamamo | 玉藻 | 愿望终于安静下来。我守住了村民，却无法独自让夜祭重新点燃。 | 温柔、释然 |
| VO_End_Sakura | 春日 さくら | 愿火由大家一起守护。明年，我们再来。 | 温暖 |
| VO_End_Suzune | 影山 すずね | 结界不该由一个人独自支撑。下次，我们一起守护。 | 难得的柔和 |
| VO_End_Mio | 猫宮 みお | 愿望回来了，那摊位也会回来吧？说好了！ | 开心 |
| VO_End_Narrator | 祭りの声 | 鸟居再次亮起，夜祭重新开始。这一次，守护它的不再只有一个人。 | 收尾，温暖 |

每关只会播放**当前所选角色**的那句，所以三位主角各录自己的 5 句就够了。

### 战斗喊招（日语，短促，可选）

| ID | 角色 | 喊什么（读音） |
|---|---|---|
| VO_Tamamo_Ring | 玉藻 | 狐火・環（きつねび・わ） |
| VO_Tamamo_Spiral | 玉藻 | 狐火・螺旋（きつねび・らせん） |
| VO_Tamamo_Pursuit | 玉藻 | 鬼火・追（おにび・おい） |
| VO_Tamamo_TailSweep | 玉藻 | 九尾薙ぎ（きゅうびなぎ） |
| VO_Tamamo_FoxRain | 玉藻 | 狐の雨（きつねのあめ） |
| VO_Tamamo_Phantom | 玉藻 | 幻影・瞬身（げんえい・しゅんしん） |
| VO_Tamamo_Phase | 玉藻 | 「ふふ……ここからが本番よ。」 |
| VO_Tamamo_Defeat | 玉藻 | 「見事……その心、確かに受け取ったわ。」 |
| VO_Sakura_Shift / E / Q | さくら | 結界（けっかい）／お祓い（おはらい）／大祓（おおはらえ） |
| VO_Suzune_Shift / E / Q | すずね | 瞬身（しゅんしん）／分身（ぶんしん）／影縫い（かげぬい） |
| VO_Mio_Shift / E / Q | みお | 招き猫（まねきねこ）／投げ銭（なげせん）／千客万来（せんきゃくばんらい） |

### 通用短句（每位主角各录，可选）

| ID 后缀 | 场合 | さくら | すずね | みお |
|---|---|---|---|---|
| _Hurt | 受伤 | 「っ……！」 | 「くっ。」 | 「にゃっ！？」 |
| _LowAmmo | 弹药不足 | 「矢が足りない……」 | 「手裏剣、残り少ない。」 | 「鈴がもうないにゃ！」 |
| _Clear | 过关 | 「次の鳥居へ。」 | 「片付いた。」 | 「大漁だにゃ～！」 |
| _Down | 倒下 | 「ごめん……みんな……」 | 「……ここまで、か。」 | 「お腹すいた……」 |

> 剧情对白用中文还是日语都可以，取决于你想要的感觉；喊招和短句建议保持日语，听起来更有和风。
