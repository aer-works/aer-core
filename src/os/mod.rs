use crate::AerError;
use std::process::Child;
use std::sync::mpsc;
use std::time::Duration;

/// Lightweight handle that the timeout monitor thread uses to kill the process tree.
///
/// Windows: wraps an `Arc<JobHandle>` so the OS handle stays alive until all
/// threads are done with it, preventing handle-value recycling races.
/// Unix: wraps the pgid (== pid after setsid); no heap allocation needed.
#[derive(Clone)]
pub(crate) struct KillHandle {
    #[cfg(windows)]
    pub(crate) job: std::sync::Arc<windows::JobHandle>,
    #[cfg(not(windows))]
    pub(crate) pgid: u32,
}

/// Handle to a live OS process.
pub(crate) struct OsHandle {
    pub(crate) pid: u32,
    pub(crate) child: Child,
    pub(crate) kill: KillHandle,
}

/// A single captured chunk, tagged with the stream it came from. Sent over one
/// shared channel so chunks are delivered to the caller in true arrival order
/// while per-stream `seq` ordering (each drain thread's sends are FIFO on its
/// own sender clone) is preserved automatically.
pub(crate) enum ChunkMsg {
    Stdout(u64, Vec<u8>),
    Stderr(u64, Vec<u8>),
}

/// Senders for captured stdout/stderr chunks — clones of the same channel,
/// distinguished by which `ChunkMsg` variant each drain thread sends.
/// `None` means the stream should be silently drained.
pub(crate) struct OutputSinks {
    pub(crate) stdout: Option<mpsc::Sender<ChunkMsg>>,
    pub(crate) stderr: Option<mpsc::Sender<ChunkMsg>>,
}

/// Platform abstraction for spawning, waiting on, and killing a process.
/// Implementations must not leak OS-specific behavior into callers.
pub(crate) trait OsProcess {
    fn spawn(program: &str, args: &[&str]) -> Result<OsHandle, AerError>;
    /// Blocks until the process exits. Returns the exit code.
    /// Returns -1 if the OS provides no exit code (e.g. signal-killed on Unix).
    /// When `sinks` contains `Some` senders, drain threads forward captured bytes
    /// to them; `None` sinks are silently discarded.
    fn wait(handle: OsHandle, sinks: OutputSinks) -> Result<i32, AerError>;
    /// Kills the entire process tree. On Unix: SIGTERM → sleep(grace) → SIGKILL
    /// to the process group. On Windows: TerminateJobObject immediately.
    fn kill_escalating(kill: KillHandle, grace: Duration) -> Result<(), AerError>;
    /// Probes whether any process in the tree is still alive. Used by task.rs
    /// to decide whether a cancel/timeout kill would actually be acting on a
    /// live process, or is arriving after the process already exited naturally.
    /// A grandchild that is still alive after the root has exited counts as
    /// "alive" — the whole tree, not just the root, must be gone.
    /// On query failure, implementations fail toward "alive" (killing an
    /// already-dead tree is harmless; skipping a kill on a live one is not).
    fn tree_alive(kill: &KillHandle) -> bool;
    /// Reaps the root process of an abandoned run (panic/early-error path,
    /// where `wait()` was never reached and the `Child` is dropped without
    /// being waited on). Without this, the killed root lingers as a zombie in
    /// the caller's process on Unix — still answering `kill(pid, 0)` probes.
    /// Windows has no zombie concept; that implementation is a no-op.
    /// Must only be called after the tree has been killed.
    fn reap_abandoned(kill: &KillHandle);
}

#[cfg(not(target_os = "windows"))]
mod unix;
#[cfg(target_os = "windows")]
pub(crate) mod windows;

#[cfg(not(target_os = "windows"))]
pub(crate) use unix::UnixProcess as PlatformProcess;
#[cfg(target_os = "windows")]
pub(crate) use windows::WindowsProcess as PlatformProcess;
