# Test: chan configure -nodelay on socket
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
if {[catch {chan configure $client -nodelay 1} err]} {
    puts [string match "*not supported*" $err]
} else {
    puts [chan configure $client -nodelay]
}
close $client
close $srv
