# Test: dict set - create variable if not exists
unset -nocomplain newvar
dict set newvar key value
puts $newvar
