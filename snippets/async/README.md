# A Minimal `std::async`, Reimplemented

Shows what `std::async(std::launch::async, func)` does under the hood:
spin up a thread, run `func` on it, and route its return value (or
exception) into a `std::future` via a `std::promise` -- both to see the
mechanism, and to point at a real limitation of the real
`std::async`-returned future that a from-scratch version can't
straightforwardly fix either.

## A real std::async gotcha this touches on

The `std::future` returned by `std::async` has a special property its
destructor blocks until the async task finishes, *only* for futures
obtained this way. A `std::future` obtained from a `std::promise`
directly (as here) has no such rule, so if this returned future is
discarded without being waited on, the spawned thread keeps running
detached from anything that could join it -- see `main` for how this
example avoids that.
