# Test: basic yieldto usage with string cat
proc gen {} {
    yield
    set result [yieldto string cat "yielded"]
    return "got: $result"
}
coroutine g gen
puts [g]
puts [g "input"]
