FROM ubuntu:20.04

WORKDIR /balance

ENV DEBIAN_FRONTEND=noninteractive

# cmake and tools
RUN apt update
RUN apt install -y git g++ software-properties-common lsb-release wget zlib1g-dev pkg-config cmake uuid-dev openjdk-11-jre libboost-dev
RUN apt clean all

# LLVM-12
RUN printf "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-12 main" | tee /etc/apt/sources.list.d/llvm-toolchain-xenial-12.list
RUN wget --no-check-certificate -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
RUN apt update
RUN apt install -y llvm-12 libclang-12-dev clang-12

# Antlr
RUN mkdir dependencies
RUN wget https://www.antlr.org/download/antlr-4.10.1-complete.jar
RUN mv antlr-4.10.1-complete.jar dependencies
