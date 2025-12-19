# Test: socket client/server communication
proc accept {chan addr port} {
    chan configure $chan -buffering line
    puts $chan "hello from server"
    close $chan
}
set srv [socket -server accept 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -buffering line
after 100
update
set line [gets $client]
puts $line
close $client
close $srv
