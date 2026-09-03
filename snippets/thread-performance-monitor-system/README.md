# Per-Thread Performance Monitor

A `mutex`-protected `std::map<thread::id, ThreadStats>` that any number
of worker threads can safely report task timings into concurrently, with
a single `print_report()` producing an aggregate view once work is done.
A straightforward, correctly-synchronized pattern for collecting
per-thread metrics without each thread needing its own separate output
channel.
