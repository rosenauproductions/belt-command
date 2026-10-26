# Belt Command

Mine asteroids, power your network, and hold the belt against alien waves. A vector-style mining and defense game for 1 to 4 players in split screen, inspired by the 2009 Flash game *The Space Game*. All art is drawn in code and all sound is synthesized, so the repo has no third-party assets.

Written in C++17 with SDL2. Runs on Chris's Batocera box (ARCADE-21) as a port.

## Modes

Pick the mode in the lobby with left/right.

| Mode | What happens |
|---|---|
| Co-op missions | One shared base and stockpile against alien waves. Seven missions. |
| Co-op endless | One shared base. The waves never stop; survive as long as you can. |
| Versus | Everyone has their own base, the aliens still come, and turrets fire on rivals too. Last command standing, or first to mine 5000. |
| Raid | No aliens. Your friends are the rivals, each with their own base in a corner. Rival bases stay hidden until your network (or your fighters) gets close. Build carriers and send fighters to destroy their command. Last command standing wins. Fighter weapons get stronger as the game goes on (x2 at 10 minutes), so a raid always ends. |

With one human player, versus and raid add a CPU rival automatically. X / Y in the lobby add or remove more CPUs.

## Structures

| # | Structure | Cost | Notes |
|---|---|---|---|
| 1 | Relay | 25 | Long power links. |
| 2 | Solar | 40 | Makes energy. |
| 3 | Battery | 60 | Stores energy. |
| 4 | Miner | 70 | Mines an asteroid in range. |
| 5 | Laser | 80 | Fast beam turret, uses energy. |
| 6 | Missile | 140 | Long-range homing, uses minerals. |
| 7 | Repair | 110 | Heals nearby structures. |
| 8 | Shield | 160 | Absorbs fire nearby. |
| 9 | Carrier | 180 | Builds fighters (3 / 4 / 5 by level, 20 minerals each). Fighters guard the carrier against aliens and rival fighters. In versus and raid they wait until the squadron is full, then launch together at rival structures (guns and research first). In raid they only go after bases you have found. Point at a rival structure and press B (ping) to send every fighter at it. |
| 0 | Research | 150 | Slowly studies the belt (6 minutes at level 1, faster with upgrades; uses energy the whole time, and progress is lost if it is destroyed). When it finishes your team discovers **crystal**: the first discovery refills every asteroid, including mined-out ones, with crystal. Only teams with research done can see and mine crystal, and it is worth 1.5x. |

Network links glow brighter and thicker the more energy and minerals pass through them.

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

`dist-beta/beltcommand_beta.x86_64` is the latest test build, committed so it can be copied to the box without building. Install it next to the main binary (or in place of it) only once it has been tried.

The launcher logs to `/userdata/system/logs/beltcommand.log`. Settings are saved in `ports/beltcommand/beltcommand.cfg`.

## Controls

| Controller | Action |
|---|---|
| Left stick / D-pad | Move cursor |
| LB / RB | Choose structure |
| A | Build |
| X | Upgrade (to level 3) |
| Hold Y | Recycle for minerals |
| B | Ping a spot for teammates (in raid, on a rival structure: send your fighters at it) |
| Triggers / right stick | Zoom |
| Hold Back | Full map |
| Start | Pause |
| Hotkey + Start | Quit to EmulationStation |

Lobby: A joins each controller, left/right picks the mode, X/Y add or remove CPU players, Start begins. Buttons are positional, so Switch Pro-style pads work the same way and show Nintendo letters on screen.

Keyboard: WASD or mouse to move, Space or left-click to build, Q/E or 1-9, 0 to choose, U upgrade, hold R recycle, Backspace or right-click ping, wheel or -/= zoom, Tab map, Esc pause.

## Testing without a screen

Run under `xvfb-run` with these environment variables:

| Variable | Effect |
|---|---|
| `BC_STATE=title\|help\|options\|lobby\|missions\|game\|icon\|logo` | Start in that screen |
| `BC_MODE=coop\|endless\|versus\|raid`, `BC_MISSION=n` | Mode and mission (0-6) |
| `BC_PLAYERS=n`, `BC_CPUS=n` | Human seats and extra CPU players |
| `BC_BOTS=1` | CPU drives the human seats too |
| `BC_ADVANCE=secs` | Simulate this many seconds before the first frame |
| `BC_SEED=n` | Fixed world seed |
| `BC_PADTEST=n`, `BC_DT=secs` | Plug in a virtual pad, tap RB n times at a fixed frame time, print how many taps the game used (input regression test) |
| `BC_CAMFIGHT=1`, `BC_ZOOM=z` | After `BC_ADVANCE`, point the views at a fighter raid on that player, and set the zoom |
| `BC_SHOT=frame`, `BC_SHOT_PATH=file.bmp` | Save a screenshot and exit |
| `BC_FRAMES=n` | Print a frame-rate benchmark and exit |
| `BC_WINDOW=WxH`, `BC_RES=0-4`, `BC_GLOW=0\|1`, `BC_MUTE=1` | Window, render resolution, glow, sound |

Example: `BC_MUTE=1 SDL_AUDIODRIVER=dummy BC_WINDOW=1920x1080 BC_STATE=game BC_MODE=endless BC_BOTS=1 BC_PLAYERS=4 BC_ADVANCE=300 BC_SHOT=10 BC_SHOT_PATH=shot.bmp xvfb-run -a -s "-screen 0 1920x1080x24" build/bc_test`

## Status

- 26 Oct 2026: fixed pad buttons that often did not register. Presses were read once per drawn frame but used by the 60 Hz game step, so a press on a frame with no game step was dropped (and on slow frames one press counted twice or three times). Presses are now caught from SDL events, held until a game step uses them, and used exactly once. Virtual-pad test, 200 taps: old code 77 used at 144 fps and 600 at 20 fps; new code 200 at every frame rate.

- 25 Oct 2026: added the Carrier, Research (crystal) and the Raid mode, plus glowing mining beams and flow-based link glow. Bot soak: raid games with 2-4 CPUs mostly end in 13-19 minutes (a few evenly matched 2- and 4-CPU games ran past 50); the alien modes play about as before. Not yet tried by people.

- Deployed to ARCADE-21 on 24 Oct 2026; first run on the box still to check (renderer, 4-player frame rate at 4K, Switch Pro pads).
- In bot soak tests, missions 4 (The Swarm) and 6 (Mothership) are the hardest; balance after real play.
- Idea for later: Wii remote pointer cursors using the direct-connect driver from Dead Center / Controller Lab.
