# Closure as a Stable Cache Key Generator

A closure that captures a `shared_ptr<std::string>` computed once (a
hash of some input) and hands back that same string every time it's
called. Useful when several independent pieces of code need to agree on
"the cache key for this thing" without recomputing or re-deriving it --
the closure *is* the shared, memoized derivation.
