# `to_underlying`

A small helper that converts a (possibly `enum class`) enumerator to its
underlying integer type, via `std::underlying_type`. Standardized as
`std::to_underlying` in C++23; before that (and still, on C++17/20
codebases) it's common to hand-roll this one-liner.

Mainly useful for `enum class`, which -- unlike a plain `enum` -- has no
implicit conversion to its underlying integer type at all.
