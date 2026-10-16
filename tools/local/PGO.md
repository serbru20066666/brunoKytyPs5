# 本地 PGO

当前正式构建：`_Build/agent30fps-20260917/build/`，Clang 18 / Release / ThinLTO。
其 profile 输入是 `_Build/pgo/walk8-20260915-v5/kyty.profdata`，不是可随测试日志删除的临时文件。
实际编译/链接参数以该构建的 Ninja 命令和 CMakeCache 为准。

重新训练时使用独立目录，保持训练和使用时的源码一致：

1. 用 `-fprofile-instr-generate` 构建训练版本；若正常退出走 `std::quick_exit`，训练版本额外链接 `pgo-quick-exit.cpp`，确保 profile 写出。
2. 设置 `LLVM_PROFILE_FILE` 到新的输出目录，正常运行代表性场景并退出。训练版本 FPS 不是收益结果。
3. 用 `llvm-profdata-18 merge` 合并确认属于本次成功运行的非空 profraw。
4. 在独立构建中使用 `-fprofile-instr-use=/绝对路径/kyty.profdata`，记录源码和二进制指纹，按 [同进程测量方法](../../docs/BENCHMARKING.md) 验证。

不要在 FPS 测量期间编译或训练，不要覆盖正在运行或正式交付的二进制。mimalloc 是独立宿主分配器选择，不能把更换分配器与 PGO 收益混为一谈。

## Windows

`build-windows.cmd` 自动带上 `-DKYTY_PGO_USE`：有 `_Build\pgo\windows\kyty.profdata` 就用它，否则用仓库里的
`tools\pgo\kyty.profdata`（GitHub 上的发布构建用这个；重新训练后把新 profile 也复制过去提交）。源码改动多了之后，
profile 里改过的函数会因哈希不符而失去 PGO，需要重新训练：

1. `python tools\local\bench-windows.py prepare`，然后 `tools\local\windows\pgo-train.ps1`：
   它在 `_Build\windows-pgogen` 构建插桩版本（`-DKYTY_PGO_GENERATE=ON`），用它跑训练场景
   （基准路线走过去站立、转镜头、走进遗迹、横移），用 live 命令 `pgo <file>` 写出 profile，
   再用 `llvm-profdata merge` 得到 `_Build\pgo\windows\kyty-<日期>.profdata`。
2. 把旧的 `kyty.profdata` 另存一份，新文件复制成 `kyty.profdata`；路径不变时 Ninja 不会重编，
   所以先 `cmake --build _Build\windows --target clean` 再 `build-windows.cmd`。
3. 用 `ab-exe.ps1` 对比旧 profile 的可执行文件（先复制到自己的目录）和新构建。

2026-10-01：9 月 27 日的 profile 换成新训练的之后，固定场景 43.83 → 45.17 fps
（ab-exe 三轮，B 组每个窗口都高于 A 组）。

2026-10-16（笔记本，16 GB 内存）：10-08 的 profile 已落后 232 个提交，在当前 HEAD 上重新训练三个场景后合并：
- 默认基准存档（序章；它有两个角色，出生点会在隧道和雾中平原之间变化，对比前先看截图）；
- `KYTY_BENCH_BASELINE=_Build\bench-baselines\user-20261001`（1-1 城门广场）；
- `world-latria`。

1-1 站立，ab-exe 两轮 ABBA（需要 `-StartupSeconds 240`：16 GB 下 1-1 要约 180 s 才到 HUD）：
渲染线程 33.60 → 32.00 ms/帧（-4.8%），fps 22.87 → 23.12（站立时帧还在等游戏侧）。
清理构建可以用 `ninja -C _Build\windows -t clean` 代替 `cmake --build ... --target clean`。
