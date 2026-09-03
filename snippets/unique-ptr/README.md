# `unique_ptr`, Reimplemented

A minimal, from-scratch `unique_ptr`: sole ownership of a heap object,
moved but never copied, deleted automatically in the destructor. Written
here purely as an exercise in the mechanics RAII smart pointers are built
from -- real code should of course use `std::unique_ptr`.

## Notes

Originally duplicated verbatim as both a `.h` and a `.txt` file in this
repository; kept as the single copy here. Namespaced as `demo::unique_ptr`
(not placed in the global namespace under the bare name `unique_ptr`) so
this file can safely be compiled alongside code that also includes
`<memory>`.
