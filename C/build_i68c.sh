#!/bin/bash
# Stop the script if any command fails
set -e

echo "=== CachyOS Cross-Compiler Build Script ==="
echo "Target: i686-elf"

# 1. Install prerequisites (Requires sudo)
echo "[1/5] Installing dependencies via pacman..."
sudo pacman -S --needed base-devel gmp mpfr libmpc gcc14 curl tar

# 2. Setup Environment Variables
export TARGET="i686-elf"
export PREFIX="$HOME/opt/cross"
export PATH="$PREFIX/bin:$PATH"

# Force the use of gcc14 to avoid CachyOS GCC 16+ macro conflicts
export CC="gcc-14"
export CXX="g++-14"

# Set versions
BINUTILS_VERSION="2.42"
GCC_VERSION="14.2.0"
WORKSPACE="$HOME/src/cross"

# Create directories
mkdir -p "$WORKSPACE"
mkdir -p "$PREFIX"
cd "$WORKSPACE"

# 3. Download and Extract Sources
echo "[2/5] Downloading sources..."
if [ ! -f "binutils-$BINUTILS_VERSION.tar.gz" ]; then
    curl -O "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz"
fi
if [ ! -d "binutils-$BINUTILS_VERSION" ]; then
    tar -xzf "binutils-$BINUTILS_VERSION.tar.gz"
fi

if [ ! -f "gcc-$GCC_VERSION.tar.gz" ]; then
    curl -O "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz"
fi
if [ ! -d "gcc-$GCC_VERSION" ]; then
    tar -xzf "gcc-$GCC_VERSION.tar.gz"
fi

# 4. Build Binutils
echo "[3/5] Building Binutils..."
mkdir -p build-binutils
cd build-binutils
# Clean directory in case of previous failed runs
rm -rf *
../binutils-$BINUTILS_VERSION/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make -j$(nproc)
make install
cd "$WORKSPACE"

# 5. Build GCC
echo "[4/5] Building GCC..."
mkdir -p build-gcc
cd build-gcc
rm -rf *
../gcc-$GCC_VERSION/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c --without-headers
make all-gcc -j$(nproc)
make all-target-libgcc -j$(nproc)
make install-gcc
make install-target-libgcc

echo "[5/5] Success! Cross-compiler built successfully."
echo "---------------------------------------------------"
echo "To use your new compiler, ensure the following is in your ~/.bashrc or ~/.zshrc:"
echo 'export PATH="$HOME/opt/cross/bin:$PATH"'
echo "Verify it works by running: i686-elf-gcc --version"
