# Test: socket buffer size configuration
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -buffersize 8192
puts [chan configure $client -buffersize]
close $client
close $srv
