# Estimating a Function's Code Size (hack)

Subtracts the addresses of two functions to guess how many bytes of code
lies between them, on the assumption that the compiler/linker placed them
back-to-back in the order they're defined.

## Honesty notes

This is *not* portable or reliable, and is shown here as a documented
hack, not a technique to reach for. The standard says nothing about
function layout in memory; nothing prevents the compiler or linker from
reordering functions, inlining one of them away, padding between them, or
placing them in entirely different sections -- any of which silently
breaks this "measurement" without a compile error. It has historically
been used in embedded/JIT contexts where a specific toolchain's actual
behavior is known and relied upon deliberately, never as portable C++.
Built here with optimizations disabled, which makes the two functions
much more likely (never guaranteed) to stay adjacent and un-inlined.
