#!/bin/bash
# Build the browser version with Emscripten -> web/dist (static files: deploy the folder as-is, e.g. to Vercel).
# Needs emsdk activated (source emsdk_env.sh) so em++ is on the PATH.
# If GitHub zip downloads are blocked, clone SDL (the tag your Emscripten's tools/ports/sdl2.py names, e.g. release-2.32.10
# for emsdk 4.x or release-2.0.20 for Ubuntu's emscripten 3.1.6 package) and set EMCC_LOCAL_PORTS="sdl2=/path/to/SDL".
# Ubuntu's package freezes its cache: copy /usr/share/emscripten/.emscripten, set FROZEN_CACHE = False and NODE_JS to your
# node, and point EM_CONFIG at the copy.
set -e
cd "$(dirname "$0")/.."
mkdir -p web/dist
em++ -std=c++17 -O2 -Wno-misleading-indentation src/main.cpp -o web/dist/index.html \
    -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web -sEXPORTED_RUNTIME_METHODS=FS,UTF8ToString \
    -sEXPORTED_FUNCTIONS=_main,_bc_main -sMINIFY_HTML=0 -lwebsocket.js \
    --shell-file web/shell.html
# stamp this build into the URLs the page asks for (index.js, index.wasm), so browsers never mix an old cached copy with a new one
V=$(date +%Y%m%d%H%M%S)
sed -i -e "s/__BUILD__/$V/g" -e "s/src=index.js>/src=index.js?v=$V>/; s/src=\"index.js\"/src=\"index.js?v=$V\"/" web/dist/index.html
grep -q "index.js?v=$V" web/dist/index.html || { echo "build stamp missing in index.html"; exit 1; }
cp art/icon.png web/dist/icon.png
ls -la web/dist
