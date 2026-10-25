// Belt Command - mine asteroids, power your network, defend it from alien waves.
// A vector-style asteroid-mining defense game for 1-4 players in split screen (C++17 / SDL2).
#include "gfx.h"
#include "audio.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static Font FONT;
static Audio AU;
static Rng R;

// ---------------------------------------------------------------- data
enum { S_HQ, S_RELAY, S_SOLAR, S_BATTERY, S_MINER, S_LASER, S_MISSILE, S_REPAIR, S_SHIELD, S_CARRIER, S_RESEARCH, S_N };
struct SDef { const char* name; const char* blurb; int cost; float hp, r, link, build; };
static const SDef SD[S_N] = {
    { "COMMAND", "YOUR BASE. IF IT FALLS, YOU LOSE.", 0, 700, 34, 240, 0 },
    { "RELAY", "EXTENDS THE POWER NETWORK FAR.", 25, 60, 10, 300, 2.5f },
    { "SOLAR", "MAKES ENERGY.", 40, 80, 16, 150, 4 },
    { "BATTERY", "STORES MORE ENERGY.", 60, 120, 15, 150, 5 },
    { "MINER", "MINES A NEARBY ASTEROID. USES ENERGY.", 70, 120, 17, 150, 6 },
    { "LASER", "FAST BEAM TURRET. USES ENERGY.", 80, 150, 16, 150, 6 },
    { "MISSILE", "LONG RANGE HOMING. USES MINERALS.", 140, 170, 18, 150, 8 },
    { "REPAIR", "HEALS STRUCTURES NEARBY. USES ENERGY.", 110, 120, 16, 150, 7 },
    { "SHIELD", "ABSORBS FIRE NEARBY. USES ENERGY.", 160, 140, 18, 150, 9 },
    { "CARRIER", "BUILDS FIGHTERS THAT ATTACK FOR YOU. MINERALS PER FIGHTER.", 180, 220, 22, 150, 10 },
    { "RESEARCH", "SLOWLY STUDIES THE BELT FOR A NEW MINERAL. USES ENERGY.", 150, 120, 17, 150, 8 },
};
static const int NBUILD = 10;
static const int BUILDS[NBUILD] = { S_RELAY, S_SOLAR, S_BATTERY, S_MINER, S_LASER, S_MISSILE, S_REPAIR, S_SHIELD, S_CARRIER, S_RESEARCH };
// research: finding crystal refills every asteroid with a second, richer mineral
static float researchTime(int lv) { return 360 / (1 + 0.4f * (lv - 1)); } // slow on purpose: a long-term investment worth defending
static const float CRYSTAL_VALUE = 1.5f;
static float lvm(int lv) { return 1 + 0.45f * (lv - 1); }
static float rangeOf(int t, int lv) {
    switch (t) {
    case S_MINER: return 135;
    case S_LASER: return 225 * (1 + 0.12f * (lv - 1));
    case S_MISSILE: return 430 * (1 + 0.10f * (lv - 1));
    case S_REPAIR: return 175 * (1 + 0.10f * (lv - 1));
    case S_SHIELD: return 140 * (1 + 0.15f * (lv - 1));
    case S_CARRIER: return 650 * (1 + 0.15f * (lv - 1)); // how far from the carrier its fighters defend
    }
    return 0;
}
// carrier fighters
static int carrierCap(int lv) { return 2 + lv; }
static const float FTR_HP = 55, FTR_SPEED = 215, FTR_RANGE = 125, FTR_DMG = 6, FTR_CD = 0.45f, FTR_BUILD = 9, FTR_SIGHT = 300;
static const int FTR_COST = 20;
static const float SIGHT = 420; // raid mode: how far your structures can see
static int upgCost(int t, int lv) { return t == S_HQ ? 250 * lv : std::max(20, (int)(SD[t].cost * 0.8f * lv)); }

