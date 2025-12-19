# Test: upvar with array element
set arr(key) "original"
proc modify {name} {
    upvar 1 $name var
    set var "modified"
}
modify arr(key)
puts "arr(key) = $arr(key)"
