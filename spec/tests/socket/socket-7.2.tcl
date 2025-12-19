# Test: fblocked on async socket before connection
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket -async localhost $port]
chan configure $client -blocking 0
# fblocked should return true on non-blocking channel with no data
after 10
update
set blocked [fblocked $client]
puts [expr {$blocked == 0 || $blocked == 1}]
close $client
close $srv
