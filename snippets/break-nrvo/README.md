# A pattern that can defeat NRVO

Named Return Value Optimization (NRVO) lets the compiler construct a
function's return value directly in the caller's storage, skipping a
copy/move, when a function returns the *same* named local variable from
every return statement. It's permitted, not mandated, by the standard --
compilers are free to still copy/move.

This is a known pattern that has, historically, defeated NRVO on some
compilers (notably older MSVC): a return statement for a *different*
temporary appears earlier in the function, even in a branch that's never
taken (`if (false)`). Some optimizers stop treating the later `return s;`
as NRVO-eligible once they've seen *any* other return statement with a
different operand, even an unreachable one.

This isn't a correctness bug -- the function is fully correct either way,
just potentially one move slower on affected compilers/versions. It's
included here as a documented gotcha, not a idiom to imitate.
