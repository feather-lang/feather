# Test: rename a builtin command
rename puts myputs
myputs "hello from myputs"
rename myputs puts
