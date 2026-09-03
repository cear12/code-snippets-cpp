# Overload Resolution via Multiple Conversion Operators

A single type can define several conversion operators
(`operator std::string()`, `operator unsigned()`, `operator Foo()`, ...),
and the *context* a value of that type is used in -- an explicit cast, an
argument slot, an initializer -- picks which one applies. Each use site
here has exactly one applicable conversion, so there's no ambiguity, even
though `Func` offers three different ones.
