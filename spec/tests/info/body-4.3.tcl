# Test: info body - nonexistent procedure
catch {info body nonexistent} err
puts $err
