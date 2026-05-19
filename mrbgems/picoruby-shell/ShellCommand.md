# R2P2 Shell Commands

R2P2's interactive shell parses a small subset of POSIX shell syntax and runs each command in an isolated mruby task. This document summarises the supported syntax, the implementation strategy, and the current limitations.

## Supported syntax

| Form                              | Example                                |
|-----------------------------------|----------------------------------------|
| Single command                    | `ls -l /sd`                            |
| Pipeline                          | `cat README.md \| head -3`             |
| Input redirect                    | `head -3 < in.txt`                     |
| Output redirect (truncate)        | `ls > out.txt`                         |
| Output redirect (append)          | `ls >> out.txt`                        |
| Stderr redirect                   | `cmd 2> err.txt` / `cmd 2>> err.txt`   |
| Merge stderr into stdout          | `cmd > out.txt 2>&1`                   |
| Redirect inside pipeline          | `cat < in \| head -3 > out.txt 2> err` |
| Background single command         | `sleep 30 &`                           |

Redirect placement rules in a pipeline:

- `<` is honoured only on the **first** segment (pipeline stdin).
- `>`, `>>` are honoured only on the **last** segment (pipeline stdout).
- `2>`, `2>>`, `2>&1` may appear on **any** segment. The stderr target is seeded once into the shell task's `$stderr` and every segment inherits it via its fork snapshot, so the effect is pipeline-wide (not per-process as in bash); if more than one segment specifies a stderr redirect the last one wins.
- Misplaced `<`/`>`/`>>` emit a warning and are ignored.

## Concepts

Each command segment is a `Sandbox` -- a separate mruby task running on the cooperative scheduler. Per-task IO isolation is implemented with **`Task#fork`** (a per-task forked global variable namespace provided by `mruby-task`). There are no refinements and no method overrides; an earlier version used task-scoped refinements (`mruby-task-refinements`) and that approach has been fully replaced by `Task#fork`.

`src_task.fork { block }` creates a task whose global variable table is a shallow snapshot of `src_task`'s at spawn time. Rebinding a global inside the block (e.g. `$stdout = pipe`) is isolated to that task, while the underlying objects stay shared. `Sandbox.new(name, stdout:, stdin:, stderr:)` uses this: when any of `stdout:`/`stdin:`/`stderr:` is given, the C `_init` (`picoruby-sandbox/src/mruby/sandbox.c`) forks the task's globals from the current task and writes the given objects into the fork's private `$stdout` / `$stdin` / `$stderr` before the task first runs. Plain `puts` / `print` / `gets` / `getc` in the loaded script then resolve to those objects automatically, because the script's globals *are* the fork's table.

Inter-segment data flow uses **`Task::Pipe`** (`picoruby-task-ext/mrblib/pipe.rb`), a thin IO-shaped wrapper around `Task::Queue` (`puts` / `print` / `write` / `gets` / `getc` / `flush` / `close` / `closed?`). The pipeline executor (`Shell::Pipeline`) creates one `Task::Pipe` per inter-stage link, spawns each segment with `Job#exec_async` (`Sandbox#load_file(.., join: false)`) in reverse order, and wires segment `i`'s stdout pipe to segment `i+1`'s stdin pipe. `Task::Pipe#fileno` returns `-1` so the `getc` shim in `shell_helpers.rb` routes terminal reads to the real `$stdin` and pipe reads to the queue.

Redirects are applied in two different ways depending on who runs the command:

- **Single external command with file redirect** (`execute_with_file_redirect`): the shell opens the files and passes them as `stdout:`/`stdin:`/`stderr:` to the `Sandbox`. The redirect lives entirely in the task's forked globals; the shell main task's own `$stdout` / `$stdin` / `$stderr` are untouched.
- **Builtins with redirect, and the file endpoints of a redirected pipeline** (`with_io_redirects`): the shell swaps its own `$stdin` / `$stdout` / `$stderr` for the duration and restores them in an `ensure` block. Builtins run in the shell main task and cannot be forked, so a real global swap is the only way to redirect them. For a redirected pipeline the same swap acts as the **seed**: each segment's `Task#fork` snapshots the shell task's globals, so the first segment inherits the redirected `$stdin` and the last inherits the redirected `$stdout`, while the inter-stage `Task::Pipe` objects override `$stdout` / `$stdin` where present.

Errors flow through a unified entry point:

```ruby
Shell.report_exception(e) # => $stderr.puts "<message> (<Class>)"
```

All sites that print exception messages (parser errors, Job lifecycle, builtin dispatch, pipeline executor, shell helpers) call this method so the user sees a consistent format. A forked task's error is captured as `Sandbox#error` and re-reported by the shell main task (`Job#report_error` -> `Shell.report_exception`), so the message lands on whatever `$stderr` the shell main task currently has -- the redirected file while inside `with_io_redirects`, the terminal otherwise.

## Backpressure / SIGPIPE equivalent

