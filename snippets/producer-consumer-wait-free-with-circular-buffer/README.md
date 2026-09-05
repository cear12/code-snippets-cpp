# Producer-Consumer with a Wait-Free Circular Buffer

Three related pieces, building up from the simplest case to a usable
service:

- **`SPSCRingBuffer`**: single-producer/single-consumer lock-free ring
  buffer, same per-slot-sequence-number technique as this repo's
  `single-producer-multiple-consumer` (a slot is ready to write when
  `sequence == position`, ready to read when `sequence == position + 1`).
  With exactly one writer and one reader, neither side ever needs a CAS --
  relaxed loads plus a release store on each side's own position is
  enough, which is what makes it wait-free rather than just lock-free.
- **`MPMCRingBuffer`**: the same layout generalized to any number of
  producers/consumers (Dmitry Vyukov's bounded MPMC queue) -- every
  producer/consumer races for a slot via `compare_exchange_weak` on a
  shared position instead of owning it outright, so it's lock-free but no
  longer wait-free.
- **`ProducerConsumerSystem`**: wraps an `SPSCRingBuffer` with
  `RunProducer`/`RunConsumer` thread-loop helpers and throughput
  statistics, for the common case of "one thread feeds it, one thread
  drains it, tell me how that went."

## Bugs fixed

- **Missing includes**: `<thread>` and `<chrono>` were used throughout
  (`std::this_thread::yield/sleep_for`, `std::chrono::microseconds`) but
  never included. Didn't compile standalone.
- **`SPSCRingBuffer::TryPush` silently discarded move semantics**: the
  original had
  `if constexpr (is_move_constructible_v<T> && !is_copy_constructible_v<T>) slot.data = forward<U>(item); else slot.data = item;`
  -- for any type that is *both* movable and copyable (`std::string`,
  `std::vector`, ...; the overwhelmingly common case), that `else` branch
  always copy-assigns, even when the caller passed an rvalue specifically
  to get a move. Assignment already dispatches to move- vs copy-assignment
  correctly based on whether the argument is an rvalue or lvalue, so the
  whole `if constexpr` was both wrong and unnecessary; replaced with a
  plain `slot.data_ = std::forward<U>(item);` (matching
  `MPMCRingBuffer::TryPush` and `single-producer-multiple-consumer`'s
  `TryEnqueue`, which already did this correctly).
- **`ProducerConsumerSystem::RunProducer` dropped items under
  backpressure**: the original called `producer()` for a fresh item on
  *every* loop iteration, including right after a failed
  `try_push()` -- so a push that failed because the buffer was full
  discarded the item it just failed to store and moved on, rather than
  retrying it. Fixed by retrying the same item in an inner loop and only
  calling the producer callback again once a push actually succeeds.
- **Shutdown could drop the producer's last item**: `RunConsumer`'s main
  loop originally exited as soon as it observed `running_ == false`
  (set by `Stop()`) and then ran its drain pass -- but `RunProducer` checks
  `running_` *before* producing/pushing the next item, so there's a window
  where `Stop()` has already flipped the flag while the producer is still
  in the middle of pushing its last item. If the consumer's drain pass ran
  before that push landed, the item was gone for good. Fixed with a
  separate `producer_done_` flag that `RunProducer` sets (release) only
  after its loop has fully exited, i.e. after every push it will ever
  make; `RunConsumer` now keys its shutdown off that flag (acquire)
  instead of `running_` directly, so by the time it decides to drain and
  stop, every item the producer pushed is guaranteed visible. Verified
  race-free with ThreadSanitizer in addition to the deterministic
  exact-count check in `main()`.

Renamed from the original loose `producer-consumer-wait-free-with-circular-buffer.h`
at the repo root (no `main()`, camelCase methods, raw `val`/`next`-style
field names) to this directory's `example.cpp`, matching every other
snippet: `class`es use PascalCase methods and trailing-underscore members,
and `main()` runs real single- and multi-threaded correctness checks
(FIFO order for the SPSC buffer, exactly-once delivery across 4 producers
x 4 consumers for the MPMC buffer, and an exact produced/consumed count
plus checksum for the full system) rather than just demonstrating that it
compiles.
