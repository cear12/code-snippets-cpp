# Non-Member Interface Function (Hidden Friend)

A class's public interface doesn't have to consist only of member
functions -- a free function taking the class by value/reference can be
just as much a part of its interface, and is found through
Argument-Dependent Lookup (see the sibling `adl` snippet) without needing
qualification, as long as it's declared in the same namespace as the
class.

Preferring non-member functions for operations that don't need private
access is a widely cited piece of guidance (Scott Meyers, *Effective
C++*, Item 23): it reduces the surface a class's own encapsulation has to
protect, and composes better with generic code that doesn't know a
member function exists.
