# Test: rename with special characters in name
proc {my proc} {} { return "special" }
rename {my proc} normalname
puts [normalname]
