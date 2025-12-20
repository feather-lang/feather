# Test: delete command by renaming to empty string
proc foo {} {
    return "hello"
}
rename foo ""
catch {foo} err
puts $err
