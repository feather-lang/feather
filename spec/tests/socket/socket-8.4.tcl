# Test: query fileevent handler
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -blocking 0
fileevent $client readable {my handler script}
puts [fileevent $client readable]
close $client
close $srv
