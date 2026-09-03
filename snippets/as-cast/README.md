# `asX()` -- Manual RTTI via Per-Type Accessors

An alternative to `dynamic_cast` that doesn't need RTTI: the base class
declares one virtual `asX()` per type that might need a safe downcast,
each defaulted to return `nullptr`; the one class that really is an `X`
overrides its own `asX()` to return `this`.

See `idioms-cpp/idioms/downcast` (technique 4) and
`idioms-cpp/idioms/manual-rtti` for the fuller writeup of this family of
techniques and their trade-offs against `dynamic_cast`.

## Notes

The method here is named `asItemA()`; naming a member function the same
as its *enclosing* class (as the original sketch did for `ItemA`'s own
`asItemA`-equivalent) is not legal C++ -- that syntax is reserved for
constructors. Also fixed: the demo previously called a method
(`asItemA`) that didn't match what was actually declared (`ItemA`).
