# Variadic Templates: With and Without a Base Case

Two different ways to write a variadic `print` that streams every
argument to `std::cout`, kept in separate namespaces so both can be
compiled and exercised without colliding (they aren't meant to coexist in
one overload set -- see Notes).

`with_base_case::print` recurses one argument at a time down to a
zero-argument termination overload -- the traditional (pre-C++17)
technique. `fold_expression::print` instead uses a C++17 fold expression
over the comma operator to expand the whole pack in one non-recursive
statement.

## Notes

The original form of this idiom defines both the recursive overload set
*and* a same-named, unconstrained `print(Args&&...)` in the same scope,
which are ambiguous for any call with one or more arguments (both become
viable overloads with no way to prefer one). Namespacing them separately
here keeps each techique demonstrable on its own.
