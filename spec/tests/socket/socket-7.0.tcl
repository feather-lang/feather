# Test: socket -async creates channel immediately (before connection completes)
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket -async localhost $port]
# Channel exists immediately even if not connected yet
puts [string match sock* $client]
close $client
close $srv
