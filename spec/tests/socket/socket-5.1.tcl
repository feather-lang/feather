# Test: bidirectional socket communication
proc accept {chan addr port} {
    chan configure $chan -buffering line
    set msg [gets $chan]
    puts $chan "echo: $msg"
    close $chan
}
set srv [socket -server accept 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -buffering line
puts $client "test message"
flush $client
after 100
update
set response [gets $client]
puts $response
close $client
close $srv
