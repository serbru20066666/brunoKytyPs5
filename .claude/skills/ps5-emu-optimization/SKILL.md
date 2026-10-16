---
name: ps5-emu-optimization
description: Methodology-first playbook for making an HLE PS5 emulator (Kyty-style, Vulkan) run a real game fast, smooth and stable, and for specializing it to one title. Covers how to find and prove a bottleneck, measured wins and dead ends (general), Demon's Souls-specific facts, and how to find the same levers in another PS5 game. Use when asked to raise fps or 1% lows, remove hitches, cut RAM/VRAM, fix long-play slowdowns or crashes, judge an optimization idea, design an A/B, reverse-engineer a game's shaders/code/settings, or carry this work to a new title.
---

# PS5 emulator optimization: method first

Distilled from three weeks (2026-09-25 .. 10-15) and ~350 perf/fix commits taking Demon's Souls (PPSA01341) on KytyPS5
from ~21 fps to 60-105 fps standing (120 fps cap, i9-14900K + RTX 5090), from multi-second shader stalls to none,
and to flat memory over long play. Numbers are that project's; use them as orders of magnitude.
Source: branch `experiment/perf-40fps-20260926` of https://github.com/chenxiao07/KytyPS5 (`KYTY_*` switches,
scripts and file names below live there).

| Task | Read |
|---|---|
| How to measure, A/B, decide, test safely; the tools | `references/methodology.md` |
| What worked / failed in any game (check before proposing an idea) | `references/general.md` |
| Demon's Souls facts: shaders, patch sites, engine settings, passes, quirks | `references/demons-souls.md` |
| Specializing another PS5 title: what generalizes, how each lever was found | `references/other-games.md` |
| Guest code, shaders, patches, crash triage, recurring bug classes | `references/reverse-engineering.md` |

## The method (in order; skipping a step cost days every time)

1. **Set the target like the porting developer.** The console runs this at 60 fps: aim for the frame budget, not
   +10%. Name the structural cause; micro-tuning a structurally bound path ends flat.
2. **Find what bounds the frame before touching code.** Trace one frame flip to flip: render-thread busy vs idle,
   GPU busy, which queue/slice gates the next one, where the game waits. Only time on the critical path converts to
   frame time (a translation thread ~95% busy; GPU ~65% busy at the same spot means: CPU-side translation).
3. **Rank by per-call cost x calls per frame** (a census of scoped timers per phase/shader), then profile the
   critical thread with symbols of the exact build. Census numbers per call beat fps for small changes.
4. **Prove causality and the ceiling.** Inject a known delay at the site (frame-time slope ~1 = on the path, ~0 =
   saving it gains nothing). Make the unsafe "skip it" version and measure: a small ceiling ends the idea.
5. **Quantify the opportunity with a counter before building.** One cheap counter decided ideas in minutes (ring
   copies trimmable by static extents: 0.25% of bytes -> dropped; draws on the slow path: 0.14-0.47 ms -> not the
   lows' cause).
6. **Change only what you can prove removes work or waits, for the whole game.** No per-scene thresholds. Put it
   behind a live switch (a global you can poke) and add a verify mode when it replaces correctness work.
7. **A/B in one process, alternating** (poke the switch, A B A B, 4-6 rounds of 5 s at a fixed spot). Cross-boot
   variance is +-3-5% per world; read single sweeps as noise. Compare render ms / per-call census, not capped fps.
8. **Test short, then broad.** 1 boot, 2-4 rounds, 120 cap for a change; a screenshot for UI; then a multi-world
   soak (worlds x cycles) for stability and memory, and route tours for crashes before keeping risky changes.
9. **Keep or delete.** Not clearly faster -> remove the code (no default-off piles). Record every dead end with its
   numbers and reason, so nobody retries it without a new argument.
10. **Re-profile.** The bottleneck moves (translation-bound -> GPU tail -> game-side waits -> memory latency).

## What the profiler will not tell you

- **Overlapping cache misses.** Late per-draw costs are memory latency spread over many structures (pairs, sets,
  held entries, guest tables in 4 KiB pages). A lookup that shows 2% in the profile can be removed for 0 gain:
  four such changes measured flat (general.md). Confirm with census per call before and after.
- **Symbol misattribution.** Exports-only DLLs name samples after the nearest export: in vcruntime140
  `_NLG_Return2` = memmove's large-copy path, `memchr` = memcmp. Check with a disassembler before chasing.
- **Instrumentation cost.** Scoped timers inflate small phases (~10x); traces add ~10%; never quote instrumented fps.
- **Sampling the wrong build.** Profiles and crash addresses are valid only against that exact exe + map/PDB.

## Rules (the user's, and the ones that saved time)

- **Crashes first, then lows, then average fps.** Stability regressions are never traded for speed.
- **Never make users recompile.** No change to the SPIR-V of shaders already in users' caches unless the cache key is
  versioned; precompile speedups stay tool-side.
- **Logic-only optimization**; hit rate is not savings (count lookup + proof + invalidation + fallback).
- **Protect the player.** Benchmarks run only on a fixed baseline save, swapped in and restored with hash checks
  (restore by hand if a script is killed); one emulator at a time; never rebuild while it runs.
- **Diagnostics cost one relaxed load when off**; keep them out of commits (a separate uncommitted set).
- **Keep the console's timing.** Vblank 60 Hz by default; above it the game clock and movies need their own fixes.
- **Release only when asked; no upstream PRs without being asked.**
- **Short tests, more thinking.** Long sweeps for a 1% change waste hours; reason in code, test briefly.

## Where the time goes (model at 60-100 fps)

1. **The render (translation) thread bounds the frame** in heavy scenes: ~15 ms at Boletaria 1-1 standing, ~5.5K
   table draws at ~1.4 us and ~1.4K table dispatches at ~1.8 us. Heavy walking views have 13-17K draws -> 25-45 ms.
2. **Per-draw cost is memory latency**, not instructions: cold guest tables (4 KiB pages, TLB misses) and emulator
   lookup tables. It grows with distinct data per frame (walking views cost 2x per draw vs standing).
3. **Memory coherence tax**: page protection (every downgrade IPIs all cores running the process), ring copies of
   guest-written constants (~10 MB/frame), uploads, readbacks the game waits on.
4. **Stutter**: first-use translation/pipelines (precompile + recorded warmup), texture streaming bursts, guest
   partial unmaps (a Windows view cannot be split: unmapping it whole and mapping the rest again cost up to 29 ms a
   call on the game's threads; now parked in the view instead, general.md).
5. **Long play**: the game cuts its direct memory finer every level load (mappings 9K -> 50K over 24 loads); every
   per-mapping structure must be O(1) or epoch-cached, or later worlds get slower.

Directions that paid, ranked, and the dead ends: `references/general.md`.
