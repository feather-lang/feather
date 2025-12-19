# Test: basic coroutine creation - immediate return gives result to coroutine caller
proc simple {} {
    return "hello from coroutine"
}
puts [coroutine mycoro simple]
