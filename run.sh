set -e
cd build
./balance ../src/TestFile.bl $@
time ./main