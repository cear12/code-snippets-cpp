# `std::forward`, Reimplemented

`std::forward<T>(arg)` is what makes perfect forwarding work: inside a
forwarding-reference function template (`template<typename T> f(T&& arg)`),
it recovers whether the original caller's argument was an lvalue or an
rvalue and casts `arg` back to that value category, so a further call like
`g(std::forward<T>(arg))` sees the same value category the original caller
used.

It's implemented with two overloads distinguished only by whether the
*template argument* `T` is deduced as a reference type, relying on
reference collapsing rules -- genuinely one of the more subtle small
pieces of the standard library, worth reading in full at least once.

Reimplemented here (under a different name -- see Notes) purely to see
what it actually does; real code should of course use `std::forward`.

## Notes

The original form of this idiom places the reimplementation directly in
`namespace std`. Renaming that in this snippet is not just tidiness: any
translation unit that also includes `<utility>` (as this one does, to
compare against the real `std::forward`) would otherwise get a hard
redefinition error from having two conflicting declarations of
`std::forward` in the same namespace.
