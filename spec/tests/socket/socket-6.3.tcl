# Test: server socket cannot be read
set srv [socket -server {apply {{chan addr port} {}}} 0]
catch {read $srv} result
puts [string match "*not open for reading*" $result]
close $srv
