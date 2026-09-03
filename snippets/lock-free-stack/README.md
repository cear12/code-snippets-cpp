# Lock-Free Stack with Hazard Pointers

A Treiber stack (lock-free singly-linked-list stack via CAS-loop `push`
and `pop`) with hazard pointers layered on top to solve the classic
lock-free-stack use-after-free: when thread A is inside `pop()` reading
`old_head->next` while thread B concurrently pops *and deletes* that same
node, A dereferences freed memory. Hazard pointers fix this by having each
thread publish, in a well-known shared slot, which node it is currently
about to dereference; a "retire" list holds popped nodes until no
thread's hazard pointer still points at them, at which point
`delete_nodes_no_hazards()` actually frees them.

## Bugs fixed

Three real, independent bugs, found by actually compiling this file
(previously just a header nobody had built) rather than reading it:

1. **Type error**: `std::this_thread::get_id() == std::hash<...>{}(...) % 10 == 0`
   compares a `std::thread::id` against the result of a boolean expression
   -- `std::thread::id` has no `operator==` overload that accepts `bool`,
   so this cannot compile under normal semantic checking. (Operator
   precedence means it parses as `get_id() == ((hash(...) % 10) == 0)`,
   not the intended `(get_id() == hash(...) % 10) == 0`-shaped guard --
   the fix drops the stray `get_id() ==` entirely, since the intent was
   just "roughly 1 thread in 10 tries a reclaim pass.")
2. **Invalid reference binding**: two call sites
   (`delete_nodes_no_hazards()` and `pop()`'s retire step) called
   `to_be_deleted.compare_exchange_weak(node->next.load(), node)`, passing
   `node->next.load()` -- a temporary `Node*` *value* returned from an
   atomic load -- as the "expected" argument, which `compare_exchange_weak`
   requires as a real lvalue reference it can overwrite on CAS failure. A
   temporary cannot bind there under the standard reference-binding rules
   (confirmed with an isolated minimal repro and with `g++ -fsyntax-only`
   on this file directly -- both reject it, as any standards-conforming
   compiler should). Fixed with the standard load-a-named-local-then-CAS
   retry pattern already used correctly elsewhere in the same file
   (`push()`).
3. **Do-while `continue` targets the condition, not the loop top**: in
   `pop()`'s hazard-pointer protect-then-verify step, the original used a
   `do { ...; if (stale) continue; ... } while (cond);` loop. In a
   do-while loop, `continue` jumps to the **condition check**, not back to
   the top of the loop body -- so when the "head changed, retry" branch
   fired, control fell through to a CAS that used the freshly-reloaded
   `old_head` while the hazard pointer was still published for the
   *previous, now-stale* node. That leaves a real (if narrow) window where
   the node about to be dereferenced isn't actually protected -- exactly
   the use-after-free hazard pointers exist to close. Restructured as a
   `for (;;)` loop, where `continue` correctly restarts the whole
   protect-then-verify sequence from the top.

## Known limitation (documented, not fixed)

`MAX_THREADS = 100` hazard-pointer slots are handed out to threads on
first use and never reclaimed for the lifetime of the process (a thread
that exits doesn't free its slot). Fine for the bounded-thread-count demo
below; a long-running server with unbounded thread creation would need a
slot-recycling scheme.
