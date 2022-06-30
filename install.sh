# cmake
sudo apt update
sudo apt install -y git g++ software-properties-common lsb-release wget zlib1g-dev pkg-config cmake uuid-dev openjdk-11-jre
sudo apt clean all

wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
sudo apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
sudo apt update
sudo apt install kitware-archive-keyring
sudo rm /etc/apt/trusted.gpg.d/kitware.gpg

sudo apt install cmake
sudo apt install uuid-dev
sudo apt install ninja-build


# LLVM-12
printf "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-12 main" |sudo tee /etc/apt/sources.list.d/llvm-toolchain-xenial-12.list
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo apt update
sudo apt install llvm-12 libclang-12-dev clang-12

# Antlr
sudo apt install openjdk-11-jdk
mkdir dependencies
wget https://www.antlr.org/download/antlr-4.10.1-complete.jar
mv antlr-4.10.1-complete.jar dependencies