# Test: socket -server with -myaddr option
set srv [socket -server {apply {{chan addr port} {}}} -myaddr localhost 0]
puts [string match sock* $srv]
close $srv
