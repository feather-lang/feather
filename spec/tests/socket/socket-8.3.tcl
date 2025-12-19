# Test: clear fileevent handler
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -blocking 0
fileevent $client readable {puts "data available"}
fileevent $client readable {}
# Empty script means no handler
puts [expr {[fileevent $client readable] eq ""}]
close $client
close $srv
