---
name: pico-debug
description: >-
  Start an on-device GDB debugging session for r2p2 PicoRuby/FemtoRuby firmware
  on a real Raspberry Pi Pico (RP2040 / RP2350) over SWD, driving openocd and
  gdb inside tmux panes. Use when the user wants to debug on real hardware
  (jikki / on-device), e.g. "pico2 wo debug build de debug shite",
  "gdb de jikki debug", or mentions openocd / gdb-pico / SWD together with a
  board name (pico, pico_w, pico2, pico2_w).
---

# pico-debug: on-device GDB debugging for r2p2 firmware

Bring up a full SWD debug session on real Pico hardware: launch openocd for the
right chip in a background tmux pane, build the requested r2p2 firmware, and
attach gdb to it in a user-visible tmux pane.

## Board / runtime / chip mapping

The rake target is `r2p2:<runtime>:<board>:<variant>`.

| board    | chip   | openocd alias    | runtime that supports it |
|----------|--------|------------------|--------------------------|
| pico     | RP2040 | `openocd-rp2040` | femtoruby only           |
| pico_w   | RP2040 | `openocd-rp2040` | femtoruby only           |
| pico2    | RP2350 | `openocd-rp2350` | picoruby (default) or femtoruby |
| pico2_w  | RP2350 | `openocd-rp2350` | picoruby (default) or femtoruby |

Rules:
- Chip and openocd alias are fully inferable from the board name. Never ask.
- `runtime` defaults to **picoruby** for pico2/pico2_w. `pico`/`pico_w` force
  **femtoruby** (picoruby has no RP2040 target). If the user explicitly says
  "femtoruby", honor it.
- `variant` defaults to **debug**. If the user asks for **prod**, confirm once
  before building (prod is optimized: limited stepping and variable inspection).
- Build output dir: `build/r2p2/<runtime>/<board>/<variant>/`, and the ELF is
  the newest `*.elf` there (dated + hashed filename).

## Environment constraints (MUST read before any step)

1. **tmux socket is blocked by the sandbox.** Every `tmux` command
   (`list-panes`, `send-keys`, `capture-pane`, `split-window`) fails with
   "Operation not permitted" unless run with `dangerouslyDisableSandbox: true`.
   The build (`rake ...`) and file reads run fine inside the sandbox.
2. **The debug commands are shell aliases / functions**, defined in
   `~/.bash_aliases`:
   - `openocd-rp2040`, `openocd-rp2350` -> aliases
   - `gdb-pico` -> bash function (`gdb-multiarch -ex "target extended-remote
     localhost:3333" $*`)
   Aliases/functions expand ONLY in an interactive login shell, so they are
   NOT callable from this agent's non-interactive shell. Always run them by
   `tmux send-keys` into an interactive bash pane. Never call them directly.
3. **Identify your own pane** via `$TMUX_PANE` (e.g. `%3`) so you never send
   keystrokes to yourself. Get it with a normal (sandboxed) `echo $TMUX_PANE`.
4. **Address panes by stable pane id** (`%1`, `%4`, ...), not by index. Indexes
   shift; ids are stable within a session. Resolve a user-stated "pane 1" to its
   id via `list-panes` before sending keys.
5. **Detecting running processes**: `#{pane_current_command}` shows the live
   command, e.g. `openocd`, `gdb-multiarch`, `claude`, `bash`, `nnn`, `vim`.
   A "free" pane for openocd must have `cmd=bash` (a pane running `nnn`/`vim`/an
   editor is NOT free).

## Parameters to gather

- **board** (required): pico | pico_w | pico2 | pico2_w.
- **variant** (default debug): debug | prod. If prod, confirm once.
- **runtime**: inferred (picoruby for pico2*, femtoruby for pico*); overridable.
- **gdb pane** (required, user-specified): the visible pane where gdb attaches.
  If the user did not say which pane, list candidate panes and ask.

The openocd pane is chosen automatically (see step 2). Do not ask for it.

## Procedure

