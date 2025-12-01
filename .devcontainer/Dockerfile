# Base image
FROM mcr.microsoft.com/devcontainers/base:ubuntu-22.04

# Install dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    software-properties-common wget gnupg ca-certificates curl \
    python3 python3-dev python3-distutils python3-venv python3-setuptools \
    libiberty-dev libnuma-dev libpq-dev postgresql-server-dev-all \
    build-essential flex bison \
    gcc-11 g++-11 cmake gdb git \
    libelf-dev libdw-dev libaudit-dev \
    libtool libtool-bin autoconf automake \
    libunwind-dev libslang2-dev libperl-dev libzstd-dev \
    libcap-dev libbabeltrace-dev libpfm4-dev libtraceevent-dev \
    pkg-config ninja-build zlib1g-dev libssl-dev \
    && curl -fsSL https://www.postgresql.org/media/keys/ACCC4CF8.asc | gpg --dearmor -o /etc/apt/trusted.gpg.d/postgresql.gpg \
    && echo "deb http://apt.postgresql.org/pub/repos/apt/ jammy-pgdg main" > /etc/apt/sources.list.d/pgdg.list \
    && apt-get update \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace
