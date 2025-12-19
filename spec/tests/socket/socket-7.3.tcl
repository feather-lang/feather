# Test: non-blocking socket configuration
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -blocking 0
puts [chan configure $client -blocking]
close $client
close $srv
