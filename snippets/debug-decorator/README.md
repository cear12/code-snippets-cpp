# A Generic Logging Decorator for Callables

Wraps *any* callable -- a free function, a functor, a lambda -- with
before/after logging, without changing the callable's own code or
needing a common base class. `DebugDecorator` is itself callable (via
`operator()`), so decorating something and then calling the result reads
exactly like calling the original.
