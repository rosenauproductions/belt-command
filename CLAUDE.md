# Belt Command: notes for Claude sessions

Chris's asteroid-mining / network-defense game (inspired by The Space Game, 2009) for his Batocera box
(ARCADE-21, x86_64) and the browser (https://belt-command.vercel.app). C++17 + SDL2, no other libraries; all art is
drawn in code and all sound is synthesized. `README.md` has modes, structures, controls, build and test switches.

## Working rules (from Chris)
- New Batocera builds ship as the beta `dist-beta/beltcommand_beta.x86_64`; say the md5. Back up the box copy before replacing it.
- Show screenshots for anything visual before calling it done.
- Keep `README.md` (Status section) up to date in plain language.
- Cloud sessions can't reach the Batocera share; Chris's Claude Desktop deploys to `/userdata/roms/ports/beltcommand/`.

## Build
- Native test build: `./build.sh native` -> `build/bc_test` (needs `libsdl2-dev`).
- Batocera release: `./build.sh` -> `build/beltcommand.x86_64` (zig c++, glibc 2.28, links the box's libSDL2).
- Web: `source emsdk_env.sh && ./web/build.sh` -> `web/dist` (committed; Vercel serves it as-is, no build step).
  If GitHub zip downloads are blocked: clone the SDL tag your Emscripten wants, set `EMCC_LOCAL_PORTS="sdl2=/path/to/SDL"`.
  Without emsdk: Ubuntu's `apt-get install emscripten` (3.1.6) works with a config copy (`FROZEN_CACHE = False`, `NODE_JS` = your node,
  `EM_CONFIG=` that copy) and SDL `release-2.0.20`. That package never exports `main()`, so the page starts the game through
  `bc_main()` (see the end of `src/ui.inc` and `web/shell.html`).

## Test headless
```
xvfb-run -a -s "-screen 0 1920x1080x24" env BC_MUTE=1 SDL_AUDIODRIVER=dummy BC_WINDOW=1280x720 \
  BC_STATE=game BC_MODE=raid BC_BOTS=1 BC_PLAYERS=2 BC_ADVANCE=300 BC_SHOT=3 BC_SHOT_PATH=shot.bmp build/bc_test
```
`BC_PADTEST=100 BC_DT=0.00694` is the input regression test (must report 100 of 100). The web build is tested with
Playwright + headless Chromium (`--use-angle=swiftshader`).

## Deploy (web)
Vercel project `belt-command`, team RosenauProductions, root `web/dist`. Production follows `main`.

## Online play
Built 28 Oct 2026 in `src/net.inc` (host server, snapshots, guest client, LAN list, tunnel) plus join/keyboard screens in `src/ui.inc`.
README "Online play" has how to use it; `docs/ONLINE_MULTIPLAYER_HANDOVER.md` the design. Bump `NET_PROTO` whenever the
snapshot or input format changes, and ship the box build and the Vercel build together (they must match).
Test: native host + `BC_JOIN` guest, or Playwright on `http://127.0.0.1:8095/#join` with the web files in `$BC_HOME/web`.
Wii U port: on hold, `docs/WIIU_PLAN.md`.
