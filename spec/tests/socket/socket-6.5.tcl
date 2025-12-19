# Test: -peername not supported on server socket
set srv [socket -server {apply {{chan addr port} {}}} 0]
catch {chan configure $srv -peername} result
puts [string match "*not supported*" [string tolower $result]]
close $srv
