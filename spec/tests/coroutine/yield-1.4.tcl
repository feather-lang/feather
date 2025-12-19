# Test: nested procedure call with yield
proc helper {} {
    yield "from helper"
}
proc outer {} {
    yield "before"
    helper
    yield "after"
    return "end"
}
coroutine o outer
puts [o]
puts [o]
puts [o]
