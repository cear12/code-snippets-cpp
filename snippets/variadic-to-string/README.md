# Variadic `to_string`

Streams an arbitrary number of arguments of arbitrary (streamable) types
into one `std::string`, using a C++17 fold expression over `operator<<`.
Equivalent to writing `oss << a << b << c << ...;` by hand, generalized to
any number of arguments of any streamable types.
