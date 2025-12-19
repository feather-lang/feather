# Test: socket with invalid port - should error
catch {socket localhost notaport} result
puts $result
