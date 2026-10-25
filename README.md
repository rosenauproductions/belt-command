# Belt Command

Mine asteroids, power your network, and hold the belt against alien waves. A vector-style mining and defense game for 1 to 4 players in split screen, inspired by the 2009 Flash game *The Space Game*. All art is drawn in code and all sound is synthesized, so the repo has no third-party assets.

Written in C++17 with SDL2. Runs on Chris's Batocera box (ARCADE-21) as a port.

## Layout

| Path | What it is |
|---|---|
| `src/main.cpp` | Data tables, entities, world generation, power network. Includes the other files. |
| `src/sim.inc` | Simulation: structures, enemies, waves, CPU players, win/lose. |
| `src/render.inc` | Drawing: world, structures, enemies, HUD, minimap, split screen. |
| `src/ui.inc` | Input, menus, lobby, options, test switches, main loop. |
| `src/gfx.h` | Glow lines, polygons, the stroke font. |
| `src/audio.h` | Synthesized sound effects and the ambient music loop. |
| `batocera/` | Launcher, hotkey file, gamelist entry and EmulationStation images. |
| `art/` | Icon, logo and a screenshot, all rendered by the game itself. |

## Build

```
apt-get install -y libsdl2-dev
./build.sh           # Batocera release: build/beltcommand.x86_64
./build.sh native    # quick local build: build/bc_test
```

The release build uses zig as the C++ compiler, targets glibc 2.28 and links the box's own `libSDL2-2.0.so.0` (needs SDL 2.0.18 or newer).

## Install on Batocera

Copy to the share (`/userdata/roms/ports`):

- `build/beltcommand.x86_64` to `ports/beltcommand/beltcommand.x86_64`
- `batocera/BeltCommand.sh` and `batocera/BeltCommand.sh.keys` to `ports/`
- `batocera/images/*.png` to `ports/images/`
- the `<game>` block in `batocera/gamelist-entry.xml` into `ports/gamelist.xml` (back it up first; edit with EmulationStation closed)

The launcher logs to `/userdata/system/logs/beltcommand.log`. Settings are saved in `ports/beltcommand/beltcommand.cfg`.

## Controls

| Controller | Action |
|---|---|
| Left stick / D-pad | Move cursor |
| LB / RB | Choose structure |
| A | Build |
| X | Upgrade (to level 3) |
| Hold Y | Recycle for minerals |
| B | Ping a spot for teammates |
| Triggers / right stick | Zoom |
| Hold Back | Full map |
| Start | Pause |
| Hotkey + Start | Quit to EmulationStation |

Lobby: A joins each controller, left/right picks the mode, X/Y add or remove CPU players, Start begins. Buttons are positional, so Switch Pro-style pads work the same way and show Nintendo letters on screen.

Keyboard: WASD or mouse to move, Space or left-click to build, Q/E or 1-8 to choose, U upgrade, hold R recycle, Backspace or right-click ping, wheel or -/= zoom, Tab map, Esc pause.

## Testing without a screen

Run under `xvfb-run` with these environment variables:

| Variable | Effect |
|---|---|
| `BC_STATE=title\|help\|options\|lobby\|missions\|game\|icon\|logo` | Start in that screen |
| `BC_MODE=coop\|endless\|versus`, `BC_MISSION=n` | Mode and mission (0-6) |
| `BC_PLAYERS=n`, `BC_CPUS=n` | Human seats and extra CPU players |
| `BC_BOTS=1` | CPU drives the human seats too |
| `BC_ADVANCE=secs` | Simulate this many seconds before the first frame |
| `BC_SEED=n` | Fixed world seed |
| `BC_SHOT=frame`, `BC_SHOT_PATH=file.bmp` | Save a screenshot and exit |
| `BC_FRAMES=n` | Print a frame-rate benchmark and exit |
| `BC_WINDOW=WxH`, `BC_RES=0-4`, `BC_GLOW=0\|1`, `BC_MUTE=1` | Window, render resolution, glow, sound |

Example: `BC_MUTE=1 SDL_AUDIODRIVER=dummy BC_WINDOW=1920x1080 BC_STATE=game BC_MODE=endless BC_BOTS=1 BC_PLAYERS=4 BC_ADVANCE=300 BC_SHOT=10 BC_SHOT_PATH=shot.bmp xvfb-run -a -s "-screen 0 1920x1080x24" build/bc_test`

## Status

- Deployed to ARCADE-21 on 24 Oct 2026; first run on the box still to check (renderer, 4-player frame rate at 4K, Switch Pro pads).
- In bot soak tests, missions 4 (The Swarm) and 6 (Mothership) are the hardest; balance after real play.
- Idea for later: Wii remote pointer cursors using the direct-connect driver from Dead Center / Controller Lab.
