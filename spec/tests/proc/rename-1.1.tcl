# Test: original name no longer exists after rename
proc foo {} {
    return "hello"
}
rename foo bar
catch {foo} err
puts $err
