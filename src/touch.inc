// Belt Command - touch screens: finger events, taps, on-screen buttons (state in touch.h).
// The touch player is the keyboard/mouse player (device -1) on a host, or this browser's own seat when it is a guest.
// Touch actions go through the same buttons as pads (A build, X upgrade, B attack, START pause, BACK map, RECYCLE),
// so online guests send them to the host like any other input.

static bool touchPlayerIs(const Player& p) {
    if (GS.on) return GS.slot >= 0 && &p == &G.pl[GS.slot];
    return p.joined && !p.cpu && !p.remote && p.dev == -1;
}
static Player* touchPlayer() {
    if (G.state != ST_GAME) return nullptr;
    for (auto& p : G.pl) if (p.joined && p.view && touchPlayerIs(p)) return &p;
    return nullptr;
}
// in-game touches go to the map and buttons; menus (and the pause / leave menus) use the mouse events SDL makes from touches
static bool touchInGame() { return G.state == ST_GAME && !G.paused && !G.result && !(GS.on && GS.leaveAsk) && touchPlayer() != nullptr; }

static void touchReset() { TC.mode = 0; TC.mapOn = false; TC.f.clear(); TC.pinched = false; }

// the player's cursor each step: the placed ghost or the selected structure, otherwise the middle of the view
static void touchCursor(Player& p) {
    if (!TC.zoomSet) { p.zoom = 1.5f; TC.zoomSet = true; } // phones are small: start closer
    p.cam.x = clampf(p.cam.x, -150, G.WW + 150); p.cam.y = clampf(p.cam.y, -150, G.WH + 150);
    p.cur = TC.mode ? TC.cur : p.cam;
}
// after hover is worked out: nothing placed, nothing hovered; a selected structure that is gone ends the selection
static void touchHover(Player& p) {
    if (TC.mode == 0 || TC.mode == 3) { if (TC.mode == 0) p.hover = -1; return; }
    else if (TC.mode == 2 && p.hover < 0) TC.mode = 0;
}

static int touchButtonAt(V2 s) {
    if (TC.btnFrame < G.frame - 3) return TA_NONE;
    for (auto& b : TC.btns) if (s.x >= b.x && s.x <= b.x + b.w && s.y >= b.y && s.y <= b.y + b.h) return b.act;
    return TA_NONE;
}
static void touchAction(Player& p, int act) {
    Dev* d = kbd(); if (!d) return;
    switch (act) {
    case TA_BUILD: d->hit[B_A] = true; TC.mode = 3; break; // mode 3: the cursor stays put until the press is used
    case TA_CANCEL: TC.mode = 0; AU.play(SND_UI, 0.5f); break;
    case TA_UPGRADE: d->hit[B_X] = true; break;
    case TA_RECYCLE: d->hit[B_RECYCLE] = true; TC.mode = 3; break;
    case TA_ATTACK: d->hit[B_B] = true; TC.mode = 3; break;
    case TA_MAP: TC.mapOn = !TC.mapOn; AU.play(SND_UI, 0.5f); break;
    case TA_PAUSE: d->hit[B_START] = true; break;
    }
    (void)p;
}
// a tap (or a mouse click on a touch button) in the game
static void touchTap(V2 s) {
    Player* pp = touchPlayer(); if (!pp) return; Player& p = *pp;
    if (getenv("BC_TOUCHLOG") || TC.log) printf("touch: tap %.0f,%.0f mode %d\n", s.x, s.y, TC.mode);
    int b = touchButtonAt(s); if (b) { touchAction(p, b); return; }
    if (TC.mapFrame >= G.frame - 3 && s.x >= TC.mx && s.x <= TC.mx + TC.mw && s.y >= TC.my && s.y <= TC.my + TC.mh) { // tap the map: look there
        p.cam = V2((s.x - TC.mx) / TC.mw * G.WW, (s.y - TC.my) / TC.mh * G.WH); TC.mapOn = false; AU.play(SND_UI, 0.4f); return;
    }
    if (TC.mapOn) { TC.mapOn = false; return; }
    int slot = barSlotAt(p.vp, s);
    if (slot >= 0) { if (p.sel != slot) { p.sel = slot; AU.play(SND_UI, 0.5f); } if (TC.mode == 2) TC.mode = 0; return; }
    View v; v.vp = p.vp; v.cam = p.cam; v.s = viewScale(p); V2 w = v.s2w(s);
    if (TC.mode == 3) return;
    if (TC.mode == 1 && dist(w, TC.cur) * v.s < 46 * TC.dpr) { touchAction(p, TA_BUILD); return; } // second tap on the ghost builds
    int h = hoverAt(p.team, w, rivals()); if (h >= 0 && !seen(p.team, G.st[h].team)) h = -1;
    if (h >= 0) { TC.mode = 2; TC.cur = G.st[h].p; AU.play(SND_UI, 0.4f); return; }
    if (TC.mode == 2) { TC.mode = 0; return; }
    TC.mode = 1; TC.cur = w; AU.play(SND_UI, 0.35f);
}

