# Test: regsub -all with back-references
puts [regsub -all {(\w+)} "hello world foo" {<\1>}]
