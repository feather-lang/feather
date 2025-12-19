# Test: fileevent with async socket connection
set done 0
set connected 0
proc accept {chan addr port} {
    chan configure $chan -buffering line
    puts $chan "welcome"
    close $chan
}
set srv [socket -server accept 0]
set port [lindex [chan configure $srv -sockname] 2]
set client [socket -async localhost $port]
chan configure $client -blocking 0 -buffering line
fileevent $client writable {
    if {![chan configure $client -connecting]} {
        set ::connected 1
        fileevent $client writable {}
    }
}
fileevent $client readable {
    if {[eof $client]} {
        set ::done 1
    } else {
        set ::msg [gets $client]
        if {$::msg ne ""} {set ::done 1}
    }
}
after 500 {set ::done 1}
vwait done
puts $connected
close $client
close $srv
