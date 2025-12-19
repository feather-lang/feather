# Test: info default - empty string default
proc emptydef {x {y {}}} { return $x }
puts [info default emptydef y defval]
puts "value:$defval:"
