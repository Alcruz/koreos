# Testing

Write tests against observable behavior, not implementation details.

A test should exercise a module through its public API and assert on outcomes
a caller could reasonably care about — return values, out-parameters, state
visible via other public calls. It should not reach into private structs,
depend on the exact order of internal operations, or pin bookkeeping the
module is free to change. If a refactor that preserves the module's contract
would break the test, the test is testing the wrong thing.
