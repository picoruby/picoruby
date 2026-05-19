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
- `2>`, `2>>`, `2>&1` may appear on **any** segment. Because `$stderr` is a single global swapped for the whole pipeline, the effect is pipeline-wide (not per-process as in bash); if more than one segment specifies a stderr redirect the last one wins.
- Misplaced `<`/`>`/`>>` emit a warning and are ignored.

## Concepts

Each command segment is a `Sandbox` — a separate `mruby_task` running on the cooperative scheduler. Inter-segment data flow uses `Task::Queue`. The shell main task acts as the pipeline executor: it spawns each segment with `Sandbox#load_file(.., join: false)`, waits for state transitions, and closes queues at the boundaries.

Per-task method override is implemented with **task-scoped refinements** (`mruby-task-refinements`, integrated into `mruby-task`).
Each pipeline segment activates two refinements at startup:

- `PipeOUT` refines `Kernel#puts` / `Kernel#print` to push lines into the segment's outbox `Task::Queue`.
- `PipeIN` refines `Kernel#gets` and `Kernel#getc` to read from the inbox queue. `getc` keeps a per-consumer leftover buffer so a single queue item (a full line or print chunk) can be drained one character at a time.

When a segment has no outbox (it is the last segment) or no inbox (it is the first), the refinement falls through to the real `$stdout` / `$stdin`. The shell uses this fall-through to implement redirects: it swaps `$stdin`, `$stdout`, and `$stderr` globally for the duration of the pipeline, and those swaps are visible to every sandbox task because globals live in the shared `mrb_state`.

Stderr redirection follows the same global-swap trick. Errors flow through a unified entry point:

```ruby
Shell.report_exception(e) # => "<message> (<Class>)" to $stderr
```

All sites that previously printed exception messages (parser errors, Job lifecycle, builtin dispatch, pipeline executor, shell helper helpers) call this method so the user sees a consistent format. When `2>` is active, the helper writes into the redirected file because `$stderr` is already swapped at the moment the rescue fires.

## Backpressure / SIGPIPE equivalent

When a consumer finishes before its producer (e.g. `tail -f | head -1`), the pipeline executor closes the producer's outbox queue. The producer's next `puts` raises `Task::Error("queue closed")`, which propagates out of the script and ends the producer task cleanly. There is no special swallow logic in `PipelineIO`; the exception is the signal.

## Limitations

- **FemtoRuby (mruby/c)**:
  - Pipelines (`|`) are rejected with a `RuntimeError` (`pipe require task-scoped refinements, which are not supported on FemtoRuby`). `Module#refine` is not available on mruby/c and is not planned. `pipe_refinements.rb` skips its `refine` body on mruby/c so the gem still loads cleanly.
  - `Task::Queue` is not available on mruby/c either, but a separate implementation is planned. Once it lands, only the refinement constraint will remain for pipelines.
  - Single commands, redirects (`<`, `>`, `>>`, `2>`, `2>>`, `2>&1`), background (`&`), and the unified error reporter still work to the extent that `$stdin/$stdout/$stderr` are wired up in the mruby/c IO layer; some scripts may degrade silently if the underlying global is not provided.
- **Single-segment cases** are still routed through the original Job/Sandbox path; the refinement layer is only injected for two-or-more segments.
- **`|&`** (pipe stdout+stderr) and **`&>`** (combined stdout/stderr file redirect) are not implemented. Use `2>&1` together with `>` instead.
- **`2>&1 > out.txt`** behaves like `> out.txt 2>&1` in this shell (both go to the file). In bash the first form would leave stderr on the original terminal. The simplification comes from applying redirects in a fixed order (stdin -> stdout -> stderr) inside `with_io_redirects`.
- **Background jobs (`&`)** are supported only for a plain external command without redirects. Pipelines, builtins, and redirected commands print a warning and run in the foreground. There is no `[N]+ Done <cmd>` notification yet; use `jobs` to inspect state.
- **Builtins in pipelines** are rejected. Builtins (`cd`, `echo`, `pwd`, `jobs`, `bg`, `fg`, `export`, ...) run synchronously in the shell main task and cannot be sandboxed into a pipeline segment, so `echo hello | wc` prints `shell: builtin 'echo' cannot be used as a pipeline segment` and aborts. Use a real executable (`cat`, `ls`, ...) as the producer instead.
- **Ctrl-C during a pipeline on POSIX** does not always return to the prompt.
  `Machine.check_signal` is wired to SIGINT, and the host's terminal is started with `ISIG` disabled, so the typed `^C` arrives as a byte 3 in stdin instead of a signal. The embedded (RP2040) target uses a USB-CDC callback path that routes byte 3 to `sigint_status` and works correctly.
- **`each_line_from_files_or_stdin`** (used by `cat` etc.) now raises `RuntimeError` on the first missing file, so subsequent file arguments in the same invocation are not processed. This mirrors the unified error format and is a deliberate trade-off against bash's continue-on-error behaviour for `cat a missing b`.
- **`Shell::ARGV`** is provided per sandbox task via `Sandbox#argv=`. It is reset for each new `Job`, so consecutive command invocations do not leak arguments. Scripts that internally chain via `load` share `Shell::ARGV` within the same task and must manage it themselves (`r2p2.rb`, `wifi_connect.rb` use `Shell::ARGV.clear` for this reason).

## Key files

- `mrblib/shell.rb` — dispatch (`run_shell`, `execute_command_node`, `execute_pipeline_node`, `with_io_redirects`, `Shell.report_exception`)
- `mrblib/job.rb` — single-segment lifecycle (`exec`, `exec_async`, `wait`, `terminate`, `report_error`)
- `mrblib/pipeline.rb` — pipeline executor (spawn, wait, Ctrl-C cleanup)
- `mrblib/pipeline_io.rb` — outbox / inbox registry, push/pop routing
- `mrblib/pipe_refinements.rb` — `PipeOUT` / `PipeIN`, `PIPELINE_USING`
- `mrblib/parser.rb` — tokenizer + parser for the supported syntax
- `mrblib/shell_helpers.rb` — `each_line_from_files_or_stdin`
