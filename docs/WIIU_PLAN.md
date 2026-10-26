# Wii U port: plan (on hold)

Decided with Chris on 26 Oct 2026. Not started; do the browser version first.

## Target
- Homebrew Wii U (Aroma), devkitPro + wut + the Wii U SDL2 port, packaged as `.wuhb`.
- Check first: does the Wii U SDL2 port support `SDL_RenderGeometry` (all our drawing uses it) and USB keyboards (nsyskbd)? If not, add a GX2 path for the triangle batches.
- CPU is slow: be ready to trim particles/glow for 4-player split screen.

## One player: the GamePad is a mouse pad
- The GamePad touchscreen moves the cursor (like a laptop touchpad, relative movement).
- **A tap is NOT a click.** Touch only moves the cursor, so there are no false builds.
- Clicking and the other actions are on the **left-hand buttons and the left stick** (so the right hand stays on the screen):
  left stick also moves the cursor / scrolls, D-pad and L/ZL do build / upgrade / recycle / ping (exact mapping to decide when building it).
- The **GamePad screen shows the build icons along its bottom**; touching an icon there selects that structure (selecting is fine on tap, building is not).
- The TV shows the game full screen.
- USB keyboard works as on PC (WASD, Space, Q/E, 1-0, U, R, Tab, Esc).

## Multiplayer (2-4)
- **Wii remotes only.** The GamePad is switched off (not used as a controller or second screen).
- Same split screen as Batocera.
