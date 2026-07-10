# AER Core

[![CI](https://github.com/aer-works/aer-core/actions/workflows/ci.yml/badge.svg)](https://github.com/aer-works/aer-core/actions/workflows/ci.yml)

Cross-language process execution and lifecycle supervision runtime with deterministic cleanup semantics.

---

## The problem

Standard process APIs (`subprocess`, `.NET Process`, `std::process`) cannot reliably manage process lifecycles under failure conditions. When a spawned process creates children of its own, those children frequently outlive the parent after cancellation or timeout — leaving orphan processes that hold ports, lock files, and burn CPU with no owner.

## What AER does

AER is a Rust core library that guarantees consistent process lifecycle behavior across platforms:

- **Deterministic events** — every execution emits `Started` then `Exited`, in that order, always
- **No silent failures** — spawn errors are typed and explicit; no swallowed results
- **Platform-agnostic contract** — Windows and Linux behave identically from the caller's perspective
- **No orphans** — process tree cleanup ensures nothing survives `run()` returning: kernel-enforced on Windows (Job Objects), best-effort on Unix (setsid/killpg — a descendant that starts its own session can escape; see spec §6)

---

## Quickstart

Requires [pixi](https://pixi.sh). The Rust toolchain is managed automatically — nothing else to install.

```sh
# M1 — basic lifecycle: spawn a process and observe events
pixi run example

# M2 — timeout: see a slow process get killed, then a fast process complete normally
pixi run example-timeout

# M3 — process tree: a process forks a background child; AER cleans up the whole tree
pixi run example-tree

# M4 — IO capture and explicit cancellation examples
pixi run example-capture
pixi run example-cancel

# Run all tests
pixi run test

# Lint and format check
pixi run lint
pixi run fmt-check
```

### Example output

```
Spawning task...

  → Started  (pid 12345)
  → Exited   (code 0)

Done.
```

---

## Architecture

```
┌─────────────────────────────────┐
│           aer-core              │  ← this repo, Milestones 1–4
│                                 │
│  Task::run()                    │
│    │                            │
│    ├── StateMachine             │  Created → Running → Exited
│    ├── Event emission           │  Started { pid }, Exited { code }
│    └── OS process layer         │  windows.rs / unix.rs
└─────────────────────────────────┘
         ↑ FFI boundary (M4)
┌────────┴────────┐  ┌────────────┐
│bindings/dotnet/ │  │ aer-python │  ← thin translation layers, M5/M6
│   (P/Invoke)    │  │ (ctypes)   │
└─────────────────┘  └────────────┘
```

Dependencies flow inward only. No process logic lives in the bindings.

---

## Roadmap

| Milestone | Status | Adds |
|---|---|---|
| **M1: Core Scaffold** | ✅ Complete | State machine, STARTED/EXITED events, single-shot execution |
| **M2: Timeout & Kill** | ✅ Complete | Configurable timeout, graceful termination, kill escalation |
| **M3: Process Tree** | ✅ Complete | Job Objects (Windows), setsid (Unix) — no orphans (hard guarantee on Windows, best-effort on Unix; spec §6) |
| **M4: FFI Boundary** | ✅ Complete | Stable C-compatible ABI for language bindings |
| **M5: .NET Binding** | Planned | P/Invoke wrapper, `IAsyncEnumerable<Event>` |
| **M6: Python Binding** | Deferred | ctypes/cffi wrapper, asyncio context manager |

Full behavioral specification: [`spec/aer-core-behavioral-spec-v1.1.md`](spec/aer-core-behavioral-spec-v1.1.md)

Project board: [AER Roadmap](https://github.com/orgs/aer-works/projects/1)

---

## Available tasks

| Command | Description |
|---|---|
| `pixi run build` | Compile the workspace |
| `pixi run test` | Run all tests |
| `pixi run lint` | Clippy with `-D warnings` |
| `pixi run fmt` | Auto-fix formatting |
| `pixi run fmt-check` | Check formatting (used in CI) |
| `pixi run example` | Run the M1 hello example |
| `pixi run example-timeout` | Run the M2 timeout example |
| `pixi run example-tree` | Run the M3 process tree example |
| `pixi run example-capture` | Run the M4 stdout/stderr capture example |
| `pixi run example-cancel` | Run the M4 manual cancellation example |

---

## License

[Unlicense](LICENSE) — public domain.
