# Test: after ms script - schedule a timer, returns an ID
set id [after 100 {set x done}]
puts [string match "after#*" $id]
