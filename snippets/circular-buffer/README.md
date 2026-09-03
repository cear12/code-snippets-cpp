# Circular (Ring) Buffer

A fixed-capacity FIFO where `put()` past capacity silently overwrites the
oldest not-yet-read element rather than growing or throwing -- the classic
shape used for audio/sample buffers, log ring buffers, and lock-protected
producer/consumer queues of bounded size. `head_`/`tail_` chase each other
around the backing array modulo `max_size_`; a separate `full_` flag
disambiguates the otherwise-identical "empty" and "full" states, both of
which have `head_ == tail_`.

`example.cpp` here is a from-scratch native implementation (mutex-guarded,
templated on element type) -- this closely follows the well-known
"Embedded Artistry" ring-buffer design, a widely-referenced public-domain
pattern for this exact problem.

## Bug fixed

`qDebug() << "Empty: ", circle.empty();` in the original used the comma
operator instead of `<<` -- so `circle.empty()`'s result was computed and
silently discarded rather than printed, an easy typo (`,` for `<<`) that
still compiles because the comma operator accepts any expression. Fixed in
the rewritten demo below (and Qt's `qDebug` replaced with `std::cout`
throughout, per this portfolio's no-Qt-at-build-time policy -- see the
sibling `qt-cpp` repository for the original Qt-based portfolio pieces).

## Boost variant (reference only, not compiled here)

The original file also demonstrated `boost::circular_buffer<T>`, a
production-grade ring buffer from Boost.Circular Buffer with a richer
interface (iterators, `push_back`/`push_front`, automatic overwrite).
Boost is not available in the sandbox this portfolio was assembled in, so
rather than add a third build target that can only ever be CI-verified,
here is that part for reference (CI does build the two dedicated Boost
snippets elsewhere in this repo -- `boost-copy-filtered`,
`boost-quick-allocator` -- to keep at least one Boost-dependent target
building in CI):

```cpp
#include <boost/circular_buffer.hpp>

boost::circular_buffer<int> cb(3);
cb.push_back(1);
cb.push_back(2);
cb.push_back(3);
cb.push_back(4);  // overwrites 1; buffer now holds {2, 3, 4}
cb.pop_front();    // removes 2; buffer now holds {3, 4}
```
