# IIFE -- Immediately-Invoked Function Expression

A lambda defined and called in the same expression, `[]{ ... }()`, used
to initialize a variable (often `const`) from logic more complex than a
single expression, without polluting the enclosing scope with the
helper variables or branches that logic needed.

The alternative -- a separate named helper function, or a sequence of
mutable statements building the value up -- either adds a symbol to the
surrounding scope that's only ever used once, or gives up on making the
variable `const` from the moment it's declared. An IIFE gets a nontrivial
initializer while keeping both benefits.
