# Test: chan configure -peername on client socket
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
set peer [chan configure $client -peername]
puts [expr {[llength $peer] == 3}]
close $client
close $srv
