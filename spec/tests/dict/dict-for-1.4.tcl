# Test: dict for - result is empty string
set d [dict create a 1 b 2]
set result [dict for {k v} $d {
    puts "$k=$v"
}]
puts "result=<$result>"
