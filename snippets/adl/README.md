# Argument-Dependent Lookup (ADL / Koenig lookup)

Named for Andrew Koenig: a set of rules, additional to ordinary
unqualified name lookup, for resolving an unqualified function call.
When a function name is looked up unqualified (no `::`), the namespaces
"associated with" each argument's type are also searched -- for a class
type, that includes the namespace the class itself was declared in, its
base classes, and so on.

This is why `std::cout << x` and `swap(a, b)` (found via ADL, picking up
a type-specific `swap` overload instead of always calling `std::swap`)
work without qualification, and it's the mechanism behind the "hidden
friend" idiom.
