# Test: socket -server with no port - should error
catch {socket -server mycallback} result
puts $result
