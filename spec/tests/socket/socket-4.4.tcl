# Test: chan configure -error on socket (no error expected)
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
puts [chan configure $client -error]
close $client
close $srv
