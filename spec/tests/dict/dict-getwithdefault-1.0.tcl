# Test: dict getwithdefault - alias for getdef
set d [dict create a 1]
puts [dict getwithdefault $d a default]
puts [dict getwithdefault $d missing default]
