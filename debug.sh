mkdir -p builds &&
make version=debug debug=1 && make clean &&
gdb
