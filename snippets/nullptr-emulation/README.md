# nullptr Emulation (pre-C++11)

The library-only emulation of what became the `nullptr` keyword in
C++11: a type that implicitly converts to any pointer type (object or
member) but to nothing else, closing the overload-resolution and
type-safety holes that plain `0`/`NULL` left open.

## Notes

`nullptr` is a reserved keyword from C++11 onward, so the emulation's
original name for its global instance can no longer be used, even though
this technique predates the keyword. This file follows the language
version its build actually targets (C++17) and names the instance
`kMyNullptr` instead -- see the fuller writeup and historical reference
(N2431) under `idioms-cpp/idioms/nullptr-emulation`, which covers the
same idiom.
