# Test: dict values - with pattern
set d [dict create a apple b banana c cherry]
puts [dict values $d]
puts [dict values $d a*]
puts [dict values $d *a*]
