# Test: upvar returns empty string
proc test {name} {
    set result [upvar 1 $name var]
    puts "upvar returned: '$result'"
}
set x 10
test x
