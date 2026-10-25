#!/bin/bash
# Build Belt Command.
#   ./build.sh          release binary for Batocera (x86_64, glibc 2.28) -> build/beltcommand.x86_64
#   ./build.sh native   quick native build for testing in this machine   -> build/bc_test
set -e
cd "$(dirname "$0")"
mkdir -p build

if [ "$1" = "native" ]; then
    g++ -std=c++17 -O2 -Wno-misleading-indentation $(sdl2-config --cflags) src/main.cpp -o build/bc_test $(sdl2-config --libs)
    echo "built build/bc_test"
    exit 0
fi

# Needs: pip install ziglang ; apt-get install libsdl2-dev
python3 -c "import ziglang" 2>/dev/null || pip install --break-system-packages ziglang
[ -f /usr/include/SDL2/SDL.h ] || { echo "install libsdl2-dev first"; exit 1; }
# Debian/Ubuntu split SDL_config.h into an arch folder; zig needs it on the include path
mkdir -p build/sdlinc/SDL2
cp /usr/include/x86_64-linux-gnu/SDL2/_real_SDL_config.h build/sdlinc/SDL2/ 2>/dev/null || true
python3 -m ziglang c++ -target x86_64-linux-gnu.2.28 -O2 -std=c++17 -Wno-misleading-indentation \
    -I/usr/include/SDL2 -Ibuild/sdlinc -D_REENTRANT src/main.cpp \
    /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0 -o build/beltcommand.x86_64
md5sum build/beltcommand.x86_64