enum { E_SCOUT, E_FIGHTER, E_BOMBER, E_SWARM, E_MOTHER, E_N };
struct EDef { const char* name; float hp, speed, r, dmg, range, cd; int score; };
static const EDef ED[E_N] = {
    { "SCOUT", 30, 150, 9, 5, 170, 1.0f, 2 },
    { "FIGHTER", 90, 100, 13, 11, 220, 1.3f, 4 },
    { "BOMBER", 260, 60, 18, 140, 0, 0, 8 },
    { "SWARM", 14, 185, 6, 3, 110, 0.7f, 1 },
    { "MOTHERSHIP", 3000, 24, 62, 20, 340, 0.5f, 100 },
};

enum { OBJ_MINE, OBJ_SURVIVE, OBJ_MOTHER, OBJ_ENDLESS, OBJ_VERSUS, OBJ_RAID };
struct Mission { const char* name; const char* desc; int obj; float target, first, interval, diff, density; bool swarmy; };
static const Mission MISSIONS[] = {
    { "FIRST LIGHT", "MINE 800 MINERALS. LIGHT RESISTANCE.", OBJ_MINE, 800, 50, 45, 0.6f, 1.1f, false },
    { "HOLDOUT", "SURVIVE FOR 4 MINUTES.", OBJ_SURVIVE, 240, 50, 40, 0.9f, 1.0f, false },
    { "DEEP VEIN", "MINE 1500 MINERALS.", OBJ_MINE, 1500, 55, 38, 1.0f, 0.9f, false },
    { "THE SWARM", "SURVIVE 6 MINUTES OF SWARM ATTACKS.", OBJ_SURVIVE, 360, 45, 34, 1.15f, 1.0f, true },
    { "LONG HAUL", "MINE 3000 MINERALS. ASTEROIDS ARE SCARCE.", OBJ_MINE, 3000, 50, 36, 1.15f, 0.6f, false },
    { "MOTHERSHIP", "A MOTHERSHIP ARRIVES AT 3:00. DESTROY IT.", OBJ_MOTHER, 180, 50, 40, 1.0f, 1.0f, false },
    { "LAST STAND", "SURVIVE 8 MINUTES. EVERYTHING COMES.", OBJ_SURVIVE, 480, 40, 30, 1.4f, 1.0f, false },
};
static const int NMISSIONS = sizeof(MISSIONS) / sizeof(MISSIONS[0]);
static const Mission M_ENDLESS = { "ENDLESS", "SURVIVE AS LONG AS YOU CAN.", OBJ_ENDLESS, 0, 50, 36, 1.0f, 1.0f, false };
static const Mission M_RAID = { "RAID", "NO ALIENS. FIND YOUR NEIGHBORS AND DESTROY THEIR COMMANDS.", OBJ_RAID, 0, 1e9f, 1e9f, 0, 1.0f, false };
static const Mission M_VERSUS = { "VERSUS", "DESTROY RIVAL COMMANDS, OR MINE 5000 FIRST.", OBJ_VERSUS, 5000, 100, 45, 0.55f, 1.0f, false };

static const Col PCOL[4] = { Col(40, 220, 255), Col(255, 160, 40), Col(130, 255, 90), Col(255, 90, 210) };
static const Col C_CRY(80, 255, 215), C_MIN(190, 140, 255), C_EN(250, 235, 100), C_ALIEN(255, 70, 70), C_AST(125, 145, 180), C_WHITE(255, 255, 255), C_DIM(120, 135, 160), C_GOOD(110, 255, 140), C_BAD(255, 80, 80);

