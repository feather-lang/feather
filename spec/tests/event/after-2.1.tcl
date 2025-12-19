# Test: after ms script - script is concatenated
set id [after 100 set x done]
puts [string match "after#*" $id]
