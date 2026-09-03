# A Policy-Based Smart Pointer

A smart pointer whose deletion strategy and debug logging are both
template parameters (policies) rather than hard-coded or virtual --
`SmartPtr<T, DeletionPolicy, DebugPolicy>` composes a deletion policy
(`delete p`? `delete[] p`? something else entirely?) and a debug policy
(log construction/destruction, or not) at compile time, resolved with
zero runtime overhead. See the sibling `idioms-cpp` idiom
`policy-based-design` for the general technique this applies.

## Bug fixed

The original sketch declared *two* class templates both named `SmartPtr`
in the same scope -- an outright redefinition error, not valid C++, and
clearly two draft iterations of the same idea left in the file together
(the first had an unused `ReleasePolicy` parameter never referenced
anywhere in its body). Kept the more complete second draft (deletion +
debug policies, both actually used) and added the deletion policy
implementations and a demo, neither of which existed in the original.
