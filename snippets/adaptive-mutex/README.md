# Self-Tuning Adaptive Mutex

A mutex that watches its own contention and adjusts its own locking
strategy at runtime, in three escalating phases:

1. **Optimistic try**: one uncontended CAS -- the common case when the
   mutex is free.
2. **Adaptive spin**: busy-wait with exponential+jittered backoff, up to
   a `spin_limit_` that grows when spinning is usually paying off
   (>80% success rate) and shrinks when it usually isn't (<30%).
3. **Sleeping wait**: once the spin budget is exhausted, back off to
   `std::this_thread::sleep_for`, doubling the sleep duration each retry
   (capped), and mark the mutex `kLockedWithWaiters` so `Unlock()` can
   tell contended and uncontended release apart.

Roughly once every 256 acquisitions, `AdaptParameters()` looks at the
last second's statistics and adjusts `spin_limit_` and
`base_sleep_duration_` for the next period -- the mutex tunes its own
spin/sleep trade-off to the contention pattern it's actually
experiencing, rather than using one fixed policy for every workload.

## Bug fixed

`TrySpinLock()` was declared `const` but calls
`state_.compare_exchange_weak(...)`, which mutates `state_` -- and unlike
the statistics counters (`total_acquisitions_`, `spin_limit_`, etc., all
correctly marked `mutable` since `AdaptParameters() const` and
`GetStatistics() const` need to touch them), `state_` itself is not
`mutable`, since normal lock-acquiring code always runs through a
non-const `AdaptiveMutex&`. That combination -- `const` method, mutating
call, non-`mutable` member -- doesn't compile ("passing `const
std::atomic<unsigned int>` as `this` argument discards qualifiers").
Every call site is already in a non-const member function, so the
`const` was simply extraneous; removed it rather than marking `state_`
`mutable`, since `state_` is the mutex's real lock state, not
incidental bookkeeping. Also removed an unused local
(`sleep_success`, read from `sleep_acquisitions_` but never referenced
again -- `-Wunused-variable`).

## Known limitation (by design, not a bug)

`kLockedWithWaiters` is tracked correctly but doesn't gate a real OS-level
wait/wake (no futex, no condition variable): a thread in the sleeping
phase busy-polls `state_` on a timer regardless of whether it marked
itself a waiter. A production implementation would use this state to
back a `std::atomic::wait`/`notify_one` (C++20) or platform futex so
`Unlock()` can wake a sleeper immediately instead of it discovering the
unlock on its next timed poll.
