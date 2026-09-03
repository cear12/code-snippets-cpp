# Compare-And-Swap (CAS)

The atomic primitive most lock-free algorithms are built from: given an
address, an expected value, and a new value, atomically replace the
address's contents with the new value *only if* it still equals the
expected value, reporting whether the swap happened. Shown two ways: a
hand-written, single-threaded version that spells out exactly what CAS
means, and real concurrent use via `std::atomic::compare_exchange_strong`
in a retry loop -- the standard pattern for a lock-free
read-modify-write (here, incrementing a shared counter without a mutex).
