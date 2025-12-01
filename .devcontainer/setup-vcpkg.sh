#!/bin/bash
set -e

# Clone vcpkg if it doesn't exist
if [ ! -d "/workspace/vcpkg" ]; then
    echo "[+] Cloning vcpkg..."
    git clone https://github.com/microsoft/vcpkg.git /workspace/vcpkg
    /workspace/vcpkg/bootstrap-vcpkg.sh
fi

# Install dependencies
echo "[+] Installing libraries with vcpkg..."
/workspace/vcpkg/vcpkg install \
    simdjson \
    boost-system \
    boost-filesystem \
    boost-thread \
    boost-beast \
    boost-asio \
    openssl \
    libpq

# Export environment variables for dev container terminals
echo 'export VCPKG_ROOT=/workspace/vcpkg' >> ~/.bashrc
echo 'export CMAKE_TOOLCHAIN_FILE=/workspace/vcpkg/scripts/buildsystems/vcpkg.cmake' >> ~/.bashrc

echo "[✔] vcpkg is set up with simdjson, Boost, and OpenSSL."

QUICKFIX_DIR=/workspace/quickfix

if [ ! -d "$QUICKFIX_DIR" ]; then
  echo "[+] Cloning QuickFIX..."
  git clone https://github.com/quickfix/quickfix.git "$QUICKFIX_DIR"
fi

cd "$QUICKFIX_DIR"

echo "[+] Building QuickFIX..."
# Clean previous builds
make clean || true

# Bootstrap and build quickfix
./bootstrap
./configure --prefix="$QUICKFIX_DIR/build"
sudo make -j$(nproc)
sudo make install

echo "[+] QuickFIX installed at $QUICKFIX_DIR/build"


