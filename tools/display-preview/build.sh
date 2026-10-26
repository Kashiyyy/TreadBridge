#!/usr/bin/env bash
# Builds the real src/ui.cpp for the PC, renders every display page and writes
# docs/images/display-portrait.png and display-landscape.png.
# Needs: g++, curl, python3 with Pillow (pip install pillow).
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build && cd build
cp ../stubs/*.h . 2>/dev/null || true
cp ../../../include/config.example.h config.h
# classic 5x7 font used by Adafruit_GFX / Arduino_GFX (downloaded, not stored in the repo)
[ -f glcdfont.c ] || curl -sL https://raw.githubusercontent.com/adafruit/Adafruit-GFX-Library/master/glcdfont.c -o glcdfont.c
g++ -std=c++17 -O1 -I. -I../stubs -I../../../include -I../../../src -o render ../render.cpp
./render
python3 ../make_images.py ../../../docs/images
