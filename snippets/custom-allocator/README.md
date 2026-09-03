# A Minimal Custom Allocator

The minimum shape a type needs to be usable as a C++ Standard Library
allocator: the handful of member typedefs and `allocate`/`deallocate`
(plus, pre-C++20, `construct`/`destroy`, still useful to define even
though the library will synthesize them if absent) that
`std::allocator_traits` looks for. Passed as a container's second
template argument, it lets a container use custom memory management
without changing anything about how the container itself is used.
