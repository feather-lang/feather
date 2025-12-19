# Test: info default - parameter with default value
proc withdef {x {y 42}} { return "$x $y" }
puts [info default withdef y defval]
puts $defval