// ---------------------------------------------------------------- entities
struct Asteroid { V2 p; float r, minerals, maxMin, crystal = 0, rot, spin; std::vector<V2> shape, flecks; bool dead = false; int miners = 0; };
struct Struct {
    int t, team, lv = 1; V2 p; float hp, maxhp, built = 0, cd = 0, charge = 0, flash = 0, invested = 0, doomT = -1, aim = 0, spin = 0, beamT = 0, chk = 0;
    float useAcc = 0, flow = 0; // resources passing through this structure (energy used + made + mined), for the link glow
    bool dead = false, connected = false, powered = false, mining = false, cryBeam = false; int ast = -1; int builder = -1;
    float r() const { return SD[t].r; }
};
struct Enemy { int t, uid; V2 p, v; float hp, maxhp, cd, ang, flash = 0, retarget = 0, spawnCd = 0, orbit = 1; int tgt = -1; bool dead = false; };
struct Fighter { int team, uid, home; V2 p, v; float ang, hp, maxhp, dmg, cd = 0, retarget = 0, flash = 0, orbitA = 0; int tk = 0, tid = -1; bool dead = false, sortie = false; }; // sortie: out raiding with its squadron. tk: 0 none, 1 alien uid, 2 fighter uid, 3 structure index
struct Bullet { V2 p, v; float dmg, life, splash = 0; int team; bool missile = false; int tgtUid = -1, tgtStruct = -1; bool dead = false; };
enum { P_SPARK, P_DOT, P_RING, P_DEBRIS };
struct Part { V2 p, v; float life, max, a, b, rot, spin; Col c; int kind; };
struct Beam { V2 a, b; float life, max, w; Col c; };
struct Warn { V2 p; float t, budget; bool mother; };
struct Ping { V2 p; float t; int player; };
struct Team {
    bool used = false, alive = true; int hq = -1; Col col; float minerals = 0, energy = 0, ecap = 150, eprod = 0, drainAcc = 0, drainRate = 0, mined = 0, lostAt = -1;
    int kills = 0, built = 0, ftrLost = 0; bool crystal = false; unsigned found = 0; int order = -1; float orderT = 0; bool dirty = true; std::vector<std::pair<int, int>> links; std::vector<float> linkFlow;
};

enum { B_A, B_B, B_X, B_Y, B_LB, B_RB, B_START, B_BACK, B_UP, B_DOWN, B_LEFT, B_RIGHT, NB };
struct Dev {
    int id = -1; SDL_GameController* gc = nullptr; std::string name; bool nintendo = false;
    bool b[NB] = {}, pb[NB] = {}; V2 mv; float tl = 0, tr = 0; int navX = 0, navY = 0, nxLast = 0, nyLast = 0; float nxT = 0, nyT = 0;
    bool ml = false, mr = false, mouseMoved = false; V2 mouse; float wheel = 0;
};
struct Player {
    bool joined = false, cpu = false, view = false; int dev = -9; int team = 0, idx = 0; Col col;
    V2 cur, cam; float zoom = 1, shake = 0; int sel = 1, hover = -1; float yHold = 0, toastT = 0; std::string toast; bool bigMap = false; bool devLost = false;
    SDL_Rect vp = { 0, 0, 0, 0 };
    float thinkT = 1, goalWait = 0, exploreT = 20; V2 goal; int goalType = -1, goalStruct = -1; bool hasGoal = false;
};

enum { ST_TITLE, ST_HELP, ST_OPTIONS, ST_LOBBY, ST_MISSIONS, ST_GAME, ST_ICON, ST_LOGO };
enum { MODE_COOP, MODE_ENDLESS, MODE_VERSUS, MODE_RAID, MODE_N };
static const char* MODENAMES[MODE_N] = { "CO-OP MISSIONS", "CO-OP ENDLESS", "VERSUS", "RAID" };
static const char* RESNAMES[5] = { "AUTO", "1280 X 720", "1920 X 1080", "2560 X 1440", "3840 X 2160" };
static const int RESH[5] = { 0, 720, 1080, 1440, 2160 };

struct Opts { int res = 0; bool glow = true, shake = true; int sfx = 8, mus = 5; unsigned done = 0; int bestEndless = 0; };

