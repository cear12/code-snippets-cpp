# Type-as-Value Tag Dispatch

Encodes a value into the *type system* (`GenericType<TypeA>` vs.
`GenericType<TypeB>` are different types, even though they hold no data)
so overload resolution -- not an `if`/`switch` -- picks the right
function at compile time. The same underlying technique as
`std::integral_constant`/tag dispatch generally: turning a compile-time
value into a distinct type lets the compiler do the branching for you.
