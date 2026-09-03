# SFINAE: Three Ways to Constrain `printContainer`

Three different, independent techniques for the same goal -- print a
container's elements with `printContainer`, but only enable that overload
for types that actually *have* `begin()`/`end()` -- kept in separate
namespaces so each can be exercised on its own (see Notes).

1. **`enable_if_style`**: a hand-written `EnableIf<condition, Type>`
   (the pre-`<type_traits>` shape of `std::enable_if`) selects the
   overload's return type only when `condition` holds.
2. **`expression_validity_style`**: a trailing `decltype` over
   `begin(declval<T>())` is only well-formed if `T` actually has a
   `begin()` -- classic expression-SFINAE, no named trait needed.
3. **`if_constexpr_style`** (C++17): a single, unconstrained function
   template branches internally with `if constexpr`, discarding the
   not-taken branch from instantiation entirely -- often simpler than
   constraining the *overload* when there's no need for genuinely
   different overloads.

## Notes

This is a cleaned-up, working version of a file that was previously a set
of disconnected notes/fragments: a bare `sizeof(...) != sizeof(char)`
expression at file scope (illegal outside a function), a `template`
keyword typo'd as `emplate`, a function body that was a literal `{ ... }`
placeholder, and several draft overloads of `printContainer` that would
have been ambiguous if actually compiled together.
