# Test: eof detection on closed socket
set done 0
proc accept {chan addr port} {
    close $chan
}
set srv [socket -server accept 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -blocking 0
fileevent $client readable {
    if {[eof $client]} {
        set ::done 1
    }
}
after 500 {set ::done 1}
vwait done
puts [eof $client]
close $client
close $srv
