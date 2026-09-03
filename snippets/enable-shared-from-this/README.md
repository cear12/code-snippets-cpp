# `enable_shared_from_this`: Good vs. Bad

`std::enable_shared_from_this<T>` lets an object safely hand out
additional `shared_ptr`s to itself that correctly share the *existing*
control block (refcount) of whatever `shared_ptr` already owns it.

The bad alternative -- `std::shared_ptr<T>(this)` -- compiles just as
readily, but creates a *second, independent* control block for the same
object. Two independent owners each believing they alone are responsible
for deleting the object leads to a double-free once both eventually let
go.

## A safety note on this file's "Bad" demo

Actually letting both of `Bad`'s independent owning `shared_ptr`s run
their destructors would double-free the object -- undefined behavior,
likely a crash. To demonstrate the bug without actually triggering it,
`main` prints both `use_count()`s (each reporting `1`, revealing that
neither knows about the other) and then deliberately leaks the second
`shared_ptr` rather than letting both destruct.
