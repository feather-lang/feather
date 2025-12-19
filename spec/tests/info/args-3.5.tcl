# Test: info args - nonexistent procedure
catch {info args nonexistent} err
puts $err
