#!/bin/bash
# Build the browser version with Emscripten -> web/dist (static files: deploy the folder as-is, e.g. to Vercel).
# Needs emsdk activated (source emsdk_env.sh) so em++ is on the PATH.
# If GitHub zip downloads are blocked, clone SDL (tag release-2.32.10) and set EMCC_LOCAL_PORTS="sdl2=/path/to/SDL".
set -e
cd "$(dirname "$0")/.."
mkdir -p web/dist
em++ -std=c++17 -O2 -Wno-misleading-indentation src/main.cpp -o web/dist/index.html \
    -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web -sEXPORTED_RUNTIME_METHODS=FS,UTF8ToString \
    --shell-file web/shell.html
cp art/icon.png web/dist/icon.png
ls -la web/dist
