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
  If GitHub zip downloads are blocked: clone SDL tag `release-2.32.10`, set `EMCC_LOCAL_PORTS="sdl2=/path/to/SDL"`.

## Test headless
```
xvfb-run -a -s "-screen 0 1920x1080x24" env BC_MUTE=1 SDL_AUDIODRIVER=dummy BC_WINDOW=1280x720 \
  BC_STATE=game BC_MODE=raid BC_BOTS=1 BC_PLAYERS=2 BC_ADVANCE=300 BC_SHOT=3 BC_SHOT_PATH=shot.bmp build/bc_test
```
`BC_PADTEST=100 BC_DT=0.00694` is the input regression test (must report 100 of 100). The web build is tested with
Playwright + headless Chromium (`--use-angle=swiftshader`).

## Deploy (web)
Vercel project `belt-command`, team RosenauProductions, root `web/dist`. Production follows `main`.

## In progress
Online multiplayer (local + internet, Batocera hosting, browser guests): see `docs/ONLINE_MULTIPLAYER_HANDOVER.md`.
Wii U port: on hold, `docs/WIIU_PLAN.md`.
