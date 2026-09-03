# Future/Promise Framework with Continuations

A `std::future`-like `Future<T>`/`Promise<T>` pair, but with continuations
(`then()`, `catch_error()`), combinators (`when_all()`, `when_any()`), and
a thread-pool-backed `async()` -- the kind of thing `std::future` doesn't
give you out of the box. Each `Future<T>` shares a `SharedState` with its
`Promise<T>` (mutex, condition variable, manually-managed storage for the
result so `T` doesn't need to be default-constructible, and an optional
stored continuation to run when the value or exception arrives).

This was, by a wide margin, the buggiest file in the repository -- four
independent, real bugs, three of which are hard compile errors and one of
which is a guaranteed deadlock on the framework's primary use case. All
four are listed here in the order they'd actually bite: compile errors
first, then the runtime one.

## Bug 1: missing includes

`<queue>`, `<thread>`, `<tuple>`, and `<utility>` are all used
(`std::queue`, `std::thread`, `std::tuple`/`std::apply`/`std::get`,
`std::forward`) but the file only included `<future>`, `<functional>`,
`<memory>`, `<type_traits>`, `<exception>`, `<mutex>`,
`<condition_variable>`, `<atomic>`, `<vector>`, `<chrono>`. Doesn't
compile standalone. Fixed by including what's used.

## Bug 2: `when_all`/`when_any` need a `value_type` that didn't exist

Both combinators do `typename std::decay_t<Futures>::value_type`, but
neither `Future<T>` nor its `Future<void>` specialization ever declared a
`value_type` member. Fixed by adding `using value_type = T;` (and, in the
`void` specialization, `using value_type = void;`).

## Bug 3: `when_all`'s per-future index wasn't a compile-time constant

```cpp
size_t index = 0;
auto process_future = [&](auto&& future) {
    size_t current_index = index++;
    future.then([..., current_index](auto&& fut) mutable {
        std::get<current_index>(*results) = fut.get(); // current_index is a runtime size_t
    });
};
```

`std::get<N>` requires `N` as a compile-time constant; a captured runtime
`size_t` cannot be used there, regardless of the fact that its value never
changes after capture -- doesn't compile. Fixed with the same technique
used for the same problem in `idioms-cpp`'s `named-template-parameter` and
this repo's own `lock-free-stack`: a free function template
(`detail::when_all_attach<I>`) taking the index as an explicit template
argument, invoked once per future via a fold expression over
`std::index_sequence_for<Futures...>`, so every `std::get<I>` call sees a
real compile-time constant. Kept C++17-compatible on purpose (no C++20
template-lambda syntax) since the rest of this repository targets C++17.

## Bug 4: `SharedState()`'s `= default` silently only worked for trivial `T`

```cpp
union { T value; };   // manual lifetime: placement-new'd in set_value(),
                       // explicitly destroyed in ~SharedState()
SharedState() = default;
```

For `T = int` this compiles fine -- but `when_all()` instantiates
`Promise<std::tuple<...>>`, and a union with a non-trivially-default-
constructible variant member (like `std::tuple`) has its *own* default
constructor implicitly deleted. `SharedState() = default` then tries to
default-initialize that union member and becomes ill-formed too:
`error: use of deleted function 'SharedState::SharedState()'`. This is
exactly the kind of bug that a quick manual test with `T = int` won't
catch -- confirmed here by testing with `T = std::tuple<int, int>`
specifically (used by `when_all`) rather than assuming a scalar type was
representative. Fixed by giving `SharedState()` a user-provided, empty
body instead of `= default`: an empty body doesn't attempt to
default-initialize the union member at all, which is exactly what the
manual placement-new lifetime scheme needs.

## Bug 5 (the real headline bug): continuations ran while still holding the lock -- guaranteed self-deadlock

```cpp
void set_value(U&& val) {
    std::lock_guard<std::mutex> lock(mutex);
    ...
    ready.store(true, std::memory_order_release);
    if (continuation) {
        continuation();     // <-- still holding `lock` here
    }
    condition.notify_all();
}
```

`continuation()` is exactly the callback `then()` installed, and that
callback's whole *point* is normally to call `.get()` on a `Future`
wrapping this same `SharedState`:

```cpp
state_->set_continuation([state = state_, func, new_promise]() mutable {
    Future<T> current_future(state);
    auto result = func(std::move(current_future)); // func typically calls fut.get()
    ...
});
```

`Future::get()` calls `state_->get()`, which does
`std::unique_lock<std::mutex> lock(mutex);` on that *same* `std::mutex` --
on the *same thread* that's still holding it from `set_value()`. A
`std::mutex` is not recursive; relocking it from the thread that already
holds it is undefined behavior, and in practice it deadlocks outright.
This isn't a rare edge case -- it reproduces on literally the first
`then()`-chained `get()` call, i.e. the primary way this API is meant to
be used, and it was only caught by actually running the code (a
`-fsyntax-only` check can't see it; the class compiles fine, it just
hangs at runtime). `set_continuation()` has the identical problem for the
case where `then()` is attached to an *already-completed* future (it runs
`cont()` immediately, still under the lock).

Fixed the same way in all three places (`set_value`, `set_exception`,
`set_continuation`, in both the primary `Future<T>::SharedState` and the
`Future<void>` specialization): capture what needs to run into a local
variable while holding the lock, release the lock, *then* invoke it. This
also happens to be the generally-correct pattern for any code that calls
into unknown/user-supplied callbacks from inside a class's own lock --
never call out while holding your own internal mutex.

## Known limitation (documented, not fixed)

Each `SharedState` holds a single `continuation` (`std::function<void()>`,
not a list) and `then()`/`set_continuation()` silently overwrite whatever
was there before if called twice on the same `Future`. Every code path in
this file (including `when_all`/`when_any`, which each create one fresh
`Promise`/`Future` pair per input future rather than attaching two
continuations to one) already respects "at most one continuation per
`Future`", so this doesn't cause a bug here -- but it's a real API
constraint a caller needs to know about, not enforced by the type system.
