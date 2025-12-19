# Test: socket -async returns immediately
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket -async localhost $port]
puts [string match sock* $client]
close $client
close $srv
