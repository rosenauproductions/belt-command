# Online multiplayer: handover

Written 26 Oct 2026 by the cloud Claude session that built the Carrier, Research, Raid mode, input fixes and the
browser port. Chris wants his Mac Claude (Claude Desktop) to implement online play from this document.
Nothing here is built yet. Read `README.md` and `CLAUDE.md` first.

## What Chris asked for (his words, tidied)

- Multiplayer is **local and internet**. Couch players (pads / Wii remotes, split screen on the TV) and online
  players can be in the same game.
- **The Batocera box must take part**, so people can "combat via a browser" against the box and its couch players.
- The **host shares a URL**. Friends open it in a browser and join.
- **The joined player's name shows on the host's screen** (lobby slot, e.g. `P3 SAM (ONLINE)`).
- **Joined players see the same screens** (lobby, game, results) **but are obviously not the host**: only the host
  changes the mode, adds/removes CPUs and presses START; guests see "WAITING FOR HOST".
- Chris approved putting **`cloudflared`** on the Batocera box for internet play (step 3 below).
- Hosting from a plain browser (no Batocera, e.g. belt-command.vercel.app) is wanted later, not first.

## Recommended architecture

**Host-authoritative.** The host runs the only real simulation (the code that exists now). Guests send input;
the host sends state snapshots; guests draw them with their own camera. Do **not** try lockstep: the shared RNG `R`
is used by effects and even by drawing (`viewOf()` screen shake calls `R.range`, particles use `R`), so two
machines would drift apart.

```
 Batocera box (host)                                 Browser guest (web build, guest mode)
 ┌───────────────────────────────┐                  ┌────────────────────────────────────┐
 │ game sim (stepWorld)          │  ws  "in" 30/s   │ reads its own mouse/keys/pad       │
 │ couch players (split screen)  │ <──────────────  │ sends buttons + cursor + zoom      │
 │ tiny HTTP+WebSocket server    │  ──────────────> │ applies snapshots, draws its own   │
 │  :8080 serves web/dist too    │  "snap" 10-15/s  │ full-screen view, own HUD          │
 └───────────────────────────────┘  + events        └────────────────────────────────────┘
        │ step 3: cloudflared quick tunnel -> https://xyz.trycloudflare.com (wss to :8080)
```

### Step 1 - net layer inside the game (biggest part)

1. **Remote devices.** Add a device kind for network players, like Light Cycle Arena's `DEV_WEB`. Belt Command's
   devices are `Dev` (`src/main.cpp`): give a remote `Dev` an id from a new range (e.g. `2000 + slot`), `gc = nullptr`,
   and fill `b[]`, `mv`, `tl/tr`, `hit[]` from network messages instead of SDL in `readDevs()` (`src/ui.inc`). Because
   presses already go through the latch (`hit[]` -> `pend[]` -> `pr()`, used exactly once per game step, see
   `usePresses()`), remote taps will not be lost or doubled. Keep that.
   - A remote player behaves like the keyboard+mouse player (it has a pointer): send the cursor as a **world
     position** each input message and apply it in `updatePlayer()` (copy the `d->id == -1` branch: WASD scrolls the
     camera, the pointer sets `p.cur`; clicking a build-bar slot selects, `barSlotAt()`).
2. **Players and views.** `Player` (`src/main.cpp`) already has everything per player (cursor, camera, zoom, sel,
   toast, team). Add `bool remote` and `std::string name`. Remote players are `joined`, have `view = false` on the
   host (they are not split on the TV; Light Cycle does the same), and `team` set by `startGame()` as now.
   `layoutViews()` only lays out `view` players, so the TV keeps its split screen for couch players.
