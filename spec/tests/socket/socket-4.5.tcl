# Test: chan configure -connecting on connected socket
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
puts [chan configure $client -connecting]
close $client
close $srv
