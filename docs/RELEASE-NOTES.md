## New in this build

- **Load Game lets you choose the character (issue #46):**
  - Load Game shows the list of your saves. Each entry has the character's name, level, place, play time and date.
  - Choose with Cross / Enter, go back with Circle / Escape. Before, it always loaded the character played last.
  - Saves from an older version show their folder name and date until the game saves them once.
- **AMD graphics cards:**
  - **The precompiled shader cache now works on AMD (PR #41, issue #28).** The driver files its pipelines under the
    program's name, so the game never found what the precompile had made. **After updating, run the precompile once
    more**: the launcher asks, and the old cache is no longer used.
  - **Phantoms and see-through bodies** no longer turn into blocks or mosaics (PR #41, issue #24, driver 26.9.2). The
    emulator now tells the driver which color targets a shader writes nothing to.
  - **RX 6700 XT / 6750 XT (RDNA2):** fixes the crash at start-up inside AMD's shader compiler (issue #43). The crash
    came from the emulator's own texture-conversion shaders, which are now built like the game's shaders. Not yet
    confirmed on an RDNA2 card: if it still crashes, please send the log. Its last "Tiler pipeline" line names the
    shader.
- **Graphics cards with 8 GB of video memory:**
  - Buffers the game stopped using are let go after 30 s (10 s when video memory runs short) instead of 1800 frames.
    On an RTX 3080 Laptop (8 GB) touring Boletaria 1-1, the frame rate went from 15-17 to 23-24 fps, and frames over
    100 ms from 137 to 21-25.
  - Pipelines unused for 3 minutes leave video memory when it runs short. They come back from the shader cache when
    needed, about 1 ms each.
- **Graphics cards with 12 GB or more (PR #44):** game buffers that ended up in system memory move back into video
  memory once there is room. The texture collector follows the video memory Windows gives now; another program, a
  Discord stream for example, can take some of it.
- **Less memory:**
  - The game's start-up memory is no longer cleared by hand. A 16 GB PC has 0.7 GB more free RAM, and start-up is
    0.26 s shorter.
  - The background shader preparation reuses its big buffers. On a 16 GB PC, frames over 50 ms while it runs fell
    from 30-56 to 11-16 in 30 s.
- **Frame generation:** GPUs such as the RTX 5080 start with DLSS frame generation again instead of stopping at "Could
  not create the Vulkan device…" (issue #32). The emulator now leaves the driver the address space frame generation
  needs, so the `KYTY_RESERVE_CEILING` workaround is no longer needed. `run.cmd -FrameGen` also uses the Streamline
  folder chosen in the launcher (issue #38).
- **Fixes:**
  - With the game's Mono Mix Mode on (Settings → Audio), a high-pitched tone looped over the sound (issue #40).
  - A rare crash in Boletaria 1-1 ("buffer ADD_TID is only valid for compute shaders").
  - When Windows refuses to write a save (antivirus, Controlled folder access, a read-only folder, a full disk), the
    log now names the file and Windows' reason (issue #31).
- **Diagnostics:**
  - `run.cmd -Set KYTY_WRITEBACK_VERIFY=1` reports GPU write-backs that would overwrite memory the game changed
    (issue #31). It is off by default and costs about 10% fps.
  - The end of the log says how many pipelines came from the precompiled cache.
- **Building from source:** Visual Studio 2026's C++ library needs Clang 20 or newer (PR #39).

## 本版本的变化

- **"读取游戏"可以选择角色（issue #46）：**
  - "读取游戏"会列出所有存档，每一项显示角色名、等级、所在地点、游戏时间和日期。
  - 用 ×（或 Enter）选择，○（或 Esc）返回。之前它总是读取最后玩的那个角色。
  - 旧版本留下的存档，在游戏再保存一次之前，只显示文件夹名和日期。
- **AMD 显卡：**
  - **预编译的着色器缓存现在在 AMD 上也能用了（PR #41，issue #28）。** AMD 驱动按程序名存放管线，所以游戏一直找不到预编译生成的结果。**更新后请再预编译一次**：启动器会提示，旧缓存不再使用。
  - **幽灵和半透明身体**不再变成方块或马赛克（PR #41，issue #24，26.9.2 驱动）。模拟器现在会告诉驱动，着色器不写哪些颜色目标。
  - **RX 6700 XT / 6750 XT（RDNA2）：** 修复启动时在 AMD 着色器编译器里崩溃的问题（issue #43）。崩溃来自模拟器自己的贴图转换着色器，现在它和游戏着色器用同样的方式构建。尚未在 RDNA2 显卡上确认：如果仍然崩溃，请发日志，日志最后一行 "Tiler pipeline" 会写明是哪个着色器。
- **8 GB 显存的显卡：**
  - 游戏不再使用的缓冲区，过 30 秒就释放（显存紧张时 10 秒），之前要等 1800 帧。在 RTX 3080 Laptop（8 GB）上巡游波雷塔利亚 1-1，帧率从 15-17 提高到 23-24 fps，超过 100 ms 的帧从 137 个减到 21-25 个。
  - 显存紧张时，3 分钟没用过的管线会移出显存，需要时再从着色器缓存里取回，每条约 1 ms。
- **12 GB 及以上显存的显卡（PR #44）：** 被放进系统内存的游戏缓冲区，等显存有空间时会移回显存。纹理回收会跟随 Windows 当前给的显存额度；其他程序（比如 Discord 推流）会占掉一部分。
- **更少的内存：**
  - 游戏启动时的内存不再由模拟器逐页清零。16 GB 内存的电脑多出 0.7 GB 空闲内存，启动快 0.26 秒。
  - 后台准备着色器时会重复使用大块缓冲区。在 16 GB 内存的电脑上，后台准备期间 30 秒内超过 50 ms 的帧从 30-56 个减到 11-16 个。
- **帧生成：** RTX 5080 等显卡开启 DLSS 帧生成后可以正常启动，不再停在 "Could not create the Vulkan device…"（issue #32）。模拟器现在会给驱动留出帧生成所需的地址空间，不再需要 `KYTY_RESERVE_CEILING`。`run.cmd -FrameGen` 也会使用启动器里选的 Streamline 文件夹（issue #38）。
- **修正：**
  - 开启游戏的单声道混音（Mono Mix Mode，设置 → 音频）时，声音里会循环一个高音（issue #40）。
  - 波雷塔利亚 1-1 的一个罕见崩溃（"buffer ADD_TID is only valid for compute shaders"）。
  - Windows 拒绝写入存档时（杀毒软件、受控文件夹访问、只读文件夹、磁盘已满），日志会写明文件名和 Windows 给出的原因（issue #31）。
- **诊断：**
  - `run.cmd -Set KYTY_WRITEBACK_VERIFY=1` 会报告 GPU 回写将覆盖游戏已修改内存的情况（issue #31）。默认关闭，开着时 fps 约低 10%。
  - 日志结尾会写明有多少条管线来自预编译缓存。
- **从源码构建：** Visual Studio 2026 的 C++ 库需要 Clang 20 或更新版本（PR #39）。
