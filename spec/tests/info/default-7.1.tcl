# Test: info default - parameter without default value
proc nodef {x y} { return "$x $y" }
puts [info default nodef x defval]