struct Game {
    SDL_Window* win = nullptr; SDL_Renderer* ren = nullptr; SDL_Texture* target = nullptr;
    int outW = 1280, outH = 720, W = 1280, H = 720; bool useTarget = false;
    Canvas cv; Opts opt; std::string cfgPath;
    int state = ST_TITLE, menuSel = 0, mode = MODE_COOP, mission = 0; float stateT = 0;
    std::vector<Dev> devs; Player pl[4];
    // world
    Mission M; float WW = 3600, WH = 2400; float time = 0, waveT = 0, slow = 1; int waveN = 0, uidNext = 1, motherUid = -1; bool motherWarned = false, motherSpawned = false;
    std::vector<Asteroid> ast; std::vector<Struct> st; std::vector<Enemy> en; std::vector<Bullet> bl; std::vector<Part> parts; std::vector<Beam> beams; std::vector<Warn> warns; std::vector<Ping> pings; std::vector<Fighter> fi; float seeT = 0; bool crystalOut = false;
    Team teams[4]; int nTeams = 1;
    bool paused = false; int pauseSel = 0; int pauser = -1;
    int result = 0; float resultT = 0; int winner = -1; int resultSel = 0; std::string resultMsg;
    bool quit = false; float alertT = 0;
    // test switches
    int shotFrame = -1; std::string shotPath = "shot.bmp"; bool bots = false; int fast = 1; int frame = 0;
} G;
static bool rivals() { return G.mode == MODE_VERSUS || G.mode == MODE_RAID; } // everyone has their own base
static bool raid() { return G.mode == MODE_RAID; }
// raid escalation: fighter weapons get stronger as the game goes on (x2 at 10 minutes), so a raid always ends
static float ftrPower() { return raid() ? 1 + G.time / 600.f : 1; }
// Raid hides rival bases until you find them
static bool seen(int viewerTeam, int team) { return !raid() || viewerTeam < 0 || team == viewerTeam || (G.teams[viewerTeam].found >> team & 1); }

// ---------------------------------------------------------------- config
static void saveCfg() {
    FILE* f = fopen(G.cfgPath.c_str(), "w"); if (!f) return;
    fprintf(f, "res=%d\nglow=%d\nshake=%d\nsfx=%d\nmusic=%d\ndone=%u\nbest_endless=%d\n", G.opt.res, G.opt.glow, G.opt.shake, G.opt.sfx, G.opt.mus, G.opt.done, G.opt.bestEndless);
    fclose(f);
}
static void loadCfg() {
    FILE* f = fopen(G.cfgPath.c_str(), "r"); if (!f) return;
    char k[64]; long v;
    while (fscanf(f, " %63[^=]=%ld", k, &v) == 2) {
        std::string s = k;
        if (s == "res") G.opt.res = (int)clampf((float)v, 0, 4); else if (s == "glow") G.opt.glow = v != 0; else if (s == "shake") G.opt.shake = v != 0;
        else if (s == "sfx") G.opt.sfx = (int)clampf((float)v, 0, 10); else if (s == "music") G.opt.mus = (int)clampf((float)v, 0, 10);
        else if (s == "done") G.opt.done = (unsigned)v; else if (s == "best_endless") G.opt.bestEndless = (int)v;
    }
    fclose(f);
}
static void applyAudioVol() { AU.sfxVol = G.opt.sfx / 10.f; AU.musVol = G.opt.mus / 10.f * 0.9f; }

static void setupTarget() {
    if (G.target) { SDL_DestroyTexture(G.target); G.target = nullptr; }
    SDL_GetRendererOutputSize(G.ren, &G.outW, &G.outH);
    int h = RESH[G.opt.res];
    if (h == 0 || h == G.outH) { G.useTarget = false; G.W = G.outW; G.H = G.outH; }
    else {
        G.H = h; G.W = (int)(h * (float)G.outW / G.outH + 0.5f);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        G.target = SDL_CreateTexture(G.ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, G.W, G.H);
        G.useTarget = G.target != nullptr; if (!G.useTarget) { G.W = G.outW; G.H = G.outH; }
    }
    G.cv.px = G.H / 1080.f;
    printf("video: output %dx%d, render %dx%d\n", G.outW, G.outH, G.W, G.H);
}

