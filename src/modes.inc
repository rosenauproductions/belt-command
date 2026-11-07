// Belt Command - tutorial script and daily challenge helpers (included from main.cpp after render.inc)
//
// Tutorial: a guided first game. One player, a calm fixed belt, no timed waves. Each step says what to do and which
// button does it on this player's controls (pad, keyboard + mouse or touch); the step ends when the player has done it.
// One small alien group comes after the first laser. Mining 400 minerals at the end finishes the tutorial.

static bool GUEST_ON(); // net.inc
struct TutStep { const char* say; const char* why; int act; int slot; };
enum { TU_INFO, TU_LOOK, TU_SOLAR, TU_MINER, TU_RELAY, TU_LASER, TU_WAVE, TU_UPGRADE, TU_BATTERY, TU_MINE };
static int buildSlot(int t) { for (int i = 0; i < NBUILD; i++) if (BUILDS[i] == t) return i; return -1; }
static const TutStep TSTEPS[] = {
    { "THIS IS YOUR COMMAND", "IT MAKES A LITTLE ENERGY. EVERYTHING YOU BUILD MUST LINK BACK TO IT.", TU_INFO, -1 },
    { "LOOK AROUND THE BELT", "", TU_LOOK, -1 },
    { "BUILD A SOLAR NEAR YOUR COMMAND", "SOLARS MAKE ENERGY. A LINE SHOWS THE POWER LINK.", TU_SOLAR, 1 },
    { "BUILD A MINER NEXT TO AN ASTEROID", "MINERS TURN ENERGY INTO MINERALS. MINERALS PAY FOR EVERYTHING.", TU_MINER, 3 },
    { "BUILD A RELAY TOWARD MORE ASTEROIDS", "RELAYS ARE CHEAP AND LINK FAR. CHAIN THEM TO REACH NEW ROCKS.", TU_RELAY, 0 },
    { "ALIENS ARE COMING. BUILD A LASER", "LASERS SHOOT ALIENS AND USE ENERGY. PUT IT NEAR YOUR COMMAND.", TU_LASER, 4 },
    { "HERE THEY COME", "THE RED MARKER SHOWS WHERE. KEEP YOUR LASER POWERED.", TU_WAVE, -1 },
    { "UPGRADE A STRUCTURE", "UPGRADES MAKE IT STRONGER. YOUR LASER IS A GOOD CHOICE.", TU_UPGRADE, -1 },
    { "BUILD A BATTERY", "BATTERIES STORE ENERGY FOR BUSY MOMENTS, LIKE A BIG FIGHT.", TU_BATTERY, 2 },
    { "LAST STEP: MINE 400 MINERALS", "BUILD MORE MINERS AND SOLARS. CHAIN RELAYS TO NEW ASTEROIDS.", TU_MINE, -1 },
};
static const int NTSTEPS = sizeof(TSTEPS) / sizeof(TSTEPS[0]);
static struct { int step = 0; float t = 0; V2 cam0; bool waveSent = false; } TUT;

