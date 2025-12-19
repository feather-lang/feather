# Test: error on file size non-existing
catch {file size /tmp/nonexistent-xyz-123.txt} err
puts [expr {$err ne ""}]
