# Test: errorCode variable after error with code
catch {error "test" "" {POSIX ENOENT}}
puts "errorCode exists: [info exists ::errorCode]"
