# `ScopeFail` -- Rollback Only on Exception

A scope guard that runs its cleanup function *only* if the scope is being
exited because of a new exception, not on ordinary (successful) return --
the complement of a plain RAII guard, which runs unconditionally.
`std::uncaught_exceptions()` (C++17) makes this possible without the
older `std::uncaught_exception()`'s ambiguity in nested-exception
scenarios: comparing the count at guard construction against the count
at destruction time tells you whether *this* scope is unwinding due to a
new exception.

## Notes

Boost has a purpose-built equivalent, `boost::scope::scope_fail`, for
codebases already depending on Boost. Not compiled here to avoid adding a
Boost dependency for one snippet when the standard-library version above
already fully demonstrates the idiom; for reference, it would be used as:

```cpp
boost::scope::scope_fail rollback_guard{[&]{ /* rollback */ }};
if (should_throw) throw std::runtime_error("Oops!");
rollback_guard.set_active(false); // cancel the rollback on success
```
