## New in this build

- **VSync and TVs (issue #30)**:
  - **No more slow motion:** the game no longer slows down when the display makes it wait. Frames are now shown on a
    thread of their own, and the game's clock (the PS5's vblank) never waits for them. V-Sync forced in the NVIDIA
    Control Panel, a TV below 60 Hz or a full VSync queue used to make the game run in slow motion.
  - **The launcher's VSync follows the display:** when the display refreshes at a whole multiple of 60 Hz, the
    emulator's vblank follows the display's own. That includes 59.94 Hz TVs, so the dropped or doubled frame every ~17 s
    is gone. Displays with a variable refresh rate keep the emulator's own timer.
  - **Steadier 60 fps:** frame pacing at 60 fps is steadier too. Standing in Boletaria 1-1 on the test PC, the 1% low
    went from 53 to 58-59 fps.
- **DLSS frame generation with "Up to 120 FPS" (issue #29):** with frame generation on, the game's own frames stay at
  60, so DLSS-G spaces its frames evenly. Before, they alternated between 8 and 17 ms.
- **The first visit to a place on a new install:**
  - Once the shader precompile has run, the background shader preparation starts with the shaders that the shipped
    recordings met in play, in the order they met them.
  - The first entry into Boletaria 1-1:

    | | Before | Now |
    |---|---|---|
    | First frame | 1.1 s | 0.4 s |
    | Slow frames | — | half as many |
    | Shaders translated while you wait | — | none |
- **Precompile on a new install (issue #28):** a PC without recorded play now compiles the shipped shader hints too.
  v20261016 compiled only the guessed shaders there, and AMD cards froze 0.2-8 s per pipeline on first visits. After
  updating, the launcher asks once to update the shader cache; only what is missing is compiled.
- **Walking back into a place:** the emulator sets shader data it has not used for a while aside to save memory. That
  data used to come back all at once when you returned, a 30-40 ms hitch each time.
  - PCs with 64 GB of RAM or more now keep it.
  - PCs with 16 and 32 GB keep the most recent and bring the rest back on other threads.
  - On the test PC, the longest frame walking back into Boletaria 1-1 went from 42 ms to 20 ms (24 ms with 16 GB). In
    Stonefang the 1% low went from 55 to 80 fps.
- **Graphics cards with less than 12 GB of video memory:**
  - Large textures are kept without their most detailed level. Textures are slightly softer up close, and play is
    smoother: with 8 GB simulated, the 1% low went from 37-49 to 53-58 fps.
  - The launcher's "Graphics" Low and Lowest now also make textures half or quarter size on any card.
  - `run.cmd -TextureReduce 0` keeps them whole.
- **Add-on content (DLC):** put it in `Content\PPSA01341\<folder>` (with its `sce_sys\param.json`), and the game sees
  it as owned (PR #25).
- **Shader hints:** the shipped shader hints add an RX 9070 XT player's recorded play.
- **Fixes:**
  - A crash at Latria: one of the game's threads dispatched an event on an empty object.
  - Frame generation: when the graphics driver refuses the device DLSS frame generation needs, the error now says so,
    and to start without frame generation (issue #32).
  - AMD driver 26.9.2 mosaics (issue #24): a test switch, `run.cmd -Set KYTY_PS_WAVE64=1`, runs pixel shaders 64 lanes
    wide where the driver allows it. The log says whether it took effect.

## 本版本的变化

- **垂直同步与电视（issue #30）**：
  - **不再慢动作：** 显示器让模拟器等待时，游戏不再变慢。现在画面在单独的线程上显示，游戏的时钟（PS5 的 vblank）不再等待显示。之前在 NVIDIA 控制面板里强制垂直同步、电视刷新率低于 60 Hz，或垂直同步队列已满时，游戏会变成慢动作。
  - **启动器的 VSync 跟随显示器：** 显示器刷新率是 60 Hz 的整数倍时，模拟器的 vblank 会跟随显示器自己的 vblank。59.94 Hz 的电视也算在内，所以不再每隔约 17 秒丢一帧或重复一帧。可变刷新率显示器仍使用模拟器自己的计时器。
  - **60 fps 更稳：** 60 fps 下帧间隔也更均匀了。在测试电脑上站在波雷塔利亚 1-1，1% low 从 53 提高到 58-59 fps。
- **DLSS 帧生成与"最高 120 FPS"（issue #29）：** 开启帧生成时，游戏自己的帧保持在 60 fps，DLSS-G 生成的帧因此间隔均匀。之前帧间隔在 8 ms 和 17 ms 之间交替。
- **新装后第一次进入某个区域：**
  - 运行过着色器预编译后，后台的着色器准备会先处理随包附带的游玩记录里用到的着色器，并按记录里遇到的顺序进行。
  - 第一次进入波雷塔利亚 1-1：

    | | 之前 | 现在 |
    |---|---|---|
    | 第一帧 | 1.1 秒 | 0.4 秒 |
    | 卡顿帧 | — | 减少一半 |
    | 等待时现场翻译的着色器 | — | 没有 |
- **新装环境的预编译（issue #28）：** 没有游玩录制的电脑现在也会编译随包附带的着色器提示。v20261016 在这种情况下只编译了推测出来的着色器，AMD 显卡第一次进入各区域时每条管线会卡 0.2-8 秒。更新后，启动器会询问一次是否更新着色器缓存，只编译缺少的部分。
- **走回之前去过的地方：** 模拟器会把一段时间没用到的着色器数据暂时移出内存来节省内存。之前回到那个地方时，这些数据会一次性全部载回，每次都会卡 30-40 ms。
  - 64 GB 及以上内存的电脑现在会一直保留这些数据。
  - 16 GB 和 32 GB 的电脑保留最近用过的部分，其余的在其他线程上载回。
  - 在测试电脑上走回波雷塔利亚 1-1 时，最长的帧从 42 ms 降到 20 ms（16 GB 时为 24 ms）。石牙隧道的 1% low 从 55 提高到 80 fps。
- **显存小于 12 GB 的显卡：**
  - 大纹理不再保留最精细的一级。近看时纹理稍微模糊一点，但运行更流畅：模拟 8 GB 显存时，1% low 从 37-49 提高到 53-58 fps。
  - 启动器"Graphics"的 Low 和 Lowest 现在在任何显卡上都会把纹理缩小到一半或四分之一。
  - 用 `run.cmd -TextureReduce 0` 可以保留完整纹理。
- **追加内容（DLC）：** 放在 `Content\PPSA01341\<文件夹>` 里（带上它的 `sce_sys\param.json`），游戏就会把它当作已拥有（PR #25）。
- **着色器提示：** 随包附带的着色器提示加入了一位 RX 9070 XT 玩家的游玩记录。
- **修正：**
  - 拉特利亚之塔的一个崩溃：游戏的一个线程对空对象分发了事件。
  - 帧生成：显卡驱动拒绝创建 DLSS 帧生成所需的设备时，错误信息现在会说明原因，并提示关闭帧生成后再启动（issue #32）。
  - AMD 26.9.2 驱动下的马赛克（issue #24）：新增测试开关 `run.cmd -Set KYTY_PS_WAVE64=1`，在驱动允许时让像素着色器按 64 宽运行，日志会说明是否生效。