3. **Snapshot (host -> guest).** Serialize what drawing needs, nothing else. A compact binary format is best
   (little-endian, quantized: positions as int16 in world units /2 or float16, angles as uint8). Per message:
   - header: game time, mode, mission index, state (lobby/game/result), result + winner + message,
     waveN/waveT/alertT, world size, `crystalOut`, motherUid;
   - teams (<= 4): used, alive, minerals, energy, ecap, eprod, drainRate, mined, kills, found bits, crystal,
     col index, order;
   - structures: index, type, team, lv, x, y, hp/maxhp, built, flags (connected, powered, mining, cryBeam),
     aim, charge, ast, beamT, flash. **Send only changed ones after the first full snapshot** (most structures don't
     move), plus a "dead" list;
   - links per team (pairs of structure indices) only when the network changed (`Team::dirty` / recompute count);
   - enemies, fighters, bullets/missiles (type, x, y, angle, hp fraction);
   - asteroids: send the whole list once at game start (shape vertices and flecks are random, so send them or
     send the world seed and regenerate with the same code); afterwards only `minerals`, `crystal`, `dead`;
   - beams and warnings (short lists);
   - per guest: its own player block (toast text, hover id, sel, yHold) - or let the guest compute hover locally.
   - Effects: do **not** sync particles. Send small **events** instead (explode at x,y size colour; sound id at x,y;
     ring; shake) and let the guest spawn its own particles and sounds. Hook them in `explode()`, `sfxAt()`,
     `ring()`, `sparks()`, `shakeAt()` in `src/main.cpp`: when hosting, also append an event to an outgoing buffer.
   - Target 10-15 snapshots/s. Guests **interpolate** positions of enemies/fighters/bullets between the last two
     snapshots (they move every frame). Budget: a 4-player raid has ~600 structures, ~60 fighters, ~250 enemies in
     endless; with deltas this stays well under 20 KB/s per guest.
4. **Fog of war.** Raid hides rival bases (`seen()`, `inSight()`, `fighterSeen()`). Easiest: send everything and let
   the guest's renderer filter exactly as now (drawing already takes the viewer's team). Cheating is not a concern
   for family play. Later, filter on the host if wanted.
