# A Minimal Custom Iterator

The smallest useful shape of an iterator: `operator*`, `operator++`, and
`operator!=`, enough to make a custom container work with a range-based
`for` loop (which only ever needs those three operations from
`begin()`/`end()`). Real STL-compatible iterators typically implement
more of the iterator interface (`operator==`, the iterator category
typedefs, etc.) so they interoperate with `<algorithm>`, but this
minimal set is exactly what range-`for` itself requires under the hood.
