# Test: chan configure -keepalive on socket
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -keepalive 1
puts [chan configure $client -keepalive]
close $client
close $srv
