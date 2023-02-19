# remove exit and cd to clone directory
exit 1
git clone https://github.com/llvm/llvm-project.git
cd llvm-project
git checkout llvmorg-12.0.1
mkdir build
cd build

# Debug
cmake -G Ninja -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_ASSERTIONS=On -DLLVM_OPTIMIZED_TABLEGEN=ON -DLLVM_TARGETS_TO_BUILD=host -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_DOCS=OFF -DLLVM_ENABLE_PROJECTS='clang' ../llvm
# Optionally with -DLLVM_PARALLEL_LINK_JOBS=2

cmake --build .
cmake --build . --target install



# Release
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=Off -DLLVM_TARGETS_TO_BUILD=host -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_DOCS=OFF -DLLVM_ENABLE_PROJECTS='clang' ../llvm/
cmake --build .
cmake --build . --target install