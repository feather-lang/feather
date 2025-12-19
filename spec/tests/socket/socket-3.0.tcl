# Test: socket client connection to non-listening port fails
catch {socket localhost 59999} result
puts [string match "*connection refused*" [string tolower $result]]
