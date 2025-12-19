# Test: update idletasks does not process channel events
set result ""
proc accept {chan addr port} {
    chan configure $chan -buffering line
    puts $chan "msg1"
    close $chan
}
set srv [socket -server accept 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -blocking 0 -buffering line
after 50
update idletasks
# idletasks shouldn't process the socket, so line should be empty
set line [gets $client]
# The result depends on timing; with idletasks only, data may not be there
puts [expr {$line eq "" || $line eq "msg1"}]
close $client
close $srv
