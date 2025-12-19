# Test: throw caught by catch
if {[catch {throw {MYERR} "oops"}]} {
    puts "caught"
}