// ---------------------------------------------------------------- effects
static void addPart(int kind, V2 p, V2 v, float life, Col c, float a = 0, float b = 0) {
    if (G.parts.size() > 5000) return;
    Part q; q.kind = kind; q.p = p; q.v = v; q.life = q.max = life; q.c = c; q.a = a; q.b = b; q.rot = R.range(0, 6.28f); q.spin = R.range(-6, 6); G.parts.push_back(q);
}
static void sparks(V2 p, int n, Col c, float spd) { for (int i = 0; i < n; i++) addPart(P_SPARK, p, fromAng(R.range(0, 6.28f)) * R.range(spd * 0.3f, spd), R.range(0.2f, 0.6f), c); }
static void ring(V2 p, float r0, float r1, Col c, float life) { addPart(P_RING, p, V2(), life, c, r0, r1); }
static void shakeAt(V2 p, float amt) {
    if (!G.opt.shake) return;
    for (auto& q : G.pl) if (q.joined && q.view) { float d = dist(q.cam, p); if (d < 1400) q.shake = std::max(q.shake, amt * (1 - d / 1400)); }
}
static void sfxAt(int s, V2 p, float vol = 1, Uint32 gap = 30) {
    float best = 1e9, pan = 0;
    for (auto& q : G.pl) if (q.joined && q.view) { float d = dist(q.cam, p); if (d < best) { best = d; pan = clampf((p.x - q.cam.x) / 1200, -0.6f, 0.6f); } }
    float att = best > 1e8 ? 0.6f : clampf(1 - (best - 500) / 1700, 0.07f, 1);
    AU.play(s, vol * att, pan, R.range(0.94f, 1.06f), gap);
}
static void explode(V2 p, float size, Col c) {
    ring(p, size * 0.3f, size * 2.2f, C_WHITE, 0.35f);
    ring(p, size * 0.2f, size * 3.2f, c, 0.6f);
    sparks(p, 8 + (int)(size * 0.6f), c, 120 + size * 6);
    sparks(p, 4 + (int)(size * 0.2f), C_WHITE, 90 + size * 4);
    addPart(P_DOT, p, V2(), 0.25f, c, size * 2.5f);
    for (int i = 0; i < 3 + (int)(size / 10); i++) addPart(P_DEBRIS, p, fromAng(R.range(0, 6.28f)) * R.range(20, 60 + size * 2), R.range(0.6f, 1.4f), c, R.range(3, 4 + size * 0.25f));
}
static void toast(Player& p, const std::string& s) { p.toast = s; p.toastT = 1.8f; }

// ---------------------------------------------------------------- world
static Asteroid makeAst(V2 p, float r, float rich) {
    Asteroid a; a.p = p; a.r = r; a.maxMin = a.minerals = r * r * 0.22f * rich; a.rot = R.range(0, 6.28f); a.spin = R.range(-0.12f, 0.12f);
    int n = 9 + R.i(5);
    for (int i = 0; i < n; i++) a.shape.push_back(fromAng((i + R.range(-0.3f, 0.3f)) * 2 * PI / n) * R.range(0.78f, 1.12f));
    int nf = 3 + (int)(r / 10);
    for (int i = 0; i < nf; i++) a.flecks.push_back(fromAng(R.range(0, 6.28f)) * R.range(0.1f, 0.62f));
    return a;
}
static bool astFree(V2 p, float r, float pad) {
    for (auto& a : G.ast) if (dist(a.p, p) < a.r + r + pad) return false;
    for (int i = 0; i < 4; i++) if (G.teams[i].used && G.teams[i].hq >= 0 && dist(G.st[G.teams[i].hq].p, p) < r + 130) return false;
    return true;
}
static int addStruct(int t, int team, V2 p, float built) {
    Struct s; s.t = t; s.team = team; s.p = p; s.maxhp = SD[t].hp; s.built = built; s.hp = built >= 1 ? s.maxhp : s.maxhp * 0.1f; s.invested = (float)SD[t].cost;
    s.aim = R.range(0, 6.28f);
    G.st.push_back(s); G.teams[team].dirty = true;
    return (int)G.st.size() - 1;
}

