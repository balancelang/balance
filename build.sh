set -e
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j8



# cmake .. -DCMAKE_BUILD_TYPE=Debug -G Ninja
# ninja