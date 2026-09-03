# Builder (fluent setters)

Separates *constructing* an object piece by piece from the object itself
being immutable once built: `Foo::Builder` accumulates values through
chained setters (each returning `*this` by reference, letting calls
chain into one expression) and only creates the actual `Foo` in
`build()`. Handy when a type has several optional/defaultable
constructor parameters that would otherwise need a long, easy-to-miswire
positional constructor call.