static void genWorld() {
    G.ast.clear();
    if (!rivals()) {
        int n = 0; for (auto& q : G.pl) if (q.joined) n++;
        G.WW = 3400 + 500.f * (n - 1); G.WH = 2300 + 300.f * (n - 1);
        V2 hqp(G.WW / 2, G.WH / 2);
        G.teams[0].hq = addStruct(S_HQ, 0, hqp, 1);
        for (int i = 0; i < 3; i++) { // guaranteed starter asteroids
            for (int tries = 0; tries < 50; tries++) {
                float r = R.range(34, 48); V2 p = hqp + fromAng(i * 2.1f + R.range(-0.5f, 0.5f)) * R.range(230, 300);
                if (astFree(p, r, 30)) { G.ast.push_back(makeAst(p, r, 1.0f)); break; }
            }
        }
        int target = (int)(G.WW * G.WH / 105000 * G.M.density);
        std::vector<V2> clusters; for (int i = 0; i < 7; i++) clusters.push_back(V2(R.range(250, G.WW - 250), R.range(250, G.WH - 250)));
        for (int tries = 0; tries < target * 30 && (int)G.ast.size() < target; tries++) {
            V2 p; if (R.f() < 0.65f) { V2 c = clusters[R.i((int)clusters.size())]; p = c + fromAng(R.range(0, 6.28f)) * R.range(0, 380); }
            else p = V2(R.range(120, G.WW - 120), R.range(120, G.WH - 120));
            float r = R.range(24, 85) * (R.f() < 0.15f ? 1.25f : 1);
            if (p.x < r + 60 || p.y < r + 60 || p.x > G.WW - r - 60 || p.y > G.WH - r - 60) continue;
            if (!astFree(p, r, 40)) continue;
            float rich = 1 + dist(p, hqp) / 1600;
            G.ast.push_back(makeAst(p, r, rich));
        }
    } else {
        G.WW = 4600; G.WH = 3000;
        V2 pos[4] = { V2(G.WW * 0.17f, G.WH * 0.2f), V2(G.WW * 0.83f, G.WH * 0.8f), V2(G.WW * 0.83f, G.WH * 0.2f), V2(G.WW * 0.17f, G.WH * 0.8f) };
        auto mir = [&](V2 p, int q) { return V2(q & 1 ? G.WW - p.x : p.x, q & 2 ? G.WH - p.y : p.y); };
        for (int t = 0; t < G.nTeams; t++) G.teams[t].hq = addStruct(S_HQ, t, pos[t], 1);
        std::vector<Asteroid> quad;
        auto okQ = [&](V2 p, float r) {
            if (p.x < r + 60 || p.y < r + 60 || p.x > G.WW / 2 - r - 30 || p.y > G.WH / 2 - r - 30) return false;
            for (auto& a : quad) if (dist(a.p, p) < a.r + r + 40) return false;
            if (dist(p, pos[0]) < r + 130) return false; return true;
        };
        for (int i = 0; i < 3; i++) for (int tries = 0; tries < 60; tries++) {
            float r = R.range(34, 48); V2 p = pos[0] + fromAng(i * 2.1f + 0.4f + R.range(-0.5f, 0.5f)) * R.range(230, 300);
            if (okQ(p, r)) { quad.push_back(makeAst(p, r, 1)); break; }
        }
        for (int tries = 0; tries < 2000 && quad.size() < 34; tries++) {
            V2 p(R.range(80, G.WW / 2), R.range(80, G.WH / 2)); float r = R.range(24, 80);
            bool centre = R.f() < 0.2f; if (centre) { p = V2(G.WW / 2, G.WH / 2) - V2(R.range(120, 520), R.range(120, 420)); r = R.range(45, 85); }
            if (!okQ(p, r)) continue;
            quad.push_back(makeAst(p, r, centre ? 2.2f : 1 + dist(p, pos[0]) / 1800));
        }
        for (int q = 0; q < 4; q++) for (auto a : quad) { a.p = mir(a.p, q); if (q & 1) for (auto& s : a.shape) s.x = -s.x; G.ast.push_back(a); }
    }
}

