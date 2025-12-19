# Test: file tempfile creates file
set chan [file tempfile fname]
puts [file exists $fname]
close $chan
file delete $fname
