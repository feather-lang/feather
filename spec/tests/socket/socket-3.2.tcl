# Test: socket -myport option
set srv [socket -server {apply {{chan addr port} {}}} 0]
set port [lindex [chan configure $srv -sockname] 2]
catch {socket -myport 0 localhost $port} client
puts [string match sock* $client]
catch {close $client}
close $srv