// ---------------------------------------------------------------- network
static float linkDist(int ta, int tb) { return std::max(SD[ta].link, SD[tb].link); }
static void recomputeNet(int team) {
    Team& T = G.teams[team]; T.dirty = false; T.links.clear();
    std::vector<int> ids; for (int i = 0; i < (int)G.st.size(); i++) if (!G.st[i].dead && G.st[i].team == team) { ids.push_back(i); G.st[i].connected = false; }
    if (T.hq < 0 || G.st[T.hq].dead) { for (int i : ids) G.st[i].powered = false; return; }
    // Prim from HQ over valid links gives both connectivity and a tidy tree to draw
    int n = (int)ids.size(); std::vector<float> best(n, 1e18f); std::vector<int> par(n, -1); std::vector<char> in(n, 0);
    int h = -1; for (int k = 0; k < n; k++) if (ids[k] == T.hq) h = k;
    best[h] = 0;
    for (;;) {
        int u = -1; float bu = 1e17f;
        for (int k = 0; k < n; k++) if (!in[k] && best[k] < bu) { bu = best[k]; u = k; }
        if (u < 0) break;
        in[u] = 1; Struct& su = G.st[ids[u]]; su.connected = true;
        if (par[u] >= 0) T.links.push_back({ ids[par[u]], ids[u] });
        for (int k = 0; k < n; k++) if (!in[k]) {
            Struct& sk = G.st[ids[k]]; float d = dist(su.p, sk.p);
            if (d <= linkDist(su.t, sk.t) && d < best[k]) { best[k] = d; par[k] = u; }
        }
    }
    for (int i : ids) G.st[i].powered = G.st[i].connected && G.st[i].built >= 1;
}
static int SPENDER = -1; // structure being updated, so energy use can be shown on its links
static bool spend(Team& T, float e) { if (T.energy >= e) { T.energy -= e; T.drainAcc += e; if (SPENDER >= 0) G.st[SPENDER].useAcc += e; return true; } return false; }

// what this team can mine from an asteroid (crystal only after its research is done)
static float astLeft(const Asteroid& a, int team) { return a.dead ? 0 : a.minerals + (team >= 0 && G.teams[team].crystal ? a.crystal : 0); }
static int astInRange(V2 p, float range, int team) {
    int best = -1; float bd = 1e9;
    for (int i = 0; i < (int)G.ast.size(); i++) { auto& a = G.ast[i]; if (astLeft(a, team) <= 0) continue; float d = dist(a.p, p) - a.r; if (d <= range && d < bd) { bd = d; best = i; } }
    return best;
}
static const char* canPlace(int team, int t, V2 q) {
    Team& T = G.teams[team]; float r = SD[t].r;
    if (q.x < r || q.y < r || q.x > G.WW - r || q.y > G.WH - r) return "OUT OF BOUNDS";
    for (auto& s : G.st) if (!s.dead && dist(s.p, q) < s.r() + r + 6) return "BLOCKED";
    for (auto& a : G.ast) if (!a.dead && dist(a.p, q) < a.r * 0.95f + r + 4) return "BLOCKED BY ASTEROID";
    bool linked = false;
    for (auto& s : G.st) if (!s.dead && s.team == team && s.connected && dist(s.p, q) <= linkDist(t, s.t)) { linked = true; break; }
    if (!linked) return "OUT OF NETWORK RANGE";
    if (t == S_MINER && astInRange(q, rangeOf(S_MINER, 1), team) < 0) return "NO ASTEROID IN RANGE";
    if (T.minerals < SD[t].cost) return "NOT ENOUGH MINERALS";
    return nullptr;
}

#include "sim.inc"
#include "render.inc"
#include "ui.inc"
