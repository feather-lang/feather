# Test: info vars - includes linked global vars
set ::gvar 100
proc testvars {} {
    global gvar
    set localvar 1
    puts [expr {"gvar" in [info vars]}]
    puts [expr {"localvar" in [info vars]}]
}
testvars
