# Test: socket -server -reuseaddr option
set srv [socket -server {apply {{chan addr port} {}}} -reuseaddr 1 0]
puts [string match sock* $srv]
close $srv
