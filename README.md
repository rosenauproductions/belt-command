# Belt Command

Mine asteroids, power your network, and hold the belt against alien waves. A vector-style mining and defense game for 1 to 4 players in split screen. All art is drawn in code and all sound is synthesized, so the repo has no third-party assets.

Copyright © 2026 Rosenau Productions. All rights reserved (see `LICENSE`). Third-party code and its licenses: `THIRD_PARTY_NOTICES.md`. Privacy: `PRIVACY.md` (also served at `/privacy.html`).

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

## Browser version

The same code builds for the web with Emscripten (SDL2 for WebGL, WebAudio and the browser gamepad API):

```
source /path/to/emsdk/emsdk_env.sh
./web/build.sh        # -> web/dist (index.html, index.js, index.wasm, icon.png)
```

`web/dist` is committed, so Vercel serves it as static files with no build step (project root directory `web/dist`). Keyboard + mouse (or touch) is player 1; gamepads join as on Batocera. Settings are kept in the browser's localStorage. Sound starts after the first click or tap (browser rule).

Phones and tablets: landscape only (portrait shows a "turn your phone" screen), full screen on the first tap where the browser allows it. iPhone Safari never allows a page to go full screen, so on iPhone the start screen explains Add to Home Screen (the home screen app runs full screen) and offers PLAY IN SAFARI INSTEAD. The game keeps clear of the notch and the home bar. Touch controls are under Controls. PeerJS (`web/peerjs.min.js`, MIT, version 1.5.5) is copied into `web/dist` by the build for browser-to-browser play.

Wii U: see `docs/WIIU_PLAN.md` (on hold).

## Install on Batocera

Copy to the share (`/userdata/roms/ports`):

- `build/beltcommand.x86_64` to `ports/beltcommand/beltcommand.x86_64`
- `batocera/BeltCommand.sh` and `batocera/BeltCommand.sh.keys` to `ports/`
- `batocera/images/*.png` to `ports/images/`
- the `<game>` block in `batocera/gamelist-entry.xml` into `ports/gamelist.xml` (back it up first; edit with EmulationStation closed)

For online play also copy `web/dist/index.html`, `index.js`, `index.wasm`, `icon.png`, `peerjs.min.js` to `ports/beltcommand/web/`, and Cloudflare's `cloudflared-linux-amd64` to `ports/beltcommand/cloudflared` (internet play only).

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

Keyboard + mouse: the mouse points (no edge scrolling), WASD or arrows scroll the map, Space or left-click to build, click a build icon to choose it, Q/E or 1-9, 0 to choose, U upgrade, hold R recycle, Backspace or right-click ping, wheel or -/= zoom, Tab map, Esc or P pause (or the PAUSE button). In full screen the browser keeps Esc for itself, so leaving full screen (or switching tabs) pauses the game.

Touch (phones, tablets):

| Touch | Action |
|---|---|
| Drag | Move the map |
| Pinch | Zoom |
| Tap a build icon | Choose a structure |
| Tap the map | Place a ghost there; tap it again (or BUILD) to build, CANCEL to drop it |
| Tap a structure | UPGRADE / RECYCLE buttons (ATTACK on a rival structure in raid), CLOSE |
| Tap the minimap | Look there |
| MAP / PAUSE buttons | Full map, pause |

Menus work by tapping. The lobby has + CPU / - CPU buttons.

## Online play

The Batocera box hosts; anyone can join from a browser (the Vercel site or the page the box serves itself) or from another box running Belt Command. A browser (phone, tablet, computer) can host too, with an invite link. Couch players on the host and online players share the same game, up to 4 seats in total.

| On the host (the box) | |
|---|---|
| Same Wi-Fi | The lobby shows `http://<box-ip>:8095` and a QR code. Friends open it in a browser and type a name. |
| Internet | Press BACK (Tab) in the lobby. The box starts `cloudflared` (next to the game) and shows a code plus a QR for `https://belt-command.vercel.app/#join=<code>`. Press BACK again to close the link. |
| Seats | Online players take free seats in the lobby and show as `NAME` / `ONLINE`. Only the host changes the mode, adds CPUs and starts. Guests who arrive mid-game watch until the next round. A guest who drops mid-game is played by a CPU and gets the seat back on reconnecting. |

| On a browser host | |
|---|---|
| Invite | PLAY opens the lobby, and the lobby opens a room: it shows a QR code, a link (`belt-command.vercel.app/#room=CODE`) and a 5-letter room code. SHARE INVITE (top right) opens the phone's share sheet or copies the link. Friends open the link, or JOIN GAME > ENTER CODE. The room closes when the host leaves to the title screen. |
| Keep it open | A phone host has to keep the game on screen: if the browser goes to the background it stops running the game and guests freeze (the game also pauses). |

| On a guest | |
|---|---|
| Browser | Open the link (or JOIN GAME on belt-command.vercel.app and type the code). Keyboard + mouse or a gamepad; the guest plays full screen. START opens LEAVE GAME. |
| Another box | Title > JOIN GAME lists games on the same network, or ENTER CODE OR ADDRESS for an internet code or an IP. |

