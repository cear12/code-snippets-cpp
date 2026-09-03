# `observer_ptr`

> "It's the dumbest smart pointer" -- Walter Brown

A thin wrapper around a plain, non-owning pointer: it never allocates,
never deletes, and exists purely to make "this pointer does not own what
it points to" part of the type instead of a comment. Based on the
`std::experimental::observer_ptr` library-fundamentals proposal, which
never made it into the standard library proper, but the pattern (and
sometimes this exact class) shows up in codebases that want the
self-documentation without pulling in the experimental header.

## Related idioms

Compare with `universal-ptr` (in `idioms-cpp`), where a similar
"observing" mode is one case handled by a single pointer type that can
also own.
