# Test: socket with negative port - should error
catch {socket localhost -1} result
puts $result