static Player* tutPlayer() { for (auto& p : G.pl) if (p.joined && p.view) return &p; return nullptr; } // BC_BOTS: a CPU can play it (tests)
static bool tutHas(int t, bool needMining = false) {
    for (auto& s : G.st) if (!s.dead && s.team == 0 && s.t == t && (!needMining || s.mining)) return true;
    return false;
}
static void tutStart() { TUT = {}; Player* p = tutPlayer(); if (p) TUT.cam0 = p->cam; printf("tutorial: start\n"); }
static void tutNext() {
    TUT.step++; TUT.t = 0; AU.play(SND_UI_OK, 0.8f);
    printf("tutorial: step %d %s\n", TUT.step + 1, TUT.step < NTSTEPS ? TSTEPS[TUT.step].say : "done");
    if (TUT.step < NTSTEPS && TSTEPS[TUT.step].slot >= 0) { Player* p = tutPlayer(); if (p) p->sel = TSTEPS[TUT.step].slot; } // the right icon is already picked
}
static void tutUpdate(float dt) {
    if (G.result || TUT.step >= NTSTEPS) return;
    TUT.t += dt; Player* p = tutPlayer(); if (!p) return;
    const TutStep& s = TSTEPS[TUT.step];
    if (getenv("BC_TUTBOT") && TUT.t > 2.5f && fmodf(TUT.t, 1.f) < dt) { // test: do this step's action
        int want = s.act == TU_SOLAR ? S_SOLAR : s.act == TU_MINER ? S_MINER : s.act == TU_RELAY ? S_RELAY : s.act == TU_LASER ? S_LASER : s.act == TU_BATTERY ? S_BATTERY : -1;
        V2 hq = G.st[G.teams[0].hq].p, at;
        if (want >= 0 && findSpot(*p, want, at, hq, true)) tryBuild(*p, want, at, true);
        if (s.act == TU_UPGRADE) for (size_t i = 0; i < G.st.size(); i++) if (!G.st[i].dead && G.st[i].t == S_LASER && G.st[i].built >= 1) { tryUpgrade(*p, (int)i, true); break; }
        if (s.act == TU_LOOK) p->cam = p->cam + V2(300, 0);
    }
    switch (s.act) {
    case TU_INFO: if (TUT.t > 6) tutNext(); break;
    case TU_LOOK: if (dist(p->cam, TUT.cam0) > 260 || TUT.t > 14) tutNext(); break;
    case TU_SOLAR: if (tutHas(S_SOLAR)) tutNext(); break;
    case TU_MINER: if (tutHas(S_MINER, true)) tutNext(); break;
    case TU_RELAY: if (tutHas(S_RELAY)) tutNext(); break;
    case TU_LASER: if (tutHas(S_LASER)) tutNext(); break;
    case TU_WAVE: {
        if (!TUT.waveSent && TUT.t > 3) { // a small group, from the side the laser faces
            V2 hq = G.st[G.teams[0].hq].p, aim = hq + V2(1, 0);
            for (auto& st : G.st) if (!st.dead && st.team == 0 && st.t == S_LASER) aim = st.p;
            V2 d = aim - hq; float l = std::sqrt(d.x * d.x + d.y * d.y); d = l > 1 ? d * (1 / l) : V2(1, 0);
            V2 at = hq + d * 900; at.x = clampf(at.x, 80, G.WW - 80); at.y = clampf(at.y, 80, G.WH - 80);
            G.warns.push_back({ at, 6, 8, false }); teamSound(SND_ALERT, 0.55f, -1); G.alertT = 2.5f; TUT.waveSent = true;
        }
        if (TUT.waveSent && G.warns.empty() && G.en.empty() && TUT.t > 10) tutNext();
        break;
    }
    case TU_UPGRADE: for (auto& st : G.st) if (!st.dead && st.team == 0 && st.t != S_HQ && st.lv >= 2) { tutNext(); break; } break;
    case TU_BATTERY: if (tutHas(S_BATTERY)) tutNext(); break;
    case TU_MINE: if (G.teams[0].mined >= G.M.target) { TUT.step = NTSTEPS; finish(1, 0, "YOU'RE READY FOR THE MISSIONS"); } break;
    }
}

// which button does it, on this player's controls
static std::string tutHow(const Player& p, int act) {
    bool touch = p.dev == -1 && TC.on, kb = p.dev == -1 && !TC.on;
    int slot = TUT.step < NTSTEPS ? TSTEPS[TUT.step].slot : -1;
    std::string key = slot >= 0 ? std::to_string((slot + 1) % 10) : "";
    switch (act) {
    case TU_LOOK: return touch ? "DRAG TO MOVE THE MAP. PINCH TO ZOOM." : kb ? "WASD OR ARROWS MOVE THE MAP. THE MOUSE POINTS. WHEEL ZOOMS." : "LEFT STICK MOVES. TRIGGERS ZOOM.";
    case TU_SOLAR: case TU_MINER: case TU_RELAY: case TU_LASER: case TU_BATTERY:
        return touch ? "IT'S PICKED IN THE BAR. TAP THE MAP TO PLACE IT, THEN TAP AGAIN TO BUILD."
             : kb ? "IT'S PICKED (OR PRESS " + key + "). POINT AND LEFT-CLICK, OR PRESS SPACE."
                  : std::string("IT'S PICKED (LB / RB CHANGE IT). MOVE THE CURSOR AND PRESS ") + btnLab(p.dev, B_A) + ".";
    case TU_UPGRADE: return touch ? "TAP THE STRUCTURE, THEN TAP UPGRADE." : kb ? "POINT AT IT AND PRESS U." : std::string("POINT AT IT AND PRESS ") + btnLab(p.dev, B_X) + ".";
    case TU_MINE: return touch ? "TAP A STRUCTURE TO UPGRADE OR RECYCLE IT. PAUSE IS BOTTOM LEFT." : kb ? "HOLD R ON A STRUCTURE TO RECYCLE IT. ESC PAUSES." : std::string("HOLD ") + btnLab(p.dev, B_Y) + " ON A STRUCTURE TO RECYCLE IT.";
    }
    return "";
}