When a consumer finishes before its producer (e.g. `tail -f | head -1`), the pipeline executor closes the producer's outbox `Task::Pipe`. The producer's next `puts` / `write` pushes onto a closed `Task::Queue`, which raises `Task::Error("queue closed")`. That exception unwinds the producer, its task goes DORMANT, and the pipeline executor proceeds. There is no special swallow logic in the producer; the exception is the signal. `Job#report_error(ignore_closed_queue: true)` suppresses this expected error for every non-final segment.

## Limitations

- **FemtoRuby (mruby/c)**:
  - Pipelines (`|`) are rejected with the message ``pipe (`|`) is not supported on FemtoRuby``. They require `Task#fork` (per-task forked globals) to isolate `$stdout` / `$stdin` per segment, which mruby/c does not provide.
  - Single commands, redirects (`<`, `>`, `>>`, `2>`, `2>>`, `2>&1`), background (`&`), and the unified error reporter work to the extent that `$stdin` / `$stdout` / `$stderr` are wired up in the mruby/c IO layer; some scripts may degrade silently if the underlying global is not provided.
- **Single-segment cases** are routed through the original Job/Sandbox path; the per-segment pipe wiring is only used for two-or-more segments.
- **`|&`** (pipe stdout+stderr) and **`&>`** (combined stdout/stderr file redirect) are not implemented. Use `2>&1` together with `>` instead.
- **`2>&1 > out.txt`** behaves like `> out.txt 2>&1` in this shell (both go to the file). In bash the first form would leave stderr on the original terminal. The simplification comes from applying redirects in a fixed order (stdin -> stdout -> stderr) inside `with_io_redirects`.
- **Background jobs (`&`)** are supported only for a plain external command without redirects. Pipelines, builtins, and redirected commands print a warning and run in the foreground. There is no `[N]+ Done <cmd>` notification yet; use `jobs` to inspect state.
- **Builtins in pipelines** are rejected. Builtins (`cd`, `echo`, `pwd`, `jobs`, `bg`, `fg`, `export`, ...) run synchronously in the shell main task and cannot be sandboxed into a pipeline segment, so `echo hello | wc` prints `shell: builtin 'echo' cannot be used as a pipeline segment` and aborts. Use a real executable (`cat`, `ls`, ...) as the producer instead.
- **Ctrl-C during a pipeline on POSIX** does not always return to the prompt. `Machine.check_signal` is wired to SIGINT, and the host's terminal is started with `ISIG` disabled, so the typed `^C` arrives as a byte 3 in stdin instead of a signal. The embedded (RP2040) target uses a USB-CDC callback path that routes byte 3 to `sigint_status` and works correctly.
- **`each_line_from_files_or_stdin`** (used by `cat` etc.) raises `RuntimeError` on the first missing file, so subsequent file arguments in the same invocation are not processed. This mirrors the unified error format and is a deliberate trade-off against bash's continue-on-error behaviour for `cat a missing b`.
- **Arguments (`$*`)** are exposed to each executable as the per-task global `$*` (Ruby's `ARGV` alias). `Sandbox#argv=` forks the task's global namespace (if not already forked) and writes `$*` into it, so each command sees its own arguments and concurrent tasks (e.g. `cmd &`) do not clobber each other. On mruby/c there is no `Task#fork`, so `$*` is a single shared global; commands run sequentially, so each overwrites it before running. Scripts that internally chain via `load` share `$*` within the same task and must manage it themselves (`r2p2.rb`, `wifi_connect.rb` use `$*.clear` for this reason).

## Key files

- `mrblib/shell.rb` -- dispatch (`run_shell`, `execute_command_node`, `execute_pipeline_node`, `classify_redirects`, `with_io_redirects`, `execute_with_file_redirect`, `execute_builtin_with_redirect`, `Shell.report_exception`)
- `mrblib/job.rb` -- single/per-segment lifecycle (`exec`, `exec_async`, `wait`, `terminate`, `report_error`); builds the `Sandbox` with `stdout:`/`stdin:`/`stderr:`
- `mrblib/pipeline.rb` -- pipeline executor (`Task::Pipe` per link, async spawn, state polling, pipe close for SIGPIPE, Ctrl-C cleanup)
- `mrblib/parser.rb` -- tokenizer + parser for the supported syntax
- `mrblib/shell_helpers.rb` -- `getc` shim, `each_line_from_files_or_stdin`
- `mrblib/kernel.rb` -- `Kernel#system`

Cross-gem pieces the shell relies on:

- `picoruby-task-ext/mrblib/pipe.rb` -- `Task::Pipe`, the IO-shaped wrapper over `Task::Queue`
- `picoruby-sandbox/mrblib/sandbox.rb`, `picoruby-sandbox/src/mruby/sandbox.c` -- `Sandbox.new(stdout:, stdin:, stderr:)` and the `Task#fork` of the task globals
- `mruby-task` -- `Task#fork`, `Task::Queue`, and the cooperative scheduler
