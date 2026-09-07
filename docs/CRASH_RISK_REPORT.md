# CRASH RISK REPORT — Sky's Fallout Cheats

**Date:** 2026-09-07  
**Pass:** Forensic stability (MainGameLoop + deferred console) — `stability-pass-v2`  
**Status:** Risks reduced; **not declared stable** until playtest confirms freezes stop.

---

## What changed (this pass)

| Before | After |
|--------|--------|
| `TickBackground` (companions, feature ticks, teleports) ran inside **D3D9 EndScene** | Runs on **NVSE `kMessage_MainGameLoop`** via `App::TickGameWorld` |
| `GameState::Tick` ran inside ImGui `OnFrame` (EndScene) | Runs on MainGameLoop only when world settled + not blocking UI |
| `ConsoleBridge::Run` called `RunScriptLine2` immediately (often from UI/EndScene) | **Queues** outside game loop; **drains** inside MainGameLoop |
| Companion actor-list scan on INSERT click / teleport arm from EndScene | Snapshot / bring **deferred** to MainGameLoop |
| Weak crash forensics | `[DIAG] seq=…` logs (config `diagnostics.enabled`, default ON) |
| Unclear which DLL was loaded | Boot logs DLL path + file mtime |

**Not disabled:** live HUD, companions, teleports, INSERT menu.

**Fallback:** If Messaging API fails to register, game-world tick still runs at the **start** of EndScene (before ImGui) — logged as `[GAMEWORK] MainGameLoop UNAVAILABLE`.

---

## Risk ratings (remaining)

### CRITICAL
| Risk | Why it remains | Mitigation / next |
|------|----------------|-------------------|
| Manual memory / vtable reads in `GameState` | Wrong offset or mid-stream object → AV in `FalloutNV.exe` (WER may not blame our DLL). `__try/__except` does not make bad native calls safe. | Reads only on MainGameLoop + settle gate. Still need offset audit / NVSE-safe APIs where possible. |
| `RunScriptLine2` (`coc`, `moveto`, forceav) | Mutates world; delayed AV/hangs possible even from game loop. | Deferred off EndScene; still engine-side risk. Prefer longer settle before post-TP `moveto`. |

### HIGH
| Risk | Why | Next |
|------|-----|------|
| Exterior streaming + soft `Invalidate` | HUD stays up while cells stream; next read may hit half-loaded refs. | Consider pausing **weapon/process** reads during exterior stream, keep HP/caps if proven safer. |
| Companion `moveto` after `coc` | Known delayed instability vector in Gamebryo trainers. | Keep delay + stable frames; A/B log: last ops before crash. |
| `CanDrawOverlay` light reads from EndScene | Player/cell pointer checks every EndScene (needed for draw gate). | Acceptable short-term; optional cache updated from game loop. |

### MEDIUM
| Risk | Why | Next |
|------|-----|------|
| D3D9 Reset / EndScene race | Device lost during settle; mitigated with `g_rendering` spin + StateBlock. | Keep; verify Alt-Tab / resolution change. |
| ClipCursor while menu open | Can feel like freeze if focus/clip stuck. | Already release on kill-focus; watch multi-monitor. |
| Feature `TickAll` still broad | Any feature that adds heavy work runs every game loop. | Keep ESP off; audit new features. |
| Diagnostic log I/O | Flush-every-line can hitch if disk slow. | Throttled; turn `diagnostics.enabled` false after investigation. |

### LOW
| Risk | Why |
|------|-----|
| Hotkey → queued console (1-frame delay) | Intentional; feel is fine. |
| Stale DLL deploy | Boot mtime log; still kill FNV before ENABLE_SFC.bat. |
| ImGui draw AV | SEH skip frame; rare if StateBlock OK. |

---

## How to verify this build

1. Close FNV completely.
2. Rebuild Win32 Release + `tools\ENABLE_SFC.bat`.
3. Launch via **NVSE**.
4. Open `Data\NVSE\Plugins\SkysFalloutCheats\sfc.log` — expect:
   - `[BOOT] build=… stability-pass-v2`
   - `[BOOT] dll=… mtime=…`
   - `[NVSE] Messaging registered — MainGameLoop owns game-world work`
   - `[GAMEWORK] MainGameLoop ACTIVE`
5. Play: run exterior, combat, town TP, companions, INSERT menu.
6. On freeze/crash: copy the **last ~80 lines** of `sfc.log` (especially `[DIAG]` / `[TELEPORT]` / `[COMPANIONS]` / `[GAMEWORK]`).

---

## Why we do **not** declare “stable” yet

Major crash *paths* were addressed (game mutations + heavy reads off EndScene; deferred companion scan; diagnostics).  
We have **not** proven in-session that the user’s “freeze while running” AV (`0xc0000005` @ `0x006a55a6`) is gone. That needs a playtest + log tail.

---

## Suggested next phases (after this playtest)

1. **Stable runtime** — this pass  
2. **Reliable GameState** — safer AV reads / less combat-time weapon poking  
3. Target system / more cheats  
4. UI polish / packaging  

Paste this report + a crash `sfc.log` tail back to ChatGPT/Cursor for the next cut.
