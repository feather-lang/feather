# Test: -connecting option on async socket
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket -async localhost $port]
# -connecting may be 1 or 0 depending on how fast connection completes
set connecting [chan configure $client -connecting]
puts [expr {$connecting == 0 || $connecting == 1}]
close $client
close $srv
