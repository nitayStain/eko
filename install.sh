#!/bin/sh
set -e

ZIG_VERSION="0.15.2"
PREFIX="${PREFIX:-/usr/local}"
BINDIR="$PREFIX/bin"

# Detect OS and arch
OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
ARCH="$(uname -m)"

case "$ARCH" in
    x86_64|amd64) ARCH="x86_64" ;;
    aarch64|arm64) ARCH="aarch64" ;;
    *) echo "Unsupported architecture: $ARCH"; exit 1 ;;
esac

case "$OS" in
    linux)  ZIG_OS="linux" ;;
    darwin) ZIG_OS="macos" ;;
    *)      echo "Unsupported OS: $OS"; exit 1 ;;
esac

ZIG_TARBALL="zig-${ZIG_OS}-${ARCH}-${ZIG_VERSION}.tar.xz"
ZIG_URL="https://ziglang.org/download/${ZIG_VERSION}/${ZIG_TARBALL}"

# Check if zig is already available and correct version
if command -v zig >/dev/null 2>&1; then
    CURRENT="$(zig version 2>/dev/null || true)"
    if [ "$CURRENT" = "$ZIG_VERSION" ]; then
        echo "Using system zig $ZIG_VERSION"
        ZIG=zig
    fi
fi

# Download zig if needed
if [ -z "$ZIG" ]; then
    TMPDIR="$(mktemp -d)"
    trap 'rm -rf "$TMPDIR"' EXIT

    echo "Downloading zig ${ZIG_VERSION}..."
    if command -v curl >/dev/null 2>&1; then
        curl -fSL "$ZIG_URL" -o "$TMPDIR/$ZIG_TARBALL"
    elif command -v wget >/dev/null 2>&1; then
        wget -q "$ZIG_URL" -O "$TMPDIR/$ZIG_TARBALL"
    else
        echo "Error: curl or wget required"; exit 1
    fi

    echo "Extracting..."
    tar -xf "$TMPDIR/$ZIG_TARBALL" -C "$TMPDIR"
    ZIG="$TMPDIR/zig-${ZIG_OS}-${ARCH}-${ZIG_VERSION}/zig"
fi

echo "Building eko..."
"$ZIG" build -Doptimize=ReleaseFast

echo "Installing to ${BINDIR}/eko..."
SUDO=""
if ! touch "$BINDIR/.eko_write_test" 2>/dev/null; then
    SUDO="sudo"
fi
rm -f "$BINDIR/.eko_write_test" 2>/dev/null
$SUDO mkdir -p "$BINDIR"
$SUDO cp zig-out/bin/eko "$BINDIR/eko"
$SUDO chmod 755 "$BINDIR/eko"

if [ ! -f "$HOME/.ekorc" ]; then
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    if [ -f "$SCRIPT_DIR/ekorc.example" ]; then
        cp "$SCRIPT_DIR/ekorc.example" "$HOME/.ekorc"
        echo "Created ~/.ekorc with default settings"
    fi
fi

echo "Done! eko installed to ${BINDIR}/eko"
