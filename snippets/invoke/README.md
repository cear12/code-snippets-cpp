# `std::invoke`

A uniform way to call *anything* callable -- a plain function, a function
object/lambda, a pointer to member function, or a pointer to member data
-- through one syntax, `std::invoke(callable, args...)`. Generic code that
needs to accept "anything callable with these arguments" (as `std::thread`,
`std::function`, and `std::bind` all do internally) is built on exactly
this uniformity.
