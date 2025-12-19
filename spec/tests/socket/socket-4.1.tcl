# Test: chan configure -sockname on client socket
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
set name [chan configure $client -sockname]
puts [expr {[llength $name] == 3}]
close $client
close $srv
