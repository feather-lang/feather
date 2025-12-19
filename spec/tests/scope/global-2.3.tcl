# Test: global in nested proc
set x 100
proc outer {} {
    proc inner {} {
        global x
        set x 999
    }
    inner
}
outer
puts "x = $x"