5. **Guest mode in the client.** The web build (and the native build, for testing) gets a guest mode:
   no `stepWorld()`; `frame()` reads local input, sends it, applies incoming snapshots/events, runs `updateFx()`
   for local particles, and draws with `renderGame()` using **one full-screen view for the local player**
   (set that player's `vp` to the whole screen). Menus: show the host's lobby (all slots with names, mode) read-only
   with "WAITING FOR HOST"; the guest can only change its own name and leave. Results/pause come from the host state.
6. **Lobby on the host.** Remote players appear in the four slots like pads (`joinPlayer()`), labelled with their name
   and `ONLINE`. Max 4 players total (couch + online + CPU) - the game is built around 4 teams/colours. Host's
   START begins for everyone. If a guest disconnects mid-game, hand their seat to a CPU (`p.cpu = true`) like
   `devLost` does for pads, and give it back if they reconnect with the same token.

### Step 2 - Batocera hosts on the home network

- Reuse Light Cycle Arena's server: `rosenauproductions/light-cycle-arena` `src/web.cpp` / `src/web.h`
  (non-blocking sockets, polled once per frame, HTTP + WebSocket with its own SHA-1/base64, no threads, no
  libraries) and `src/qr.cpp` / `src/qr.h` (QR encoder; the light-cycle menus draw it). It also shows how it finds
  the LAN address for the URL (`url()`) and how phone players become devices. Strip the light-cycle specifics.
- Serve `web/dist` (index.html, index.js, index.wasm, icon.png) from the game folder on the box, e.g.
  `/userdata/roms/ports/beltcommand/web/`. Correct content types: `.wasm` must be `application/wasm`.
- The page decides host vs guest by the URL: `http://<box>:8080/#join` (served by the box) means "be a guest of
  this server" and connects to `ws://<same host>:8080/ws`. The Vercel page has no `#join` and plays locally.
- The Batocera lobby shows `JOIN: http://192.168.x.x:8080` and a QR code.
- Use a port other than 8080 if Light Cycle Arena might run at the same time (it uses 8080).

### Step 3 - internet play with a Cloudflare quick tunnel

- Ship the `cloudflared` linux-amd64 binary in the game folder (Chris approved). When the host picks
  **HOST ONLINE**, start it as a child process: `cloudflared tunnel --url http://localhost:<port> --no-autoupdate`,
  read its stderr until a line contains `https://...trycloudflare.com`, and show that URL + QR in the lobby.
  Kill the process when leaving the lobby / quitting. No account needed. The tunnel carries WebSocket upgrades,
  so guests connect with `wss://xyz.trycloudflare.com/ws` (the page must use `wss:` when loaded over `https:`).
- The Vercel page could also accept `https://belt-command.vercel.app/#join=<tunnel-host>` and connect to
  `wss://<tunnel-host>/ws` - nicer links, same thing.

### Step 4 (later) - hosting from a browser

A browser cannot listen for connections, so this needs WebRTC (host browser <-> guests) with a signalling service
(Supabase Realtime is connected to Chris's account; PeerJS's public server is the no-setup option), or a relay.
The same snapshot/input protocol works over a WebRTC data channel.

## Where things are (Belt Command)

| What | Where |
|---|---|
| Data tables, entities (`Struct`, `Enemy`, `Fighter`, `Bullet`, `Team`, `Dev`, `Player`, `Game G`), world gen, network (`recomputeNet`) | `src/main.cpp` |
| Simulation: `stepWorld()`, `updateStructs/Enemies/Fighters/Bullets/Fx/Waves`, `cpuThink()`, `checkEnd()`, `startGame()`, `layoutViews()` | `src/sim.inc` |
| Drawing: `renderGame()`, `drawWorld(cv, view, me)`, `drawHud()`, minimap, build bar (`barOf`, `barSlotAt`) | `src/render.inc` |
| Input (`readDevs`, `keyHit`, `padButton`, press latch `pr`/`usePresses`), `updatePlayer()`, menus + lobby, `frame()`, `main()`, test switches | `src/ui.inc` |
| Effects/sound helpers that should also emit net events: `explode`, `sparks`, `ring`, `shakeAt`, `sfxAt` | `src/main.cpp` |
| Web shell (cover page, keys, full screen, cache-busting) and web build script | `web/shell.html`, `web/build.sh` |

## Testing without the box

- Native build hosts: `./build.sh native` then run `build/bc_test` (add a `BC_HOST=1` / `BC_JOIN=ws://...` switch).
- Guests: the web build in headless Chromium via Playwright (pre-installed in Claude cloud sessions; on the Mac
  install `playwright`), several pages at once, each joining with a different name. The cloud session used scripts
  that click the cover, join, start, click build icons and take screenshots - copy that approach.
- Existing switches still work (`BC_STATE`, `BC_MODE`, `BC_PLAYERS`, `BC_BOTS`, `BC_ADVANCE`, `BC_SHOT`,
  `BC_PADTEST`, `BC_DT`, `BC_CAMFIGHT`, `BC_ZOOM`). Keep `BC_PADTEST` passing (input regression test).

## Build + deploy notes

- Web: Emscripten (`source emsdk_env.sh`; `./web/build.sh` -> `web/dist`, committed). If GitHub zip downloads are
  blocked, clone SDL `release-2.32.10` and set `EMCC_LOCAL_PORTS="sdl2=/path/to/SDL"`. The build stamps
  `index.js?v=` / `index.wasm?v=` so browsers never mix cached versions.
- Vercel: project `belt-command` in team RosenauProductions (`team_M281dwym4rgiMTX09vVUtovg`), root directory
  `web/dist`, no build step. Production follows `main`. Work so far is on branch `claude/clever-turing-rsbgvy`
  and was deployed to production by hand from that branch; merge it to `main` so pushes deploy automatically.
- Batocera: `./build.sh` (zig, glibc 2.28) -> `build/beltcommand.x86_64`; the beta copy is
  `dist-beta/beltcommand_beta.x86_64`. Deploy = copy to `/userdata/roms/ports/beltcommand/` (back up the old one).
  Add `web/dist/*` and `cloudflared` to that folder for online play.

## Decisions already made (don't revisit)

- **Multipath network was tried and reverted** (26 Oct): it made bases too hard to cut off and raids stalled for over
  an hour. Links stay a single tree from Command (Prim in `recomputeNet()`).
- Mouse: points only, **no edge scrolling**; WASD / arrows scroll. Mouse works in menus. Keep it that way for guests.
- Raid: no aliens, rival bases hidden until found, carrier fighters, fighter power escalates (x2 at 10 min);
  research takes 6 minutes at level 1 on purpose.
- Wii U port is on hold (`docs/WIIU_PLAN.md`).
