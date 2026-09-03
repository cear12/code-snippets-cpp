# Functional-Style Factory

A factory that's just a `std::function` returning `unique_ptr<IProduct>`,
instead of a `Factory` base class with a virtual `create()`. `Client`
depends only on that function's signature, not on any concrete factory
type -- a lambda, a free function, or a stateful functor are all equally
usable wherever a `Factory` is expected, with no inheritance involved on
the factory side at all.
