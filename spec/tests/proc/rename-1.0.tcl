# Test: basic rename - rename a procedure
proc foo {} {
    return "hello from foo"
}
rename foo bar
puts [bar]
