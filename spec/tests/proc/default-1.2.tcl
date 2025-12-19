# Test: procedure with multiple default arguments
proc config {{host localhost} {port 8080}} {
    puts "Connecting to $host:$port"
}
config
config myserver
config myserver 9000
