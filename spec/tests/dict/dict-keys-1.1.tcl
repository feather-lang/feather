# Test: dict keys - with pattern
set d [dict create apple 1 banana 2 apricot 3 cherry 4]
puts [dict keys $d]
puts [dict keys $d a*]
puts [dict keys $d *a*]
