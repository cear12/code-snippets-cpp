# C++20 Coroutine Task Scheduler with Work Stealing

A `Task<T>` coroutine type (usable as `Task<int>`, `Task<void>`, etc.) run
by a `CoroutineScheduler` that owns a pool of worker threads, each with
its own local queue; `schedule()`/`delay()`/`run_on_thread_pool()` are
awaitables a coroutine `co_await`s to hop onto the scheduler, sleep
without blocking a worker thread, or offload blocking work, respectively.
Workers that run out of local work steal from another worker's queue
before falling back to polling a shared global queue.

**This is the one file in the repository built with C++20** (`CXX_STANDARD
20` set specifically for this target in `CMakeLists.txt`; everything else
is C++17) since coroutines are a C++20 feature.

## Bug fixed: a promise type cannot declare both `return_void` and `return_value`

The original `Task<T>::promise_type` tried to support both `Task<void>`
and `Task<U>` from one template using `requires`-constrained overloads:

```cpp
void return_void() requires std::is_void_v<T> {}
void return_value(T value) requires (!std::is_void_v<T>) {}
```

This does not compile for *any* `T`: `[dcl.fct.def.coroutine]` requires a
promise type to have **exactly one** of `return_void`/`return_value`, and
that check is on the declarations present on the class, not on whether
their constraints could ever both be satisfied simultaneously -- GCC
rejects it outright ("declares both `return_value` and `return_void`")
before ever getting far enough to check which constraint holds for a
particular `T`. Fixed with the standard technique for this exact problem:
factor the result-holding logic into a small base template,
`detail::PromiseResult<T>`, with a `void` specialization --
`promise_type` privately inherits from it, so for any single
instantiation exactly one of the two member functions is actually
*declared* (via inherited-member lookup), satisfying the promise-type
contract. Everything else about `promise_type` (`get_return_object`,
`initial_suspend`, `final_suspend`, `unhandled_exception`,
`await_transform`) stays shared and unchanged.

## Bug fixed: `DelayAwaitable` read a member through `this` after a detached sleep

```cpp
void await_suspend(std::coroutine_handle<> handle) const {
    std::thread([handle, this]() {
        std::this_thread::sleep_for(duration);   // reads *this after suspension
        get_scheduler().schedule(handle);
    }).detach();
}
```

`duration` is read through `this` *after* the coroutine has suspended and
the sleep has elapsed -- safe only because the awaiter happens to live
inside the coroutine frame, which stays alive as long as nothing destroys
the `Task` early. `ThreadPoolAwaitable` right below it avoids exactly this
hazard by capturing its captured function *by value* into the detached
thread's lambda instead of reaching through `this`. Made `DelayAwaitable`
consistent with that already-correct sibling: capture `duration` by value,
so the detached thread no longer depends on the coroutine frame's
lifetime at all.

## Dead code removed

`TaskAwaitable` duplicated `ScheduleAwaitable` exactly (`await_suspend`
just calls `get_scheduler().schedule(handle)`, identically) and was never
referenced anywhere else in the file. Removed rather than kept as an
unexplained second name for the same thing.

## Known limitation (documented, not fixed)

`schedule()` always pushes directly onto a worker's *local* queue
(round-robin) and calls `worker->condition.notify_one()` -- but no code
anywhere actually `wait()`s on a `WorkerThread::condition`, and nothing
ever pushes onto `global_queue`. So a worker whose local queue just went
empty, and who fails to steal from anyone else, always falls through to a
10ms `wait_for` on `global_condition` before it loops back around --
harmless (every scheduled task still runs, worker-to-worker stealing
still works), but it means an idle worker can take up to ~10ms to notice
new local work instead of waking immediately. Left as a documented
characteristic rather than rewired, since fixing it properly (waiting on
each worker's own condition variable, with a real producer for
`global_queue`) is a bigger redesign than this cleanup pass's bug-fixing
scope.
