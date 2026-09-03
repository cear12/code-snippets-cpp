# Self-Registering Factory (breaking a core -> plugin dependency)

The problem this solves: a `core` module defines an abstract base class and
wants to create its concrete subclasses, but those subclasses live in
separate modules that *depend on* `core` -- so `core` creating them
directly by name (`new WincryptProvider()`) would mean `core` depending
*back* on its own dependents, a Dependency Inversion Principle violation
and, in a multi-project build, an outright circular dependency.

The fix: `core` exposes only an abstract `ICryptoProvider` interface and a
`CryptoProviderFactory` that creates instances by string id, with no
compile-time knowledge of any concrete provider. Each concrete provider
module self-registers into the factory at static-initialization time via
the `REGISTER_CLASS` macro -- `core` never names `WincryptProvider` or
`CryptokiProvider` anywhere in its own source.

## Layout note

In a real multi-project build this would be three separate projects:
`core/` (declares `ICryptoProvider` + `CryptoProviderFactory`, depends on
nothing), `wincrypt/` (declares `WincryptProvider`, depends on `core`),
and `cryptoki/` (declares `CryptokiProvider`, depends on `core`) -- `core`
has no dependency edge back to either. Collapsed into the one
`example.cpp` here since this portfolio's snippet format is a single
translation unit per idiom; the comment banners below mark where each
piece would live.

## Rewritten from scratch

The original file was, unusually, a **pasted ChatGPT conversation
transcript** left in place of source code -- Russian-language chat prose,
`ChatGPT сказал:`, and markdown code-fence/button artifacts
(`` cpp ``, `Копировать код`) sitting at file scope, which would have been
a large pile of syntax errors if anyone had tried to compile it directly.
Underneath the transcript were three draft designs in various states of
completion: a working string-id registration factory, a `REGISTER_CLASS`
macro sketch, and a final broken fragment calling
`Factory::registerType<B>("B")` as if it were templated and
bool-returning, which doesn't match the factory interface defined two
drafts earlier in the same file. This rewrite keeps the `REGISTER_CLASS`
self-registration approach (the most maintainable of the three -- adding a
provider needs no edit to any registry list) as a complete, real, compiling
program, and renames the original placeholder `A`/`B`/`C` classes to
`ICryptoProvider`/`WincryptProvider`/`CryptokiProvider` to match what the
original Russian-language question was actually about.
