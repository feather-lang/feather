# Test: non-blocking read returns empty when no data
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -blocking 0
after 10
update
set data [read $client]
puts [expr {$data eq ""}]
close $client
close $srv
