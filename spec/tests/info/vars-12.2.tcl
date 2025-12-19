# Test: info vars - in procedure shows local vars
proc testvars {} {
    set localvar1 1
    set localvar2 2
    puts [lsort [info vars localvar*]]
}
testvars
