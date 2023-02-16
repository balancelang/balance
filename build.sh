set -e
mkdir -p _build
cd _build
cmake .. -D CMAKE_BUILD_TYPE=Debug
make -j8



# cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
# cmake .. -DCMAKE_BUILD_TYPE=Debug -G Ninja
# ninja