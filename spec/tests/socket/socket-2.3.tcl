# Test: socket -server creates a channel (port 0 = OS allocates)
set srv [socket -server {apply {{chan addr port} {}}} 0]
puts [string match sock* $srv]
close $srv