// the guide box under the objective, plus a glow on the icon to use and on the asteroid to mine
static void drawTutorial(Canvas& cv, Player& p, float u) {
    if (!G.tutorial || GUEST_ON() || G.result || TUT.step >= NTSTEPS) return;
    const TutStep& s = TSTEPS[TUT.step]; const SDL_Rect& vp = p.vp;
    float pulse = 0.55f + 0.45f * std::sin(G.time * 5);
    std::string how = s.act == TU_INFO || s.act == TU_WAVE ? s.why : tutHow(p, s.act);
    std::string why = s.act == TU_INFO || s.act == TU_WAVE ? "" : s.why;
    // between the top-left panel (minerals, energy, touch buttons) and the minimap, under the objective line
    float mw = std::min(vp.w * 0.2f, 300 * u * 1.3f), left = vp.x + 300 * u, right = vp.x + vp.w - mw - 28 * u;
    if (right - left < 420 * u) { left = vp.x + 14 * u; right = vp.x + vp.w - 14 * u; } // narrow view: full width
    float w = std::min(right - left, 1040 * u), h = (why.empty() ? 90 : 118) * u, x = (left + right) / 2 - w / 2, y = vp.y + 62 * u;
    cv.rect(x, y, w, h, Col(6, 10, 18, 225)); cv.rectLine(x, y, w, h, 1.6f * u, withA(PCOL[0], 0.5f + 0.4f * pulse), 0.6f);
    auto fit = [&](const std::string& t, float size) { float tw = Font::width(t.c_str(), size); return tw > w * 0.94f ? size * w * 0.94f / tw : size; };
    char st[32]; snprintf(st, sizeof st, "TUTORIAL  %d / %d", TUT.step + 1, NTSTEPS);
    FONT.draw(cv, st, V2(x + 14 * u, y + 10 * u), 11 * u, withA(C_DIM, 0.9f));
    FONT.draw(cv, s.say, V2(x + w / 2, y + 38 * u), fit(s.say, 24 * u), PCOL[0], 1, 0, 0.6f);
    FONT.draw(cv, how.c_str(), V2(x + w / 2, y + 68 * u), fit(how, 16 * u), withA(C_WHITE, 0.92f), 1);
    if (!why.empty()) FONT.draw(cv, why.c_str(), V2(x + w / 2, y + 95 * u), fit(why, 14 * u), withA(C_DIM, 1), 1);
    // the icon to use
    if (s.slot >= 0) {
        Bar b = barOf(vp); float ox = b.sx + s.slot * (b.slot + b.gap), g = 5 * u;
        cv.rectLine(ox - g, b.sy - g, b.slot + 2 * g, b.slot + 2 * g, 2.5f * u, withA(C_GOOD, pulse), 1.0f);
    }
    // the asteroid to mine: the nearest one to command with minerals left
    if (s.act == TU_MINER && G.teams[0].hq >= 0) {
        V2 hq = G.st[G.teams[0].hq].p; int best = -1; float bd = 1e9f;
        for (size_t i = 0; i < G.ast.size(); i++) if (!G.ast[i].dead && G.ast[i].minerals > 50) { float d = dist(G.ast[i].p, hq); if (d < bd) { bd = d; best = (int)i; } }
        if (best >= 0) {
            View v; v.vp = vp; v.cam = p.cam; v.s = viewScale(p);
            V2 c = v.w2s(G.ast[best].p); float r = (G.ast[best].r + 26 + 8 * pulse) * v.s;
            cv.circle(c, r, 2.2f * u, withA(C_GOOD, pulse), 0.9f);
        }
    }
}

// daily challenge: a line for the lobby
static std::string dailyLobbyLine() {
    uint32_t d = todayUTC(); Mission m = dailyMission(d);
    std::string s = dayText(d) + "  -  " + m.desc;
    return s;
}
