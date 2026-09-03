# `BOOST_SP_USE_QUICK_ALLOCATOR`

A build-time flag that switches `boost::shared_ptr`'s internal control
block allocation to a faster, pooled allocator instead of the default
`operator new`/`operator delete`, worthwhile for code that creates and
destroys huge numbers of short-lived `shared_ptr`s. Must be defined
*before* any Boost smart pointer header is included (as shown), since
it's read by that header's own preprocessor logic, not passed as a
regular compiler flag here.

## Honesty notes

**Not locally verified.** Needs Boost, unavailable in the sandbox this
portfolio was assembled in (see the sibling `boost-copy-filtered`
snippet's README for the same note). CI installs Boost and builds this
file on every push.
