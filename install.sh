#!/bin/sh

LICENSES_DIR="/usr/local/share/licenses/clapper"
DOC_DIR="/usr/local/share/doc/clapper"
MAIN_DIR="/usr/local/share/clapper"
BIN_DIR="/usr/local/bin"
GJS_DIR="/usr/local/share/gjs-1.0"

SCRIPT_ERR="Error: this script must be"

if [ ! -d "./clapper_src" ]; then
    echo "$SCRIPT_ERR run from clapper directory!" 1>&2
    exit 1
elif [ "$EUID" -ne 0 ]; then
    echo "$SCRIPT_ERR run as root!" 1>&2
    exit 1
fi

echo "Creating directories..."
mkdir -p "$LICENSES_DIR"
mkdir -p "$DOC_DIR"
mkdir -p "$MAIN_DIR"
mkdir -p "$GJS_DIR"
mkdir -p "$BIN_DIR"

echo "Copying files..."
cp -f "./COPYING" "$LICENSES_DIR/"
cp -f "./README.md" "$DOC_DIR/"
cp -rf "./clapper_src" "$MAIN_DIR/"
cp -f "./main.js" "$MAIN_DIR/"
cp -f "./gjs-1.0/clapper.js" "$GJS_DIR/"
cp -f "./bin/clapper" "$BIN_DIR/"

echo "Creating executables..."
chmod +x "$BIN_DIR/clapper"

echo "Install finished"
