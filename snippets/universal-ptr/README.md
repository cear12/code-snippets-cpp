# Universal Pointer (v2: always-owning via `shared_ptr`)

A different design point from `idioms-cpp`'s `universal-ptr` (which has an
explicit observing/non-owning mode): this version always ends up owning
*something* -- a reference is copied into a new, `shared_ptr`-owned
object; a raw pointer is adopted and will be deleted by the `shared_ptr`;
a `unique_ptr` transfers ownership in. There is no way to hand this
version a pointer it merely *observes* without copying or adopting it.

## Bug fixed

The original sketch declared a `NoDelete` static member (a would-be no-op
deleter for a genuinely non-owning mode) but never wired it into any
constructor -- dead code suggesting an unfinished non-owning path. Removed
here rather than left dangling unused, since as written every constructor
already takes ownership one way or another; see `idioms-cpp`'s
`universal-ptr` for a version that actually implements the observing case
via exactly this no-op-deleter technique.
