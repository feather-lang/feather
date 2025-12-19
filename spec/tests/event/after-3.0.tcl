# Test: after idle script - schedule idle callback
set id [after idle {set x done}]
puts [string match "after#*" $id]
