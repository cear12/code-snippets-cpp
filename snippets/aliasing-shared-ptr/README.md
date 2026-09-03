# Aliasing `shared_ptr`

`std::shared_ptr` has a constructor that shares another `shared_ptr`'s
*ownership* (its reference count and deleter) while pointing at a
*different* address -- typically a subobject of the owned object. The
resulting pointer keeps the whole parent object alive for as long as it
exists, even though dereferencing it only ever touches the subobject.

Useful for handing out a `shared_ptr` to one field of a larger owned
object without exposing (or separately managing the lifetime of) the
object itself.
