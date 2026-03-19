FROM ubuntu:latest

SHELL ["/bin/bash", "-c"]

RUN apt-get update && apt-get install -y \
    build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev \
    libisl-dev texinfo wget g++ nasm git ca-certificates \
    && rm -rf /var/lib/apt/lists/*

ENV PREFIX="/opt/cross"
ENV TARGET="sh4-elf"
ENV PATH="$PREFIX/bin:$PATH"
ENV SDK_DIR="/workspace/src/cp400/hollyhock-2/sdk"
ENV SRC="/src"

RUN mkdir -p "$PREFIX" "$SRC"
WORKDIR "$SRC"

# Binutils
RUN wget https://ftp.gnu.org/gnu/binutils/binutils-2.35.tar.gz \
    && tar -xzf binutils-2.35.tar.gz \
    && mkdir build-binutils && cd build-binutils \
    && ../binutils-2.35/configure --target=$TARGET --prefix=$PREFIX --with-sysroot --disable-nls --disable-werror \
    && make -j$(nproc) && make install \
    && cd .. && rm -rf build-binutils binutils-2.35 binutils-2.35.tar.gz

# GCC
RUN wget https://ftp.gnu.org/gnu/gcc/gcc-10.2.0/gcc-10.2.0.tar.gz \
    && tar -xzf gcc-10.2.0.tar.gz \
    && cd gcc-10.2.0 && ./contrib/download_prerequisites && cd .. \
    && mkdir build-gcc && cd build-gcc \
    && ../gcc-10.2.0/configure --target=$TARGET --prefix=$PREFIX --disable-nls \
       --enable-languages=c,c++ --without-headers --with-multilib-list=m4-nofpu \
    && make all-gcc -j$(nproc) && make all-target-libgcc -j$(nproc) \
    && make install-gcc install-target-libgcc \
    && cd .. && rm -rf build-gcc gcc-10.2.0 gcc-10.2.0.tar.gz

WORKDIR /workspace
CMD ["/bin/bash"]