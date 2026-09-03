# "Voldemort Types" -- Unnameable Local Types

A type defined *inside* a function (a "local class") has no name any
caller can write down -- yet a lambda with `auto` return type can still
return an instance of it, and a caller can hold that instance via `auto`
(and reuse the type itself via `decltype`), without either side ever
naming the type. Sometimes called "Voldemort types" (the type whose name
must not be spoken) after a talk by Joseph Falcone/Alberto Ganesh
(Walter E. Brown popularized the name at CppCon).

Because the type is still known to the compiler (just not writable by a
programmer), it composes fine with `auto`, `decltype`, templates, and --
as shown here -- can even derive from a normal, nameable interface.
