# Fast C++ Stream I/O

Two well-known one-time setup calls that speed up `std::cin`/`std::cout`
considerably (often the difference between passing and failing a time
limit in competitive programming), wrapped in an immediately-invoked
lambda assigned to a `static const` so the setup runs exactly once, before
`main` starts using the streams.

`sync_with_stdio(false)` stops the C++ streams from staying byte-for-byte
interleaved with C's `stdio` (`printf`/`scanf`), which otherwise forces
unbuffered, slow operation. `cin.tie(nullptr)` stops `cin` from flushing
`cout` before every extraction, letting output batch up instead of
flushing on every single input read.

Once desynchronized, mixing `std::cin`/`cout` with `scanf`/`printf` on the
same streams is no longer safe -- pick one family and stick to it.
