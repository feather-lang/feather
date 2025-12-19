# Test: socket with port out of range - should error
catch {socket localhost 99999} result
puts $result
