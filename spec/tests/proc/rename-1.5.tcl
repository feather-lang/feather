# Test: error when renaming to existing command name
proc foo {} { return "foo" }
proc bar {} { return "bar" }
catch {rename foo bar} err
puts $err
