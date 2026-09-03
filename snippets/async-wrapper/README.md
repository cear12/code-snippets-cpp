# RAII `std::async` Wrapper

Wraps a `std::async`-launched task so its destructor always calls
`.get()`, guaranteeing the task is joined-and-observed (including any
exception it threw) no matter how the wrapper's scope is exited --
similar in spirit to how `std::async`'s own returned future already
blocks on destruction, made explicit and reusable as its own type.
