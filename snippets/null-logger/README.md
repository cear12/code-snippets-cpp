# Null Object via aliasing `shared_ptr`

Returns a `shared_ptr<Logger>` that points at a static (program-lifetime)
`NullLogger` singleton, without the `shared_ptr` trying to `delete` it --
combining the Null Object pattern with the `shared_ptr` aliasing
constructor (see the sibling `aliasing-shared-ptr` snippet) and an empty
`shared_ptr<void>` as a stand-in "owns nothing" control block.

Useful when an API's contract is "returns a `shared_ptr<Logger>`" (so
callers can hold it exactly like any other logger) but the specific
logger being handed out is a do-nothing singleton with no real lifetime
to manage.
