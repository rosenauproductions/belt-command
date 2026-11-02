// Belt Command - touch screens (phones, tablets): state shared by drawing and input. Logic is in touch.inc.
// Drag pans, pinch zooms, tap a build icon to pick, tap the map to place a ghost, then tap it again (or BUILD) to build.
// Tap a structure for UPGRADE / RECYCLE (or ATTACK on a rival in raid). MAP and PAUSE buttons on the left.
enum { TA_NONE, TA_BUILD, TA_CANCEL, TA_UPGRADE, TA_RECYCLE, TA_ATTACK, TA_MAP, TA_PAUSE };
struct TFinger { SDL_FingerID id; V2 start, pos, last; Uint32 t0; bool moved; };
static struct {
    bool on = false;             // a finger touched the screen (a real mouse switches back)
    float dpr = 1;               // render pixels per CSS pixel (phones: 2-3)
    int mode = 0; V2 cur;        // 0 nothing, 1 ghost placed at cur, 2 structure at cur selected, 3 an action at cur waiting for the next game step
    bool log = false;
    bool mapOn = false, zoomSet = false;
    std::vector<TFinger> f; float pinchD = 0; V2 pinchMid; bool pinched = false;
    struct Btn { float x, y, w, h; int act; };
    std::vector<Btn> btns; int btnFrame = -9;
    float mx = 0, my = 0, mw = 0, mh = 0; bool mapBig = false; int mapFrame = -9; // where the map was drawn (tap to look there)
    float hudX = 0;              // left edge of the touch button column (the hover panel sits left of it)
} TC;
static bool touchPlayerIs(const Player& p);
static void drawTouchUi(Canvas& cv, Player& p, float u, float barY);
static int touchButtonAt(V2 s);
static void touchAction(Player& p, int act);
static void touchCursor(Player& p);
static void touchHover(Player& p);
static float zoomMax() { return TC.on ? 3.5f : 1.7f; }
