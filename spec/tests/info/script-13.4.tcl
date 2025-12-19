# Test: info script - nested proc still returns script
proc getScript {} {
    return [info script]
}
puts [expr {[getScript] ne ""}]
