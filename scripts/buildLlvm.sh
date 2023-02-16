# remove exit and cd to clone directory
exit 1
git clone https://github.com/llvm/llvm-project.git
cd llvm-project
git checkout llvmorg-12.0.1
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_ASSERTIONS=On -DLLVM_ENABLE_PROJECTS='clang' ../llvm/
# Optionally with -DLLVM_PARALLEL_LINK_JOBS=2

cmake --build .
cmake --build . --target install