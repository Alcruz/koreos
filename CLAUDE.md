# Testing

Write tests against observable behavior, not implementation details.

A test should exercise a module through its public API and assert on outcomes
a caller could reasonably care about — return values, out-parameters, state
visible via other public calls. It should not reach into private structs,
depend on the exact order of internal operations, or pin bookkeeping the
module is free to change. If a refactor that preserves the module's contract
would break the test, the test is testing the wrong thing.

Do not introduce production code — new abstractions, factored-out helpers,
new public APIs — solely to make something testable. If a helper only earns
its keep when the tests are counted, it's a test-driven abstraction, not a
real one; put it in the test tree (duplicating a few lines is fine) or find
a way to test through the existing surface. Production code exists to serve
the running system; tests observe that system, they don't get to shape it.
