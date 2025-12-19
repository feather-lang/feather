# Test: socket buffering modes - none (unbuffered)
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -buffering none
puts [chan configure $client -buffering]
close $client
close $srv
