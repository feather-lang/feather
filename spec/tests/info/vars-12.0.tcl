# Test: info vars - at global level
set myvar1 10
set myvar2 20
puts [expr {"myvar1" in [info vars]}]
puts [expr {"myvar2" in [info vars]}]
