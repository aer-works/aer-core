#ifndef AER_H
#define AER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Error codes
 * All FFI functions that can fail return one of these values.
 * The integer values are stable ABI — never reorder or renumber them.
 * ------------------------------------------------------------------------- */
typedef enum {
    AER_OK = 0,
    AER_ERR_NULL_POINTER = 1,         /* a required pointer argument was NULL */
    AER_ERR_SPAWN_FAILED = 2,         /* OS refused to spawn the process */
    AER_ERR_WAIT_FAILED = 3,          /* OS error while waiting for exit */
    AER_ERR_INVALID_STATE_TRANSITION = 4,
    AER_ERR_TIMED_OUT = 5,            /* process killed because timeout elapsed */
    AER_ERR_KILL_FAILED = 6,          /* kill attempt itself failed */
    AER_ERR_ALREADY_RUN = 7,          /* aer_task_run called more than once */
    AER_ERR_PANIC = 8,                /* unexpected internal panic (see aer_last_error_message) */
} AerErrorCode;

/* -------------------------------------------------------------------------
 * Event
 * Delivered to AerEventCallback in order: Started, then Exited.
 * ------------------------------------------------------------------------- */
typedef struct {
    uint32_t kind;  /* 0 = Started (pid field valid), 1 = Exited (code field valid) */
    uint32_t pid;   /* process ID; meaningful when kind == 0 */
    int32_t  code;  /* exit code;  meaningful when kind == 1; -1 on signal/timeout kill */
} AerEvent;

/* -------------------------------------------------------------------------
 * Task (opaque handle)
 * Allocate with aer_task_new; free with aer_task_free.
 * Do not share a single handle across threads without external synchronisation.
 * ------------------------------------------------------------------------- */
typedef struct AerTask AerTask;

/* -------------------------------------------------------------------------
 * Event callback
 * Called synchronously from aer_task_run on the calling thread.
 * The `event` pointer is only valid for the duration of the callback.
 * `user_data` is whatever was passed to aer_task_run.
 * ------------------------------------------------------------------------- */
typedef void (*AerEventCallback)(const AerEvent *event, void *user_data);

/* -------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/**
 * Create a new task.
 *
 * `program`  — null-terminated, valid UTF-8, no embedded NUL bytes.
 * `args`     — array of `args_len` null-terminated strings; may be NULL when args_len == 0.
 * Returns NULL on any invalid input (NULL program, non-UTF-8 string, NULL element).
 * The returned handle must be freed with aer_task_free.
 */
AerTask *aer_task_new(const char *program,
                      const char *const *args,
                      size_t args_len);

/**
 * Set a wall-clock timeout in milliseconds.
 *
 * Must be called before aer_task_run. If the process has not exited by the
 * deadline it is killed and aer_task_run returns AER_ERR_TIMED_OUT.
 */
AerErrorCode aer_task_with_timeout(AerTask *task, uint64_t timeout_ms);

/**
 * Spawn the process and block until it exits.
 *
 * `callback`  — called with Started then Exited events; may be NULL to ignore events.
 * `user_data` — passed through to callback unchanged; may be NULL.
 *               Must remain valid for the duration of this call.
 *
 * A given handle may only be run once; a second call returns AER_ERR_ALREADY_RUN.
 * Call aer_last_error_message() to get a human-readable description of any error.
 */
AerErrorCode aer_task_run(AerTask *task,
                           AerEventCallback callback,
                           void *user_data);

/**
 * Free a task handle. Safe to call with NULL (no-op).
 * Do not use the handle after this call.
 */
void aer_task_free(AerTask *task);

/**
 * Return the last error message for this thread as a null-terminated C string.
 * Returns NULL if no error has occurred since the last successful operation.
 *
 * The pointer is valid until the next FFI call on this thread. Do not free it.
 */
const char *aer_last_error_message(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AER_H */
