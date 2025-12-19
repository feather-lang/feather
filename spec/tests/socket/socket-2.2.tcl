# Test: socket -server with invalid port
catch {socket -server mycallback notaport} result
puts $result
