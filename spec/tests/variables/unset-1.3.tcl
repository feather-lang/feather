# Test: unset -nocomplain doesn't error on missing var
unset -nocomplain nonexistent
set x 1
unset -nocomplain x
puts [info exists x]
