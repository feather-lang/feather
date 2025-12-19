# Test: update processes pending events
set result ""
proc accept {chan addr port} {
    chan configure $chan -buffering line
    puts $chan "msg1"
    flush $chan
    close $chan
}
set srv [socket -server accept 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -blocking 0 -buffering line
after 50
update
set line [gets $client]
puts [expr {$line eq "msg1"}]
close $client
close $srv
