# Bit Manipulation Cheat Sheet

A handful of small, widely-used bitwise tricks:

- Set/clear/toggle a single bit by position.
- Test whether a number is a power of two: `(n & (n-1)) == 0` (a power of
  two has exactly one set bit; subtracting 1 flips every bit below it,
  including that one, so ANDing the two clears everything -- true only
  when there was exactly one set bit to begin with, plus a zero check
  since the identity also holds for `n == 0`).
- Clear the lowest set bit: `n & (n-1)`.
- Isolate the lowest set bit: `n & -n` (relies on two's-complement
  negation: `-n` is `~n + 1`, so it shares no set bits with `n` below the
  lowest one, and flips everything above it).
