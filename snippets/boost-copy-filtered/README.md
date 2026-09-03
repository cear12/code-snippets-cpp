# `boost::copy` + `boost::adaptors::filtered`

Boost.Range's pipe-style range adaptors let a filter be composed directly
into a `copy` call -- `copy(people | filtered(predicate), out)` -- instead
of writing a manual loop with an `if`, or reaching for
`std::copy_if` plus a separate output-sizing concern. This is the same
lazy-adaptor style the C++20 Ranges library later brought into the
standard (`std::views::filter`); Boost.Range predates it by well over a
decade.

## Honesty notes

**Not locally verified.** This snippet needs Boost (specifically
Boost.Range), which is not available in the sandbox this portfolio was
assembled in (no package-manager root access to install `libboost-dev`).
The code below is written to be correct, and CI installs Boost and
compiles it on every push -- see `.github/workflows/ci.yml`.
