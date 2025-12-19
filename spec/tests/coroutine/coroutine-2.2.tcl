# Test: coroutine in namespace
namespace eval myns {
    proc gen {} {
        yield
        yield "in namespace"
        return "done"
    }
    coroutine ::myns::coro gen
}
puts [myns::coro]
puts [myns::coro]
