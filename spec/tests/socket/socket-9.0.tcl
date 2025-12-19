# Test: socket buffering modes - line buffering
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -buffering line
puts [chan configure $client -buffering]
close $client
close $srv
