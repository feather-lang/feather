# Test: fileevent readable on socket
set done 0
proc accept {chan addr port} {
    chan configure $chan -buffering line
    puts $chan "hello"
    close $chan
}
set srv [socket -server accept 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket localhost $port]
chan configure $client -blocking 0 -buffering line
fileevent $client readable {
    set ::line [gets $client]
    set ::done 1
}
after 500 {set ::done 1}
vwait done
puts $line
close $client
close $srv
