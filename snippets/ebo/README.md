# Empty Base Optimization (EBO)

A C++ object's size can never be zero -- otherwise two distinct objects
could share the same address, breaking pointer arithmetic and identity.
So even a class with no data members at all (an "empty class") has some
minimal nonzero size, usually 1 byte.

The standard carves out an explicit exception for *base class*
subobjects: a compiler is permitted to give an empty base class zero
space within a derived object, as long as doing so doesn't cause two
subobjects of the same type to share an address. This lets a derived
class effectively "absorb" an empty base for free -- used throughout the
standard library and elsewhere to attach zero-cost tag types, policies,
or (stateless) allocators/deleters to a class without growing it.
