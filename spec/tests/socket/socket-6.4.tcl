# Test: server socket cannot be written
set srv [socket -server {apply {{chan addr port} {}}} 0]
catch {puts $srv "test"} result
puts [string match "*not open for writing*" $result]
close $srv