All `tmux` calls below require `dangerouslyDisableSandbox: true`.

### 0. Resolve panes

```bash
echo "self=$TMUX_PANE"
tmux list-panes -a -F '#{session_name}:#{window_index}.#{pane_index} id=#{pane_id} active=#{pane_active} cmd=#{pane_current_command}'
```

- Map the user's stated gdb pane (e.g. "pane 1") to its pane id. Confirm it is
  not `$TMUX_PANE` and not already running gdb/openocd.
- Note the window that gdb pane lives in.

### 1. Ensure openocd is running for the correct chip

Scan the pane list for `cmd=openocd`.
- If one is already running: capture it and verify it targets the requested chip
  (`rp2350.cm0` / `rp2040`), and that it prints
  `Listening on port 3333 for gdb connections`. If it matches, reuse it and skip
  to step 2. If it targets the wrong chip, ask the user to stop it (a second
  openocd will fail: ports 3333/4444/6666 already in use).
- If none is running: continue to step 2's launch below.

### 2. Launch openocd in a free background pane

Pick the openocd pane: prefer a pane in the SAME window as the gdb pane with
`cmd=bash`, whose id is neither the gdb pane nor `$TMUX_PANE`. If none in that
window, use any idle `cmd=bash` pane. If there is genuinely no free bash pane,
create one: `tmux split-window -v -t <gdb-window>` then re-list to get its id.

```bash
tmux send-keys -t <openocd_pane_id> 'openocd-rp2350' Enter   # or openocd-rp2040
sleep 2
tmux capture-pane -p -t <openocd_pane_id>
```

Verify the capture contains BOTH:
- `Examination succeed` (the chip was detected over SWD), and
- `Listening on port 3333 for gdb connections`.

If instead you see no CMSIS-DAP device / "unable to find" / open errors, the
probe or board is not connected or not in debug mode: report and stop.

### 3. Build the firmware (sandboxed, no override needed)

```bash
rake r2p2:<runtime>:<board>:<variant> 2>&1 | tail -40
```

Confirm it ends with `Built target ...`. On failure, surface the error and stop.

### 4. Resolve the ELF (absolute path)

```bash
ls -t build/r2p2/<runtime>/<board>/<variant>/*.elf | head -1
```

Prepend `/home/hasumi/work/picoruby/` if the path is relative; the gdb pane's
cwd may differ, so always pass gdb an absolute path.

### 5. Attach gdb in the user-visible pane

```bash
tmux send-keys -t <gdb_pane_id> "gdb-pico <ABS_ELF_PATH>" Enter
sleep 6
tmux capture-pane -p -t <gdb_pane_id> | tail -30
```

Verify the capture shows `Remote debugging using localhost:3333` and a `(gdb)`
prompt. The target is now halted; the user drives it from that pane
(`bt`, `c`, `b <fn>`, `si`, ...).

### 6. Report

Give a compact status table: openocd pane + chip, build target + result, gdb
pane + attach status, and the ELF path. Tell the user gdb is interactive in
their pane.

## Troubleshooting

- **"Operation not permitted" on any tmux call**: you forgot
  `dangerouslyDisableSandbox: true`.
- **openocd exits immediately / "Address already in use"**: a previous openocd
  still holds 3333/4444/6666. Reuse it (step 1) or stop the old one.
- **`gdb-pico: command not found` in the pane**: that pane's shell did not load
  `~/.bash_aliases` (not a login/interactive shell). Use a normal interactive
  bash pane, or source it first.
- **gdb says "Connection refused"**: openocd is not up yet or died; recheck
  step 2's capture.
- **prod build, values optimized out / can't step**: expected. Offer to rebuild
  as debug.

## Cleanup (only when the user asks to end the session)

- In the gdb pane: send `quit` then `y`.
- In the openocd pane: send `C-c` (Ctrl-C) to stop openocd.
Leave the panes themselves as they were (do not kill user panes you did not
create; a pane you created via split-window may be closed with
`tmux kill-pane -t <id>`).
