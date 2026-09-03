# code-snippets-cpp

A collection of small, focused C++ idioms and techniques -- one per
directory under `snippets/`, each a single, self-contained,
compiler-verified `example.cpp` with a `README.md` explaining the
technique and a `main()` that actually demonstrates it (real output, not
just "it compiles").

This collection sits alongside `idioms-cpp` in this portfolio: `idioms-cpp`
covers named design idioms (CRTP, type erasure, policy-based design, and
so on); this repository covers more varied, often lower-level techniques
-- lock-free data structures, coroutine schedulers, a Future/Promise
framework, allocator and smart-pointer tricks, and assorted small
utilities.

## Layout

```
snippets/<name>/
  README.md      -- what the technique is, why it's useful, what (if anything) was fixed
  example.cpp     -- a single compiling, runnable translation unit
```

## Building

```
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure
```

Requires a C++17 compiler and CMake 3.16+. One target
(`coroutines-based-task-scheduler`) is built as C++20 specifically, since
it uses the `<coroutine>` header -- see that snippet's `CMakeLists.txt`
override.

### Boost

Two snippets (`boost-copy-filtered`, `boost-quick-allocator`) use Boost
(Boost.Range and Boost.SmartPtr respectively). If Boost isn't found on
your system, CMake skips just those two targets with a `STATUS` message
rather than failing the whole configure/build -- everything else is
self-contained (standard library only). CI installs `libboost-dev` so
both build there; see `.github/workflows/ci.yml`.

## Notable techniques

Concurrency: `lock-free-stack` (hazard-pointer-protected Treiber stack),
`single-producer-multiple-consumer` (Disruptor-style SPMC ring buffer),
`adaptive-mutex` (self-tuning spin/sleep mutex), `coroutines-based-task-scheduler`
(C++20 coroutines with work stealing), `future-based-async-system`
(`Future`/`Promise` with `then()`/`when_all()`/`when_any()`), `cas`,
`thread-performance-monitor-system`.

Smart pointers & lifetime: `aliasing-shared-ptr`, `enable-shared-from-this`,
`unique-ptr`, `universal-ptr`, `policy-smart-ptr`, `observer-ptr`,
`null-logger`, `moved-class`, `exception-safe-rollback`.

Templates & metaprogramming: `sfinae`, `variadic-template`,
`variadic-to-string`, `generic-type`, `invoke`, `underlying-type`.

Design patterns: `factory` (self-registering, breaks a core/plugin
dependency cycle), `builder`, `functional-style-factory`,
`debug-decorator`, `closure-hash-key-cache`.

Misc idioms: `adl`, `as-cast`, `bit-manipulation`, `break-nrvo`,
`circular-buffer`, `custom-allocator`, `ebo`, `fast-io`, `function-size`,
`iife`, `iterator`, `nullptr-emulation`, `perfect-forward`,
`stack-polimorphism`, `cmd-line-params-parsing`, `async`,
`async-wrapper`, `function-return-overload`, `non-member-friend-interface`.

## Where this collection came from

Rewritten from a flat collection of `.h`/`.txt` files (some containing
working code, some scratch notes, and in one case -- `factory` -- a
literally pasted ChatGPT conversation transcript). Every file was
compiled and run, not just read; a running total of real bugs found and
fixed along the way is in `code_quality_standards.md` at the repository
root of this portfolio (or see each snippet's own `README.md` for the
specifics, under "Bug(s) fixed").
