# Test: non-blocking gets returns -1 when no complete line
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -blocking 0
after 10
update
set result [gets $client line]
puts [expr {$result == -1}]
close $client
close $srv