// SDL finger events (x, y are 0..1 across the window)
static void touchEvent(const SDL_Event& e) {
    V2 s(e.tfinger.x * G.W, e.tfinger.y * G.H);
    if (!TC.on) { TC.on = true; printf("input: touch screen\n"); }
    if (!touchInGame()) { TC.f.clear(); return; }
    Player& p = *touchPlayer();
    if (e.type == SDL_FINGERDOWN) {
        TFinger f; f.id = e.tfinger.fingerId; f.start = f.pos = f.last = s; f.t0 = SDL_GetTicks(); f.moved = false; TC.f.push_back(f);
        if (TC.f.size() == 2) { TC.pinchD = std::max(1.f, dist(TC.f[0].pos, TC.f[1].pos)); TC.pinchMid = (TC.f[0].pos + TC.f[1].pos) * 0.5f; TC.pinched = true; }
        return;
    }
    TFinger* f = nullptr; for (auto& q : TC.f) if (q.id == e.tfinger.fingerId) f = &q;
    if (!f) return;
    if (e.type == SDL_FINGERMOTION) {
        f->pos = s;
        if (dist(f->pos, f->start) > 12 * TC.dpr) f->moved = true;
        float vs = viewScale(p);
        if (TC.f.size() >= 2) { // pinch: zoom around the fingers and pan with them
            V2 a = TC.f[0].pos, b = TC.f[1].pos; float d = std::max(1.f, dist(a, b)); V2 mid = (a + b) * 0.5f;
            View v; v.vp = p.vp; v.cam = p.cam; v.s = vs; V2 wmid = v.s2w(TC.pinchMid);
            p.zoom = clampf(p.zoom * d / TC.pinchD, 0.4f, zoomMax());
            float ns = viewScale(p); // keep the world point under the old midpoint under the new one
            p.cam = wmid - (mid - V2(p.vp.x + p.vp.w * 0.5f, p.vp.y + p.vp.h * 0.5f)) / ns;
            TC.pinchD = d; TC.pinchMid = mid;
        } else if (f->moved && !TC.pinched) p.cam -= (f->pos - f->last) / vs; // one finger drags the map
        f->last = s;
        p.cam.x = clampf(p.cam.x, -150, G.WW + 150); p.cam.y = clampf(p.cam.y, -150, G.WH + 150);
        return;
    }
    // finger up: a short touch that did not move is a tap
    bool tap = !f->moved && !TC.pinched && SDL_GetTicks() - f->t0 < 450 && TC.f.size() == 1;
    TC.f.erase(TC.f.begin() + (f - &TC.f[0]));
    if (TC.f.empty()) TC.pinched = false;
    if (tap) touchTap(s);
}

// on-screen buttons. PAUSE (and MAP on touch) on the left; BUILD / CANCEL or UPGRADE / RECYCLE / ATTACK on the right.
static void drawTouchUi(Canvas& cv, Player& p, float u, float barY) {
    const SDL_Rect& vp = p.vp; Team& T = G.teams[p.team];
    TC.btns.clear(); TC.btnFrame = G.frame;
    auto button = [&](float x, float y, float w, float h, int act, const char* l1, const char* l2, Col c, bool on) {
        TC.btns.push_back({ x, y, w, h, act });
        cv.rect(x, y, w, h, Col(6, 10, 18, 215)); cv.rectLine(x, y, w, h, 1.6f * u, on ? c : withA(C_DIM, 0.6f), on ? 0.8f : 0.2f);
        float ls = std::min(15 * u, w * 0.8f / std::max(1.f, (float)strlen(l1)));
        FONT.draw(cv, l1, V2(x + w / 2, y + h * (l2 ? 0.22f : 0.5f) - ls / 2 + (l2 ? 0 : 0)), ls, on ? c : C_DIM, 1, 0, on ? 0.6f : 0);
        if (l2) FONT.draw(cv, l2, V2(x + w / 2, y + h * 0.6f), std::min(11 * u, w * 0.9f / std::max(1.f, (float)strlen(l2))), on ? withA(C_WHITE, 0.8f) : C_DIM, 1);
    };
    float bw = 120 * u, bh = 54 * u, gap = 9 * u;
    // left: pause, map (touch only)
    float lx = vp.x + 14 * u, ly = vp.y + 88 * u;
    button(lx, ly, 78 * u, 46 * u, TA_PAUSE, "PAUSE", nullptr, PCOL[0], true);
    if (TC.on) button(lx, ly + 46 * u + gap, 78 * u, 46 * u, TA_MAP, TC.mapOn ? "CLOSE" : "MAP", nullptr, PCOL[0], true);
    TC.hudX = vp.x + vp.w - 14 * u;
    if (!TC.on) return;
    // right: what you can do with the ghost or the selected structure
    float rx = vp.x + vp.w - bw - 14 * u, y = barY - 14 * u - bh;
    char buf[48];
    if (TC.mode == 1) {
        int t = BUILDS[p.sel]; snprintf(buf, sizeof buf, "%d", SD[t].cost);
        const char* why = canPlace(p.team, t, TC.cur);
        button(rx, y, bw, bh, TA_BUILD, "BUILD", why ? why : buf, C_GOOD, !why); y -= bh + gap;
        button(rx, y, bw, bh, TA_CANCEL, "CANCEL", nullptr, PCOL[0], true); y -= bh + gap;
        TC.hudX = rx;
    } else if (TC.mode == 2 && p.hover >= 0) {
        const Struct& s = G.st[p.hover];
        if (s.team == p.team) {
            bool canUp = s.lv < 3 && s.built >= 1; snprintf(buf, sizeof buf, "%d", upgCost(s.t, s.lv));
            button(rx, y, bw, bh, TA_UPGRADE, "UPGRADE", canUp ? buf : s.lv >= 3 ? "MAX LEVEL" : "BUILDING", C_MIN, canUp && T.minerals >= upgCost(s.t, s.lv)); y -= bh + gap;
            if (s.t != S_HQ) { snprintf(buf, sizeof buf, "+%d", (int)recycleValue(s)); button(rx, y, bw, bh, TA_RECYCLE, "RECYCLE", buf, Col(255, 200, 90), true); y -= bh + gap; }
        } else if (raid()) { button(rx, y, bw, bh, TA_ATTACK, "ATTACK", "SEND FIGHTERS", C_BAD, true); y -= bh + gap; }
        button(rx, y, bw, bh, TA_CANCEL, "CLOSE", nullptr, PCOL[0], true);
        TC.hudX = rx;
    }
}
