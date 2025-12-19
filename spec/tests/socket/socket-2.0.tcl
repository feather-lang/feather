# Test: socket -server with no command - should error
catch {socket -server} result
puts $result
