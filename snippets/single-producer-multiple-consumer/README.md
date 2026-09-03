# Single-Producer, Multiple-Consumer Ring Buffer (Disruptor-style)

A lock-free SPMC queue using the LMAX-Disruptor technique of a
per-slot sequence number instead of separate head/tail indices: each
`Slot` carries its own `sequence`, and a slot is "ready to read" exactly
when `sequence == position + 1`. The single producer only ever does
relaxed loads/a release store on its own position, so it never contends
with consumers; multiple consumers race each other for a slot via one
`compare_exchange_weak` on a shared cursor, and the loser simply retries
at its (CAS-updated) new position. `TryEnqueueBatch`/`TryDequeueBatch`
extend the same idea to whole ranges at once. `ManagedSPMCSystem` wraps
the queue with a pool of consumer threads and a processor callback, for
the common case of "just hand me items or batches as they arrive."

Renamed from the original `single-produer-multiply-consumer.h` (typo'd
filename) to the correctly-spelled `single-producer-multiple-consumer`.

## Bugs fixed

- **Missing includes**: `<thread>`, `<chrono>`, `<functional>`, and
  `<iterator>` were all used (`std::this_thread`, `std::chrono`,
  `std::function`, `std::back_inserter`) but never included -- the file
  only compiled by accident when some other translation unit happened to
  drag those headers in first. Doesn't compile standalone; fixed by
  including what's used.
- **Dead local variable**: `try_dequeue_batch()` declared `T item;` at
  the top and never touched it again -- the loop moves straight from
  `slot.data` into the output iterator (`*out++ = std::move(slot.data)`)
  without going through it. Removed (`-Wunused-variable`).
- **Unused parameter**: `consumer_loop(size_t consumer_id, bool)` never
  reads `consumer_id` -- every consumer thread runs identical logic, so
  there's nothing to key off. Left unnamed rather than invented a use for
  it.