How it works: the host runs the only real game. Box hosts use WebSocket; browser hosts use WebRTC data channels through PeerJS (the free PeerJS cloud server introduces the browsers, and PeerJS's STUN/TURN servers help them connect; the game data then goes browser to browser). Room codes only work in the browser version; a box can't join a browser's room. Guests send their buttons, cursor and selected structure about 30 times a second; the host sends snapshots (structures that changed, everything that moves, teams, links) and effect events (explosions, beams, sounds) about 15 times a second, about 3-17 KB/s per guest in tests. The box serves the browser build from `ports/beltcommand/web/`. Internet guests connect through a Cloudflare quick tunnel (no account needed); a box joining over the internet uses the system's OpenSSL for the secure link. Port 8095 (TCP) for the game, 8096 (UDP) for the LAN game list. Host and guests must run the same version (`NET_PROTO` in `src/net.inc`).

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
| `BC_JOIN=host:port\|code\|wss://...`, `BC_NAME=x` | Native build joins that game as a guest at start |
| `BC_HOSTWAIT=n`, `BC_HOSTCPU=1` | Host starts the game from the lobby once n online players joined; the CPU plays their seats |
| `BC_GUESTBOT=1` | A guest builds something near its command every second |
| `BC_PORT=n`, `BC_NOSERVER=1`, `BC_TUNNEL=1`, `BC_HOME=dir` | Game port, no server (for a second copy on one machine), open the internet link at start, game/settings folder |

Example: `BC_MUTE=1 SDL_AUDIODRIVER=dummy BC_WINDOW=1920x1080 BC_STATE=game BC_MODE=endless BC_BOTS=1 BC_PLAYERS=4 BC_ADVANCE=300 BC_SHOT=10 BC_SHOT_PATH=shot.bmp xvfb-run -a -s "-screen 0 1920x1080x24" build/bc_test`

## Status

- 2 Nov 2026: getting ready to sell. Removed every reference to other games from the repo, added `LICENSE` (all rights reserved), `THIRD_PARTY_NOTICES.md` (SDL2 zlib, PeerJS and Emscripten MIT), `PRIVACY.md` and `/privacy.html`, a copyright line on the title screen, credits on HOW TO PLAY, and PRIVACY / LICENSES links on the web start page. Older commits in this private repo still mention the inspiration; squash the history before ever making the repo public.

- 1 Nov 2026 (later): iPhone. The full-screen button can't work in iPhone Safari (Apple doesn't allow it), so it is hidden there and the start screen shows the Add to Home Screen steps, with a PLAY IN SAFARI option and the room code for anyone arriving by invite link (the home screen app opens without the link, so they type the code). Added a web app manifest and home screen icon, and the game now stays inside the iPhone's safe area (first try sized it to the full width, which pushed it off center on Chris's iPhone 12; fixed the same evening).

- 1 Nov 2026: phones and tablets. Touch controls (drag, pinch, tap to place then tap again to build, structure buttons, MAP and PAUSE), a bigger HUD on phones, landscape only, full screen on the first tap. Browsers can host with an invite link (`#room=CODE`, WebRTC through PeerJS). On-screen PAUSE button, and the game pauses when the browser leaves full screen (where Esc never reaches the game) or the tab is hidden. Online protocol is now 2: the box and the website must both be updated. Tested in headless Chromium at phone size (844x390, touch): a browser host with a phone guest, and a phone guest on a box host. Not yet tried on real phones, or over mobile data.

- 28 Oct 2026: online multiplayer. The box hosts (LAN link + QR, internet code through a Cloudflare quick tunnel); browsers and other boxes join, with names in the lobby, guest screens that follow the host, drop-and-rejoin, and a JOIN GAME menu with a LAN game list and an on-screen keyboard. Tested in the cloud: box host with native guests (plain and TLS) and browser guests in headless Chromium, a 3-minute 4-player soak, reconnect, and the pad input test (100 of 100). Not yet tried: the Cloudflare tunnel itself (the cloud workspace can't reach it), and real play on the box.

- 26 Oct 2026: browser version live at https://belt-command.vercel.app. Mouse points (no edge scrolling), WASD scrolls, mouse works in menus. A multipath (web) network was tried and reverted: it made bases too hard to cut off and raids stalled. Online multiplayer is planned in `docs/ONLINE_MULTIPLAYER_HANDOVER.md`.

- 26 Oct 2026: fixed pad buttons that often did not register. Presses were read once per drawn frame but used by the 60 Hz game step, so a press on a frame with no game step was dropped (and on slow frames one press counted twice or three times). Presses are now caught from SDL events, held until a game step uses them, and used exactly once. Virtual-pad test, 200 taps: old code 77 used at 144 fps and 600 at 20 fps; new code 200 at every frame rate.

- 25 Oct 2026: added the Carrier, Research (crystal) and the Raid mode, plus glowing mining beams and flow-based link glow. Bot soak: raid games with 2-4 CPUs mostly end in 13-19 minutes (a few evenly matched 2- and 4-CPU games ran past 50); the alien modes play about as before. Not yet tried by people.

- Deployed to ARCADE-21 on 24 Oct 2026; first run on the box still to check (renderer, 4-player frame rate at 4K, Switch Pro pads).
- In bot soak tests, missions 4 (The Swarm) and 6 (Mothership) are the hardest; balance after real play.
- Idea for later: Wii remote pointer cursors using the direct-connect driver from Dead Center / Controller Lab.
