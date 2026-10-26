// Belt Command - input, menus, main loop (included from main.cpp)

static int PADTEST_HITS = 0; // BC_PADTEST: how many RB presses the game acted on
#ifdef __EMSCRIPTEN__
static const int NTITLE = 3; // a web page has nothing to quit to
#else
static const int NTITLE = 4;
#endif
static Dev* devById(int id) { for (auto& d : G.devs) if (d.id == id) return &d; return nullptr; }
static Dev* kbd() { return devById(-1); }
// a press counts exactly once: latched until the next game step or menu update uses it
static bool pr(const Dev& d, int b) { return d.pend[b]; }
static void usePresses() { for (auto& d : G.devs) { memset(d.pend, 0, sizeof d.pend); d.clickPend = false; d.digit = -1; } }
// mouse on menus: menuList() records where it drew its items, so hovering selects and clicking activates
static struct { int n = 0, frame = -9; float cx = 0, y = 0, size = 0, gap = 0, w[12] = {}; } MH;
static struct { float mx = 0, my = 0, mw = 0, mh = 0, sx = 0, sy = 0, sw = 0, sh = 0; int frame = -9; } LB; // lobby: mode label, start button
static Dev* kbd();
static bool mouseIn(float x, float y, float w, float h) { Dev* k = kbd(); return k && k->mouse.x >= x && k->mouse.x <= x + w && k->mouse.y >= y && k->mouse.y <= y + h; }
static int menuMouse(int& sel, bool& click) { // hover selects; returns the item clicked, or -1
    Dev* k = kbd(); click = false; if (!k || MH.frame < G.frame - 2) return -1;
    int h = -1;
    for (int i = 0; i < MH.n; i++) { float w = MH.w[i] + MH.size * 3; if (mouseIn(MH.cx - w / 2, MH.y + i * MH.gap - MH.gap * 0.25f, w, MH.gap * 0.9f)) h = i; }
    if (h >= 0 && k->mouseMoved && sel != h) { sel = h; AU.play(SND_UI, 0.4f); }
    if (h >= 0 && k->clickPend) { sel = h; click = true; }
    return click ? h : -1;
}
// keyboard keys as buttons, from SDL key events (same mapping as readDevs) so quick taps are never lost
static void keyHit(Dev& d, SDL_Scancode sc, bool inGame) {
    switch (sc) {
    case SDL_SCANCODE_SPACE: d.hit[B_A] = true; break;
    case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: d.hit[B_A] = true; if (!inGame) d.hit[B_START] = true; break;
    case SDL_SCANCODE_BACKSPACE: d.hit[B_B] = true; break;
    case SDL_SCANCODE_ESCAPE: d.hit[inGame ? B_START : B_B] = true; break;
    case SDL_SCANCODE_P: d.hit[B_START] = true; break;
    case SDL_SCANCODE_U: case SDL_SCANCODE_F: d.hit[B_X] = true; break;
    case SDL_SCANCODE_R: case SDL_SCANCODE_DELETE: d.hit[B_Y] = true; break;
    case SDL_SCANCODE_Q: case SDL_SCANCODE_PAGEUP: d.hit[B_LB] = true; break;
    case SDL_SCANCODE_E: case SDL_SCANCODE_PAGEDOWN: d.hit[B_RB] = true; break;
    case SDL_SCANCODE_TAB: d.hit[B_BACK] = true; break;
    case SDL_SCANCODE_W: case SDL_SCANCODE_UP: d.hit[B_UP] = true; break;
    case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN: d.hit[B_DOWN] = true; break;
    case SDL_SCANCODE_A: case SDL_SCANCODE_LEFT: d.hit[B_LEFT] = true; break;
    case SDL_SCANCODE_D: case SDL_SCANCODE_RIGHT: d.hit[B_RIGHT] = true; break;
    default:
        if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) d.digit = sc - SDL_SCANCODE_1;
        else if (sc == SDL_SCANCODE_0) d.digit = 9;
    }
}
static int padButton(int sdlButton) {
    switch (sdlButton) {
    case SDL_CONTROLLER_BUTTON_A: return B_A; case SDL_CONTROLLER_BUTTON_B: return B_B; case SDL_CONTROLLER_BUTTON_X: return B_X; case SDL_CONTROLLER_BUTTON_Y: return B_Y;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return B_LB; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return B_RB;
    case SDL_CONTROLLER_BUTTON_START: return B_START; case SDL_CONTROLLER_BUTTON_BACK: return B_BACK;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return B_UP; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return B_DOWN; case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return B_LEFT; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return B_RIGHT;
    }
    return -1;
}

static void openPad(int index) {
    if (!SDL_IsGameController(index)) { printf("pad: joystick %d is not a game controller (%s)\n", index, SDL_JoystickNameForIndex(index)); return; }
    SDL_GameController* gc = SDL_GameControllerOpen(index); if (!gc) return;
    int id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gc));
    if (devById(id)) return;
    Dev d; d.id = id; d.gc = gc; d.name = SDL_GameControllerName(gc) ? SDL_GameControllerName(gc) : "PAD";
    char guid[64]; SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(gc)), guid, sizeof guid);
    SDL_GameControllerType ty = SDL_GameControllerGetType(gc);
    std::string ln = d.name; for (auto& ch : ln) ch = (char)tolower(ch);
    d.nintendo = ty == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO || ln.find("nintendo") != std::string::npos || ln.find("switch") != std::string::npos || ln.find("pro controller") != std::string::npos;
    printf("pad: added id %d \"%s\" guid %s type %d%s\n", id, d.name.c_str(), guid, (int)ty, d.nintendo ? " (nintendo layout)" : "");
    G.devs.push_back(d);
    for (auto& p : G.pl) if (p.joined && p.devLost && !p.cpu) { p.dev = id; p.devLost = false; printf("pad: reassigned to P%d\n", p.idx + 1); SDL_GameControllerSetPlayerIndex(gc, p.idx); break; }
}
static void closePad(int id) {
    for (size_t i = 0; i < G.devs.size(); i++) if (G.devs[i].id == id) {
        printf("pad: removed id %d \"%s\"\n", id, G.devs[i].name.c_str());
        SDL_GameControllerClose(G.devs[i].gc); G.devs.erase(G.devs.begin() + i); break;
    }
    for (auto& p : G.pl) if (p.joined && p.dev == id) p.devLost = true;
}

static int navAxis(Dev& d, int axis, float dt) {
    int dir = 0;
    if (axis) { if (d.b[B_UP] || d.mv.y < -0.55f) dir = -1; else if (d.b[B_DOWN] || d.mv.y > 0.55f) dir = 1; }
    else { if (d.b[B_LEFT] || d.mv.x < -0.55f) dir = -1; else if (d.b[B_RIGHT] || d.mv.x > 0.55f) dir = 1; }
    int& last = axis ? d.nyLast : d.nxLast; float& t = axis ? d.nyT : d.nxT;
    if (!dir) { last = 0; return 0; }
    if (dir != last) { last = dir; t = 0.36f; return dir; }
    t -= dt; if (t <= 0) { t = 0.1f; return dir; }
    return 0;
}
static void readDevs(float dt, bool inGame) {
    int mx, my; Uint32 mb = SDL_GetMouseState(&mx, &my);
    for (auto& d : G.devs) {
        memcpy(d.pb, d.b, sizeof d.b); memset(d.b, 0, sizeof d.b); d.mv = V2(); d.tl = d.tr = 0;
        if (d.id == -1) {
            const Uint8* k = SDL_GetKeyboardState(nullptr);
            if (k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]) { d.mv.y -= 1; d.b[B_UP] = true; }
            if (k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]) { d.mv.y += 1; d.b[B_DOWN] = true; }
            if (k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT]) { d.mv.x -= 1; d.b[B_LEFT] = true; }
            if (k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT]) { d.mv.x += 1; d.b[B_RIGHT] = true; }
            d.ml = (mb & SDL_BUTTON_LMASK) != 0; d.mr = (mb & SDL_BUTTON_RMASK) != 0;
            d.b[B_A] = k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_KP_ENTER]; // mouse clicks arrive as events (d.click)
            d.b[B_B] = k[SDL_SCANCODE_BACKSPACE] || d.mr || (!inGame && k[SDL_SCANCODE_ESCAPE]);
            d.b[B_X] = k[SDL_SCANCODE_U] || k[SDL_SCANCODE_F];
            d.b[B_Y] = k[SDL_SCANCODE_R] || k[SDL_SCANCODE_DELETE];
            d.b[B_LB] = k[SDL_SCANCODE_Q] || k[SDL_SCANCODE_PAGEUP];
            d.b[B_RB] = k[SDL_SCANCODE_E] || k[SDL_SCANCODE_PAGEDOWN];
            d.b[B_START] = (inGame && k[SDL_SCANCODE_ESCAPE]) || k[SDL_SCANCODE_P] || (!inGame && (k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_KP_ENTER]));
            d.b[B_BACK] = k[SDL_SCANCODE_TAB];
            if (k[SDL_SCANCODE_MINUS] || k[SDL_SCANCODE_KP_MINUS]) d.tr = 1;
            if (k[SDL_SCANCODE_EQUALS] || k[SDL_SCANCODE_KP_PLUS]) d.tl = 1;
            float sx = (float)G.W / G.outW, sy = (float)G.H / G.outH; int ww, wh; SDL_GetWindowSize(G.win, &ww, &wh);
            float ox = mx * (float)G.outW / std::max(ww, 1), oy = my * (float)G.outH / std::max(wh, 1);
            d.mouse = V2(ox * sx, oy * sy);
        } else if (d.gc) {
            auto Bt = [&](SDL_GameControllerButton b) { return SDL_GameControllerGetButton(d.gc, b) != 0; };
            d.b[B_A] = Bt(SDL_CONTROLLER_BUTTON_A); d.b[B_B] = Bt(SDL_CONTROLLER_BUTTON_B); d.b[B_X] = Bt(SDL_CONTROLLER_BUTTON_X); d.b[B_Y] = Bt(SDL_CONTROLLER_BUTTON_Y);
            d.b[B_LB] = Bt(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); d.b[B_RB] = Bt(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            d.b[B_START] = Bt(SDL_CONTROLLER_BUTTON_START); d.b[B_BACK] = Bt(SDL_CONTROLLER_BUTTON_BACK);
            d.b[B_UP] = Bt(SDL_CONTROLLER_BUTTON_DPAD_UP); d.b[B_DOWN] = Bt(SDL_CONTROLLER_BUTTON_DPAD_DOWN); d.b[B_LEFT] = Bt(SDL_CONTROLLER_BUTTON_DPAD_LEFT); d.b[B_RIGHT] = Bt(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            V2 st(SDL_GameControllerGetAxis(d.gc, SDL_CONTROLLER_AXIS_LEFTX) / 32767.f, SDL_GameControllerGetAxis(d.gc, SDL_CONTROLLER_AXIS_LEFTY) / 32767.f);
            float m = len(st); if (m < 0.2f) st = V2(); else st = st / m * clampf((m - 0.2f) / 0.75f, 0, 1);
            V2 dp((d.b[B_RIGHT] ? 1.f : 0.f) - (d.b[B_LEFT] ? 1.f : 0.f), (d.b[B_DOWN] ? 1.f : 0.f) - (d.b[B_UP] ? 1.f : 0.f));
            d.mv = st + dp; if (len(d.mv) > 1) d.mv = norm(d.mv);
            d.tl = clampf(SDL_GameControllerGetAxis(d.gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.f, 0, 1);
            d.tr = clampf(SDL_GameControllerGetAxis(d.gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.f, 0, 1);
            // right stick also zooms (handy on pads with digital triggers)
            float ry = SDL_GameControllerGetAxis(d.gc, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.f;
            if (ry > 0.4f) d.tr = std::max(d.tr, (ry - 0.4f) / 0.6f); else if (ry < -0.4f) d.tl = std::max(d.tl, (-ry - 0.4f) / 0.6f);
        }
        if (d.click) { d.clickPend = true; if (inGame) d.pend[B_A] = true; d.click = false; }
        for (int k = 0; k < NB; k++) {
            if (d.hit[k]) d.b[k] = true; // a tap that went down and up between reads still counts as held this frame
            if ((d.b[k] && !d.pb[k]) || d.hit[k]) d.pend[k] = true;
            d.hit[k] = false;
        }
        d.navX = navAxis(d, 0, dt); d.navY = navAxis(d, 1, dt);
    }
}
struct MenuIn { int x = 0, y = 0; bool a = false, b = false, start = false; int devA = -99; };
static MenuIn menuInput() {
    MenuIn m;
    for (auto& d : G.devs) {
        if (d.navX) m.x = d.navX; if (d.navY) m.y = d.navY;
        if (pr(d, B_A)) { m.a = true; m.devA = d.id; }
        if (pr(d, B_B)) m.b = true; if (pr(d, B_START)) m.start = true;
    }
    return m;
}

// ---------------------------------------------------------------- in-game player control
static void updatePlayer(Player& p, float dt) {
    p.toastT = std::max(0.f, p.toastT - dt); p.shake *= std::pow(0.02f, dt); if (p.shake < 0.1f) p.shake = 0;
    Team& T = G.teams[p.team];
    if (p.cpu) {
        cpuThink(p, dt);
        if (p.view) { float vs = viewScale(p); float hx = p.vp.w * 0.5f / vs * 0.5f, hy = p.vp.h * 0.5f / vs * 0.5f;
            p.cam.x = clampf(p.cam.x, p.cur.x - hx, p.cur.x + hx); p.cam.y = clampf(p.cam.y, p.cur.y - hy, p.cur.y + hy); }
        p.hover = hoverAt(p.team, p.cur, false);
        return;
    }
    Dev* d = devById(p.dev); if (!d) return;
    if (!T.alive && G.mode == MODE_VERSUS) { /* spectate: still allow moving */ }
    // zoom
    float z = (d->tr - d->tl) * dt * 1.3f; if (d->wheel != 0) { z -= d->wheel * 0.1f; }
    p.zoom = clampf(p.zoom * (1 - z), 0.4f, 1.7f);
    float vs = viewScale(p);
    int barSlot = -1;
    if (d->id == -1) {
        // keyboard + mouse: WASD / arrows scroll the map, the mouse only points (no edge scrolling)
        V2 off = p.cur - p.cam;
        p.cam += d->mv * (900 / p.zoom * dt);
        p.cam.x = clampf(p.cam.x, -150, G.WW + 150); p.cam.y = clampf(p.cam.y, -150, G.WH + 150);
        V2 ms = d->mouse;
        if (ms.x >= p.vp.x && ms.x < p.vp.x + p.vp.w && ms.y >= p.vp.y && ms.y < p.vp.y + p.vp.h) { View v; v.vp = p.vp; v.cam = p.cam; v.s = vs; p.cur = v.s2w(ms); barSlot = barSlotAt(p.vp, ms); }
        else p.cur = p.cam + off;
    } else {
        // pads: the stick moves the cursor and the view follows it
        float m = len(d->mv);
        float spd = (160 + 1150 * m * m) / p.zoom;
        p.cur += d->mv * (spd * dt);
        p.cur.x = clampf(p.cur.x, -100, G.WW + 100); p.cur.y = clampf(p.cur.y, -100, G.WH + 100);
        float hx = p.vp.w * 0.5f / vs, hy = p.vp.h * 0.5f / vs, bx = hx * 0.55f, by = hy * 0.5f;
        if (p.cur.x > p.cam.x + bx) p.cam.x = p.cur.x - bx; if (p.cur.x < p.cam.x - bx) p.cam.x = p.cur.x + bx;
        if (p.cur.y > p.cam.y + by) p.cam.y = p.cur.y - by; if (p.cur.y < p.cam.y - by) p.cam.y = p.cur.y + by;
    }
    p.hover = hoverAt(p.team, p.cur, rivals());
    if (p.hover >= 0 && !seen(p.team, G.st[p.hover].team)) p.hover = -1;
    p.bigMap = d->b[B_BACK];
    if (!T.alive) return;
    if (pr(*d, B_LB)) { p.sel = (p.sel + NBUILD - 1) % NBUILD; AU.play(SND_UI, 0.5f); }
    if (pr(*d, B_RB)) { p.sel = (p.sel + 1) % NBUILD; AU.play(SND_UI, 0.5f); PADTEST_HITS++; }
    if (d->id == -1 && d->digit >= 0) { if (d->digit < NBUILD && p.sel != d->digit) { p.sel = d->digit; AU.play(SND_UI, 0.5f); } d->digit = -1; }
    if (barSlot >= 0 && d->clickPend) { if (p.sel != barSlot) { p.sel = barSlot; AU.play(SND_UI, 0.5f); } d->pend[B_A] = false; } // clicking the build bar picks, never builds
    bool own = p.hover >= 0 && G.st[p.hover].team == p.team;
    if (pr(*d, B_A)) { if (own) toast(p, "SPACE TAKEN - MOVE OFF THE STRUCTURE"), AU.play(SND_ERR, 0.4f); else tryBuild(p, BUILDS[p.sel], p.cur, false); }
    if (pr(*d, B_X)) { if (own) tryUpgrade(p, p.hover, false); else { toast(p, "POINT AT A STRUCTURE TO UPGRADE"); AU.play(SND_ERR, 0.4f); } }
    if (d->b[B_Y] && own && G.st[p.hover].t != S_HQ) { p.yHold += dt; if (p.yHold >= 0.6f) { recycle(p, p.hover); p.yHold = -10; } }
    else if (!d->b[B_Y]) p.yHold = 0; else p.yHold = std::min(p.yHold, 0.f);
    if (pr(*d, B_B)) {
        G.pings.push_back({ p.cur, 3.0f, p.idx }); AU.play(SND_PING, 0.5f);
        // raid: ping a rival structure to send every fighter at it
        if (raid() && p.hover >= 0 && G.st[p.hover].team != p.team) { T.order = p.hover; T.orderT = 45; for (auto& f : G.fi) if (f.team == p.team) f.retarget = 0; toast(p, "FIGHTERS ATTACKING"); }
    }
}

// ---------------------------------------------------------------- menus
static void drawPanel(Canvas& cv, float x, float y, float w, float h, Col edge, float u) { cv.rect(x, y, w, h, Col(6, 10, 18, 220)); cv.rectLine(x, y, w, h, 1.3f * u, edge, 0.5f); }
static void drawTitleBackdrop(Canvas& cv, float u) {
    View v; v.vp = { 0, 0, G.W, G.H }; v.s = G.H / 900.f; v.cam = V2(G.stateT * 12, G.stateT * 4);
    drawStars(cv, v); cv.flush(G.ren);
    static std::vector<Asteroid> deco; static Rng dr;
    if (deco.empty()) { dr.s = 777; Rng keep = R; R = dr; for (int i = 0; i < 14; i++) deco.push_back(makeAst(V2(R.range(-900, 900), R.range(-500, 500)), R.range(25, 90), 1)); R = keep; }
    View v2 = v; v2.cam = V2(G.stateT * 22, 0);
    for (auto& a : deco) { Asteroid b = a; b.p.x = std::fmod(a.p.x - G.stateT * 22 + 1100 + 4000, 2200.f) - 1100 + G.stateT * 22; b.rot = a.rot + G.stateT * a.spin * 3; drawAsteroid(cv, v2, b); }
    cv.flush(G.ren);
    (void)u;
}
static void drawLogo(Canvas& cv, float cx, float y, float size) {
    float u = size / 80;
    FONT.draw(cv, "BELT COMMAND", V2(cx + 3 * u, y + 3 * u), size, withA(PCOL[3], 0.35f), 1, size * 0.09f, 0.4f);
    FONT.draw(cv, "BELT COMMAND", V2(cx, y), size, PCOL[0], 1, size * 0.09f, 1.3f);
    FONT.draw(cv, "MINE  -  POWER  -  DEFEND", V2(cx, y + size * 1.35f), size * 0.24f, withA(C_MIN, 0.9f), 1);
}
static void menuList(Canvas& cv, const char** items, int n, int sel, float cx, float y, float size, float gap) {
    MH.n = std::min(n, 12); MH.frame = G.frame; MH.cx = cx; MH.y = y; MH.size = size; MH.gap = gap;
    for (int i = 0; i < MH.n; i++) MH.w[i] = Font::width(items[i], size);
    for (int i = 0; i < n; i++) {
        bool s = i == sel; Col c = s ? PCOL[0] : withA(C_WHITE, 0.6f);
        FONT.draw(cv, items[i], V2(cx, y + i * gap), size, c, 1, 0, s ? 1.0f : 0.3f);
        if (s) { float w = Font::width(items[i], size); float bl = 0.5f + 0.5f * std::sin(G.stateT * 6); FONT.draw(cv, ">", V2(cx - w / 2 - size * 1.4f - bl * size * 0.2f, y + i * gap), size, PCOL[0], 0); FONT.draw(cv, "<", V2(cx + w / 2 + size * 0.4f + bl * size * 0.2f, y + i * gap), size, PCOL[0], 0); }
    }
}

static void enterLobby() { G.state = ST_LOBBY; G.stateT = 0; G.menuSel = 0; }
static int joinedCount() { int n = 0; for (auto& p : G.pl) if (p.joined) n++; return n; }
static void joinPlayer(int devId, bool cpu) {
    for (int i = 0; i < 4; i++) if (!G.pl[i].joined) {
        Player& p = G.pl[i]; p = Player(); p.joined = true; p.cpu = cpu; p.view = !cpu; p.dev = cpu ? -9 : devId; p.idx = i; p.col = PCOL[i];
        if (!cpu) { Dev* d = devById(devId); if (d && d->gc) SDL_GameControllerSetPlayerIndex(d->gc, i); }
        AU.play(SND_UI_OK, 0.7f); printf("lobby: P%d joined (%s)\n", i + 1, cpu ? "CPU" : devId == -1 ? "keyboard" : "pad");
        return;
    }
}
static void updateMenus(float dt) {
    MenuIn m = menuInput(); G.stateT += dt;
    switch (G.state) {
    case ST_TITLE: {
        if (m.y) { G.menuSel = (G.menuSel + m.y + NTITLE) % NTITLE; AU.play(SND_UI, 0.6f); }
        { bool c; menuMouse(G.menuSel, c); if (c) m.a = true; }
        if (m.a || m.start) {
            AU.play(SND_UI_OK, 0.7f);
            if (G.menuSel == 0) enterLobby(); else if (G.menuSel == 1) { G.state = ST_HELP; G.stateT = 0; } else if (G.menuSel == 2) { G.state = ST_OPTIONS; G.menuSel = 0; } else G.quit = true;
        }
        break;
    }
    case ST_HELP: if (m.a || m.b || m.start || (kbd() && kbd()->clickPend)) { G.state = ST_TITLE; G.menuSel = 1; AU.play(SND_UI, 0.6f); } break;
    case ST_OPTIONS: {
        if (m.y) { G.menuSel = (G.menuSel + m.y + 6) % 6; AU.play(SND_UI, 0.6f); }
        { bool c; menuMouse(G.menuSel, c); if (c) m.a = true; }
        int dx = m.x + (m.a ? 1 : 0);
        if (dx) {
            AU.play(SND_UI, 0.6f);
            switch (G.menuSel) {
            case 0: G.opt.res = (G.opt.res + dx + 5) % 5; setupTarget(); layoutViews(); break;
            case 1: G.opt.glow = !G.opt.glow; break;
            case 2: G.opt.shake = !G.opt.shake; break;
            case 3: G.opt.sfx = (int)clampf((float)(G.opt.sfx + (m.a ? 0 : dx)), 0, 10); break;
            case 4: G.opt.mus = (int)clampf((float)(G.opt.mus + (m.a ? 0 : dx)), 0, 10); break;
            case 5: if (m.a) { saveCfg(); G.state = ST_TITLE; G.menuSel = 2; } break;
            }
            applyAudioVol();
        }
        if (m.b) { saveCfg(); G.state = ST_TITLE; G.menuSel = 2; }
        break;
    }
    case ST_LOBBY: {
        bool startNow = false;
        for (auto& d : G.devs) {
            int pi = -1; for (int i = 0; i < 4; i++) if (G.pl[i].joined && !G.pl[i].cpu && G.pl[i].dev == d.id) pi = i;
            if (pi < 0 && pr(d, B_A) && joinedCount() < 4) { joinPlayer(d.id, false); continue; }
            if (pi >= 0) {
                if (pr(d, B_B)) { G.pl[pi].joined = false; AU.play(SND_UI, 0.6f); }
                if (pr(d, B_X) && joinedCount() < 4) joinPlayer(-9, true);
                if (pr(d, B_Y)) { for (int i = 3; i >= 0; i--) if (G.pl[i].joined && G.pl[i].cpu) { G.pl[i].joined = false; AU.play(SND_UI, 0.6f); break; } }
                if (d.navX) { G.mode = (G.mode + d.navX + MODE_N) % MODE_N; AU.play(SND_UI, 0.6f); }
                if (pr(d, B_START)) startNow = true;
            } else if (pr(d, B_B) && joinedCount() == 0) { G.state = ST_TITLE; G.menuSel = 0; AU.play(SND_UI, 0.6f); }
        }
        if (Dev* k = kbd()) if (k->clickPend && LB.frame >= G.frame - 2) {
            bool kbJoined = false; for (auto& p : G.pl) if (p.joined && !p.cpu && p.dev == -1) kbJoined = true;
            if (mouseIn(LB.mx, LB.my, LB.mw, LB.mh)) { G.mode = (G.mode + (k->mouse.x < LB.mx + LB.mw / 2 ? MODE_N - 1 : 1)) % MODE_N; AU.play(SND_UI, 0.6f); }
            else if (mouseIn(LB.sx, LB.sy, LB.sw, LB.sh)) { if (!kbJoined && joinedCount() < 4) joinPlayer(-1, false); startNow = true; }
            else if (!kbJoined && joinedCount() < 4) joinPlayer(-1, false);
        }
        bool anyHuman = false; for (auto& p : G.pl) if (p.joined && !p.cpu) anyHuman = true;
        if (startNow && anyHuman) {
            if (rivals() && joinedCount() < 2) joinPlayer(-9, true);
            AU.play(SND_UI_OK, 0.8f);
            if (G.mode == MODE_COOP) { G.state = ST_MISSIONS; G.menuSel = 0; for (int i = 0; i < NMISSIONS; i++) if (!(G.opt.done & (1u << i))) { G.menuSel = i; break; } G.stateT = 0; }
            else startGame();
        }
        break;
    }
    case ST_MISSIONS: {
        if (m.y) { G.menuSel = (G.menuSel + m.y + NMISSIONS) % NMISSIONS; AU.play(SND_UI, 0.6f); }
        if (Dev* k = kbd()) { // mission rows (same layout as the drawing)
            float u = G.H / 1080.f, y0 = 190 * u, rh = 92 * u, w = 1100 * u;
            for (int i = 0; i < NMISSIONS; i++) if (mouseIn(G.W * 0.5f - w / 2, y0 + i * rh - 12 * u, w, rh - 10 * u)) {
                if (k->mouseMoved && G.menuSel != i) { G.menuSel = i; AU.play(SND_UI, 0.4f); }
                if (k->clickPend) { G.menuSel = i; m.a = true; }
            }
        }
        if (m.a || m.start) { G.mission = G.menuSel; AU.play(SND_UI_OK, 0.8f); startGame(); }
        if (m.b) { enterLobby(); AU.play(SND_UI, 0.6f); }
        break;
    }
    }
}

static void renderMenus(Canvas& cv) {
    float u = G.H / 1080.f, cx = G.W * 0.5f;
    drawTitleBackdrop(cv, u);
    switch (G.state) {
    case ST_TITLE: {
        drawLogo(cv, cx, G.H * 0.2f, 96 * u);
        const char* items[4] = { "PLAY", "HOW TO PLAY", "OPTIONS", "QUIT" };
        menuList(cv, items, NTITLE, G.menuSel, cx, G.H * 0.52f, 34 * u, 62 * u);
        FONT.draw(cv, "1-4 PLAYERS  -  SPLIT SCREEN", V2(cx, G.H - 60 * u), 16 * u, withA(C_DIM, 0.9f), 1);
        break;
    }
    case ST_HELP: {
        FONT.draw(cv, "HOW TO PLAY", V2(cx, 70 * u), 44 * u, PCOL[0], 1, 0, 1.1f);
        const char* lines[] = {
            "YOUR COMMAND MAKES A LITTLE ENERGY AND STARTS THE NETWORK.",
            "EVERY STRUCTURE MUST LINK BACK TO COMMAND OR IT HAS NO POWER.",
            "RELAYS REACH FAR. USE THEM TO GET TO NEW ASTEROIDS.",
            "SOLARS MAKE ENERGY. BATTERIES STORE IT.",
            "MINERS NEXT TO ASTEROIDS TURN ENERGY INTO MINERALS.",
            "MINERALS PAY FOR BUILDING, UPGRADES AND MISSILES.",
            "LASERS BURN ENERGY. MISSILES BURN MINERALS. PLAN FOR BOTH.",
            "ALIENS ATTACK IN WAVES FROM THE EDGE. RED MARKERS WARN YOU.",
            "IF A LINK BREAKS, EVERYTHING PAST IT GOES DARK.",
            "CARRIERS BUILD FIGHTERS. THEY GUARD YOUR BASE AND RAID RIVALS.",
            "RESEARCH FINDS CRYSTAL: EVERY ASTEROID REFILLS AND IT IS WORTH 1.5X.",
            "RAID: NO ALIENS. RIVAL BASES STAY HIDDEN UNTIL YOU GET CLOSE.",
            "",
            "STICK - MOVE CURSOR      LB / RB - CHOOSE STRUCTURE",
            "A - BUILD     X - UPGRADE     HOLD Y - RECYCLE     B - PING",
            "TRIGGERS OR RIGHT STICK - ZOOM     BACK - BIG MAP     START - PAUSE",
            "KEYBOARD: MOUSE POINTS, WASD SCROLLS, CLICK/SPACE, U, R, Q/E, 1-0, ESC",
        };
        int n = sizeof(lines) / sizeof(lines[0]);
        drawPanel(cv, cx - 760 * u, 140 * u, 1520 * u, (n * 46 + 40) * u, withA(PCOL[0], 0.5f), u);
        for (int i = 0; i < n; i++) FONT.draw(cv, lines[i], V2(cx, 170 * u + i * 46 * u), 20 * u, i >= 13 ? withA(C_MIN, 0.95f) : withA(C_WHITE, 0.9f), 1);
        FONT.draw(cv, "PRESS A TO GO BACK", V2(cx, G.H - 60 * u), 16 * u, withA(C_DIM, 0.9f), 1);
        break;
    }
    case ST_OPTIONS: {
        FONT.draw(cv, "OPTIONS", V2(cx, 110 * u), 50 * u, PCOL[0], 1, 0, 1.1f);
        char l[6][64];
        snprintf(l[0], 64, "RESOLUTION  %s", RESNAMES[G.opt.res]);
        snprintf(l[1], 64, "GLOW  %s", G.opt.glow ? "ON" : "OFF");
        snprintf(l[2], 64, "SCREEN SHAKE  %s", G.opt.shake ? "ON" : "OFF");
        snprintf(l[3], 64, "SOUND  %d", G.opt.sfx);
        snprintf(l[4], 64, "MUSIC  %d", G.opt.mus);
        snprintf(l[5], 64, "BACK");
        const char* items[6] = { l[0], l[1], l[2], l[3], l[4], l[5] };
        menuList(cv, items, 6, G.menuSel, cx, G.H * 0.3f, 30 * u, 64 * u);
        char info[96]; snprintf(info, sizeof info, "DISPLAY %d X %d   RENDERING %d X %d", G.outW, G.outH, G.W, G.H);
        FONT.draw(cv, info, V2(cx, G.H - 100 * u), 15 * u, withA(C_DIM, 0.9f), 1);
        FONT.draw(cv, "LEFT / RIGHT TO CHANGE", V2(cx, G.H - 60 * u), 16 * u, withA(C_DIM, 0.9f), 1);
        break;
    }
    case ST_LOBBY: {
        FONT.draw(cv, "PLAYERS", V2(cx, 80 * u), 50 * u, PCOL[0], 1, 0, 1.1f);
        float pw = 380 * u, ph = 330 * u, gap = 36 * u, x0 = cx - (4 * pw + 3 * gap) / 2, y0 = 190 * u;
        for (int i = 0; i < 4; i++) {
            Player& p = G.pl[i]; float x = x0 + i * (pw + gap);
            drawPanel(cv, x, y0, pw, ph, p.joined ? p.col : withA(C_DIM, 0.4f), u);
            char t[32]; snprintf(t, sizeof t, "P%d", i + 1);
            FONT.draw(cv, t, V2(x + pw / 2, y0 + 40 * u), 64 * u, p.joined ? p.col : withA(C_DIM, 0.4f), 1, 0, p.joined ? 1 : 0.2f);
            if (p.joined) {
                std::string nm = p.cpu ? "CPU" : p.dev == -1 ? "KEYBOARD + MOUSE" : (devById(p.dev) ? devById(p.dev)->name : "PAD");
                for (auto& ch : nm) ch = (char)toupper(ch); if (nm.size() > 20) nm = nm.substr(0, 20);
                FONT.draw(cv, nm.c_str(), V2(x + pw / 2, y0 + 150 * u), 15 * u, C_WHITE, 1);
                Struct g; g.t = S_HQ; g.p = V2(0, 0); g.team = 0; g.built = 1; g.powered = true; g.maxhp = g.hp = 1;
                View iv; iv.vp = { (int)x, (int)(y0 + 190 * u), (int)pw, (int)(100 * u) }; iv.cam = V2(); iv.s = 1.3f * u;
                drawStruct(cv, iv, g, p.col, 1, true);
                FONT.draw(cv, p.cpu ? "" : "READY", V2(x + pw / 2, y0 + ph - 34 * u), 16 * u, C_GOOD, 1);
            } else {
                float bl = 0.5f + 0.5f * std::sin(G.stateT * 4 + i);
                FONT.draw(cv, "PRESS A", V2(x + pw / 2, y0 + 170 * u), 22 * u, withA(C_WHITE, 0.4f + 0.5f * bl), 1);
                FONT.draw(cv, "TO JOIN", V2(x + pw / 2, y0 + 205 * u), 22 * u, withA(C_WHITE, 0.4f + 0.5f * bl), 1);
            }
        }
        float my = y0 + ph + 70 * u;
        char ml[64]; snprintf(ml, sizeof ml, "<  %s  >", MODENAMES[G.mode]);
        FONT.draw(cv, ml, V2(cx, my), 34 * u, PCOL[0], 1, 0, 1.0f);
        { float mw = Font::width(ml, 34 * u) + 40 * u; LB.mx = cx - mw / 2; LB.my = my - 14 * u; LB.mw = mw; LB.mh = 70 * u; LB.frame = G.frame; }
        { bool any = false; for (auto& p : G.pl) if (p.joined && !p.cpu) any = true;
          float bw = 260 * u, bh = 64 * u, bx = cx - bw / 2, by = my + 130 * u; LB.sx = bx; LB.sy = by; LB.sw = bw; LB.sh = bh;
          bool hot = mouseIn(bx, by, bw, bh);
          drawPanel(cv, bx, by, bw, bh, any ? (hot ? C_WHITE : PCOL[0]) : withA(C_DIM, 0.5f), u);
          FONT.draw(cv, any ? "START" : "CLICK TO JOIN", V2(cx, by + 20 * u), (any ? 26 : 18) * u, any ? (hot ? C_WHITE : PCOL[0]) : withA(C_WHITE, 0.6f), 1, 0, any ? 1.0f : 0.3f); }
        const char* md = G.mode == MODE_COOP ? "SHARE ONE BASE AND ONE STOCKPILE. PLAY THE MISSIONS." : G.mode == MODE_ENDLESS ? "SHARE ONE BASE. THE WAVES NEVER STOP." : G.mode == MODE_VERSUS ? "EVERYONE BUILDS THEIR OWN BASE. TURRETS FIRE ON RIVALS TOO." : "NO ALIENS. YOUR FRIENDS ARE THE RIVALS. FIND THEIR BASES AND SEND CARRIER FIGHTERS.";
        FONT.draw(cv, md, V2(cx, my + 56 * u), 18 * u, withA(C_WHITE, 0.85f), 1);
        if (G.mode == MODE_ENDLESS && G.opt.bestEndless > 0) FONT.draw(cv, ("BEST " + mmss((float)G.opt.bestEndless)).c_str(), V2(cx, my + 92 * u), 16 * u, C_MIN, 1);
        FONT.draw(cv, "A JOIN    B LEAVE    LEFT/RIGHT MODE    X ADD CPU    Y REMOVE CPU    START BEGIN", V2(cx, G.H - 70 * u), 16 * u, withA(C_DIM, 0.95f), 1);
        break;
    }
    case ST_MISSIONS: {
        FONT.draw(cv, "MISSIONS", V2(cx, 80 * u), 50 * u, PCOL[0], 1, 0, 1.1f);
        float y0 = 190 * u, rh = 92 * u, w = 1100 * u;
        for (int i = 0; i < NMISSIONS; i++) {
            bool s = i == G.menuSel, done = G.opt.done & (1u << i); float y = y0 + i * rh;
            if (s) drawPanel(cv, cx - w / 2, y - 12 * u, w, rh - 10 * u, PCOL[0], u);
            char t[64]; snprintf(t, sizeof t, "%d  %s", i + 1, MISSIONS[i].name);
            FONT.draw(cv, t, V2(cx - w / 2 + 30 * u, y), 28 * u, s ? PCOL[0] : withA(C_WHITE, 0.7f), 0, 0, s ? 1 : 0.3f);
            FONT.draw(cv, MISSIONS[i].desc, V2(cx - w / 2 + 30 * u, y + 40 * u), 15 * u, withA(C_WHITE, s ? 0.9f : 0.5f));
            if (done) FONT.draw(cv, "COMPLETE", V2(cx + w / 2 - 30 * u, y + 6 * u), 18 * u, C_GOOD, 2);
        }
        FONT.draw(cv, "A START    B BACK", V2(cx, G.H - 60 * u), 16 * u, withA(C_DIM, 0.95f), 1);
        break;
    }
    }
    cv.flush(G.ren);
}

// ---------------------------------------------------------------- pause and results overlays
static void updateOverlay(float dt) {
    MenuIn m = menuInput();
    if (G.result) {
        G.resultT += dt; if (G.resultT < 1.6f) return;
        bool canNext = G.mode == MODE_COOP && G.result == 1 && G.mission + 1 < NMISSIONS;
        int n = canNext ? 3 : 2;
        if (m.y) { G.resultSel = (G.resultSel + m.y + n) % n; AU.play(SND_UI, 0.6f); }
        { bool c; menuMouse(G.resultSel, c); if (c) m.a = true; }
        if (m.a || m.start) {
            AU.play(SND_UI_OK, 0.7f);
            int opt = canNext ? G.resultSel : (G.resultSel == 0 ? 0 : 2);
            if (opt == 0) startGame(); else if (opt == 1) { G.mission++; startGame(); } else { for (auto& p : G.pl) if (p.joined && p.cpu && !p.view) {} enterLobby(); }
        }
        return;
    }
    if (G.paused) {
        if (m.y) { G.pauseSel = (G.pauseSel + m.y + 3) % 3; AU.play(SND_UI, 0.6f); }
        { bool c; menuMouse(G.pauseSel, c); if (c) m.a = true; }
        if (m.b || (m.start && G.pauseSel == 0)) { G.paused = false; AU.play(SND_UI, 0.6f); return; }
        if (m.a || m.start) {
            AU.play(SND_UI_OK, 0.7f);
            if (G.pauseSel == 0) G.paused = false; else if (G.pauseSel == 1) startGame(); else { G.paused = false; G.state = ST_TITLE; G.menuSel = 0; G.stateT = 0; }
        }
    }
}
static void renderOverlay(Canvas& cv) {
    float u = G.H / 1080.f, cx = G.W * 0.5f;
    if (G.paused && !G.result) {
        cv.rect(0, 0, (float)G.W, (float)G.H, Col(0, 0, 0, 150));
        FONT.draw(cv, "PAUSED", V2(cx, G.H * 0.3f), 60 * u, PCOL[0], 1, 0, 1.1f);
        const char* items[3] = { "RESUME", "RESTART", "QUIT TO TITLE" };
        menuList(cv, items, 3, G.pauseSel, cx, G.H * 0.48f, 30 * u, 60 * u);
    }
    if (G.result && G.resultT > 0.8f) {
        float a = clampf((G.resultT - 0.8f) * 2, 0, 1);
        cv.rect(0, 0, (float)G.W, (float)G.H, Col(0, 0, 0, (int)(170 * a)));
        std::string title; Col tc = PCOL[0];
        if (rivals()) { if (G.winner >= 0) { for (auto& p : G.pl) if (p.joined && p.team == G.winner) { title = std::string(p.cpu ? "CPU " : "") + "P" + std::to_string(p.idx + 1) + " WINS"; tc = p.col; } } else title = "DRAW"; }
        else title = G.result == 1 ? "MISSION COMPLETE" : (G.mode == MODE_ENDLESS ? "THE BELT IS LOST" : "MISSION FAILED");
        if (G.result == 2) tc = C_BAD;
        FONT.draw(cv, title.c_str(), V2(cx, G.H * 0.2f), 64 * u, withA(tc, a), 1, 0, 1.2f);
        FONT.draw(cv, G.resultMsg.c_str(), V2(cx, G.H * 0.2f + 90 * u), 22 * u, withA(C_WHITE, a), 1);
        float y = G.H * 0.42f;
        for (int t = 0; t < G.nTeams; t++) {
            Team& T = G.teams[t]; char b[160];
            snprintf(b, sizeof b, "%s   MINED %d   KILLS %d   BUILT %d   FIGHTERS LOST %d   TIME %s", rivals() ? ("TEAM " + std::to_string(t + 1)).c_str() : "", (int)T.mined, T.kills, T.built, T.ftrLost, mmss(G.time).c_str());
            FONT.draw(cv, b, V2(cx, y + t * 40 * u), 20 * u, withA(T.col, a), 1);
        }
        if (G.resultT > 1.6f) {
            bool canNext = G.mode == MODE_COOP && G.result == 1 && G.mission + 1 < NMISSIONS;
            const char* i3[3] = { "RETRY", "NEXT MISSION", "BACK TO LOBBY" };
            const char* i2[2] = { "PLAY AGAIN", "BACK TO LOBBY" };
            if (canNext) menuList(cv, i3, 3, G.resultSel, cx, G.H * 0.66f, 28 * u, 54 * u); else menuList(cv, i2, 2, G.resultSel, cx, G.H * 0.66f, 28 * u, 54 * u);
        }
    }
    cv.flush(G.ren);
}

static void saveShot() {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, G.useTarget ? G.W : G.outW, G.useTarget ? G.H : G.outH, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return;
    SDL_RenderReadPixels(G.ren, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    SDL_SaveBMP(s, G.shotPath.c_str()); SDL_FreeSurface(s); printf("shot: saved %s\n", G.shotPath.c_str());
}


// ---------------------------------------------------------------- icon and logo art (rendered by the game itself)
static void renderIcon(Canvas& cv) {
    float S = (float)std::min(G.W, G.H);
    View v; v.vp = { (G.W - (int)S) / 2, (G.H - (int)S) / 2, (int)S, (int)S }; v.cam = V2(0, 0); v.s = S / 250.f;
    G.time = 1.3f; G.teams[0].col = PCOL[0]; G.teams[0].energy = 120; G.teams[0].ecap = 150; G.teams[0].used = true;
    drawStars(cv, v); cv.flush(G.ren);
    cv.glowDisc(v.w2s(V2(15, -10)), 150 * v.s, Col(40, 140, 220, 50));
    Rng keep = R; R.s = 4711; Asteroid a = makeAst(V2(-62, 52), 46, 1); a.minerals = a.maxMin; R = keep;
    drawAsteroid(cv, v, a);
    auto mk = [&](int t, V2 p, float aim) { Struct s; s.t = t; s.p = p; s.team = 0; s.built = 1; s.powered = true; s.connected = true; s.maxhp = s.hp = 1; s.aim = aim; s.mining = true; s.spin = 2; return s; };
    Struct hq = mk(S_HQ, V2(22, -18), 0), mn = mk(S_MINER, V2(-12, 40), PI * 0.95f), lz = mk(S_LASER, V2(70, 42), -1.05f), so = mk(S_SOLAR, V2(-40, -45), 0);
    lz.beamT = 0.1f;
    Struct* all[4] = { &hq, &mn, &lz, &so };
    for (int i = 1; i < 4; i++) { V2 d = norm(all[i]->p - hq.p); cv.line(v.w2s(hq.p + d * 37), v.w2s(all[i]->p - d * (all[i]->r() + 3)), 1.3f * v.s, withA(PCOL[0], 0.55f), 0.8f); cv.glowDisc(v.w2s(hq.p + (all[i]->p - hq.p) * 0.55f), 4 * v.s, withA(C_WHITE, 0.8f)); }
    { V2 p0 = mn.p + norm(a.p - mn.p) * 15, p1 = a.p + norm(mn.p - a.p) * 26; V2 n = perp(norm(p1 - p0)); V2 prev = v.w2s(p0);
      for (int i = 1; i <= 8; i++) { float t = i / 8.f; V2 q = p0 + (p1 - p0) * t + n * std::sin(t * 12) * 3 * (1 - std::fabs(t - 0.5f) * 2); cv.line(prev, v.w2s(q), 1.6f * v.s, C_MIN, 1.2f); prev = v.w2s(q); } }
    for (auto* s : all) drawStruct(cv, v, *s, PCOL[0], 1, false);
    Enemy e; e.t = E_FIGHTER; e.p = V2(98, -62); e.ang = 2.3f; e.hp = e.maxhp = 1; drawEnemy(cv, v, e);
    V2 muzzle = lz.p + fromAng(lz.aim) * 23, hit = e.p + V2(-6, 10);
    cv.line(v.w2s(muzzle), v.w2s(hit), 3.0f * v.s, PCOL[0], 1.5f); cv.line(v.w2s(muzzle), v.w2s(hit), 1.0f * v.s, C_WHITE, 0);
    cv.glowDisc(v.w2s(hit), 16 * v.s, Col(255, 220, 160, 230));
    Rng sr; sr.s = 99; for (int i = 0; i < 9; i++) { V2 d = fromAng(sr.range(0, 6.28f)); cv.line(v.w2s(hit + d * 5), v.w2s(hit + d * sr.range(12, 22)), 1.2f * v.s, Col(255, 180, 120), 0.8f); }
    Enemy e2; e2.t = E_SCOUT; e2.p = V2(-95, -95); e2.ang = 0.8f; e2.hp = e2.maxhp = 1; drawEnemy(cv, v, e2);
    cv.flush(G.ren);
    // frame
    float m = S * 0.03f; V2 o(v.vp.x + m, v.vp.y + m); float w = S - 2 * m, rr = S * 0.08f;
    V2 prev; bool first = true;
    for (int c = 0; c < 4; c++) { V2 cc = o + V2(c == 0 || c == 3 ? rr : w - rr, c < 2 ? rr : w - rr); float a0 = PI + c * PI / 2;
        for (int k = 0; k <= 8; k++) { V2 q = cc + fromAng(a0 + k * PI / 16) * rr; if (!first) cv.line(prev, q, S * 0.007f, withA(PCOL[0], 0.8f), 1); prev = q; first = false; } }
    cv.line(prev, o + V2(0, rr), S * 0.007f, withA(PCOL[0], 0.8f), 1);
    cv.flush(G.ren);
}
static void renderLogoArt(Canvas& cv) { float sz = std::min(G.H * 0.42f, G.W * 0.92f / 12); drawLogo(cv, G.W * 0.5f, G.H * 0.5f - sz * 0.75f, sz); cv.flush(G.ren); }

static void renderFrame(bool shotNow) {
    if (G.useTarget) SDL_SetRenderTarget(G.ren, G.target);
    SDL_SetRenderDrawColor(G.ren, 2, 4, 9, 255); SDL_RenderClear(G.ren);
    G.cv.glow = G.opt.glow; G.cv.px = G.H / 1080.f;
    if (G.state == ST_GAME) { renderGame(G.cv); renderOverlay(G.cv); } else if (G.state == ST_ICON) renderIcon(G.cv); else if (G.state == ST_LOGO) { SDL_SetRenderDrawColor(G.ren, 0, 0, 0, 255); SDL_RenderClear(G.ren); renderLogoArt(G.cv); } else renderMenus(G.cv);
    if (shotNow && G.useTarget) saveShot();
    if (G.useTarget) {
        SDL_SetRenderTarget(G.ren, nullptr); SDL_SetRenderDrawColor(G.ren, 0, 0, 0, 255); SDL_RenderClear(G.ren);
        SDL_RenderCopy(G.ren, G.target, nullptr, nullptr);
    }
    if (shotNow && !G.useTarget) saveShot();
    SDL_RenderPresent(G.ren);
}

// ---------------------------------------------------------------- test setup from environment
static void testSetup() {
    const char* st = getenv("BC_STATE"); if (!st) return;
    std::string s = st;
    int n = getenv("BC_PLAYERS") ? atoi(getenv("BC_PLAYERS")) : 1;
    int cpus = getenv("BC_CPUS") ? atoi(getenv("BC_CPUS")) : 0;
    if (getenv("BC_MODE")) { std::string m = getenv("BC_MODE"); G.mode = m == "raid" ? MODE_RAID : m == "versus" ? MODE_VERSUS : m == "endless" ? MODE_ENDLESS : MODE_COOP; }
    if (getenv("BC_MISSION")) G.mission = atoi(getenv("BC_MISSION"));
    if (s == "title") return;
    if (s == "icon") { G.state = ST_ICON; return; }
    if (s == "logo") { G.state = ST_LOGO; return; }
    if (s == "options") { G.state = ST_OPTIONS; return; }
    if (s == "help") { G.state = ST_HELP; return; }
    for (int i = 0; i < n; i++) { joinPlayer(i == 0 ? -1 : 1000 + i, false); if (G.bots) G.pl[i].cpu = true; }
    for (int i = 0; i < cpus; i++) joinPlayer(-9, true);
    if (rivals() && joinedCount() < 2) joinPlayer(-9, true); // same as the lobby
    if (s == "lobby") { G.state = ST_LOBBY; return; }
    if (s == "missions") { G.state = ST_MISSIONS; return; }
    startGame();
    if (getenv("BC_ADVANCE")) {
        float secs = (float)atof(getenv("BC_ADVANCE"));
        for (float t = 0; t < secs && !G.result; t += 1 / 60.f) { for (auto& p : G.pl) if (p.joined) updatePlayer(p, 1 / 60.f); stepWorld(1 / 60.f); }
        // BC_CAMFIGHT=1: point every view at the nearest fighter dogfight / raid (for screenshots)
        if (getenv("BC_CAMFIGHT")) for (auto& q : G.pl) if (q.joined) { float bd = 1e18f; for (auto& f : G.fi) if (!f.dead && f.tk == 3 && f.team != q.team && G.st[f.tid].team == q.team) { float d = dist2(f.p, q.cam); if (d < bd) { bd = d; q.cam = f.p; } } q.cur = q.cam; }
        if (getenv("BC_ZOOM")) for (auto& q : G.pl) if (q.joined) q.zoom = (float)atof(getenv("BC_ZOOM"));
        printf("test: advanced %.0fs: structs %d enemies %d waves %d minerals %.0f mined %.0f result %d\n", secs, (int)G.st.size(), (int)G.en.size(), G.waveN, G.teams[0].minerals, G.teams[0].mined, G.result);
        for (int t = 0; t < G.nTeams; t++) { int alive = 0; for (auto& q : G.st) if (!q.dead && q.team == t) alive++; { int nf = 0; for (auto& f : G.fi) if (f.team == t) nf++; printf("test: team %d alive structs %d mined %.0f kills %d alive %d fighters %d lost %d found %u\n", t, alive, G.teams[t].mined, G.teams[t].kills, G.teams[t].alive, nf, G.teams[t].ftrLost, G.teams[t].found); } }
    }
}

// one frame: events, input, fixed 60 Hz game steps, draw. Returns false to stop (native only).
static struct { Uint64 last = 0; double acc = 0; float fpsT = 0; int fpsN = 0; } LOOP;
static bool frame() {
#ifdef __EMSCRIPTEN__
    { // follow the browser window size
        int cw = EM_ASM_INT({ return window.innerWidth; }), ch = EM_ASM_INT({ return window.innerHeight; }); int ww, wh; SDL_GetWindowSize(G.win, &ww, &wh);
        if (cw > 0 && ch > 0 && (cw != ww || ch != wh)) { SDL_SetWindowSize(G.win, cw, ch); setupTarget(); if (G.state == ST_GAME) layoutViews(); }
    }
#endif
    double& acc = LOOP.acc; float& fpsT = LOOP.fpsT; int& fpsN = LOOP.fpsN; Uint64& last = LOOP.last;
    {
        for (auto& d : G.devs) { d.mouseMoved = false; d.wheel = 0; }
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: G.quit = true; break;
            case SDL_CONTROLLERDEVICEADDED: openPad(e.cdevice.which); break;
            case SDL_CONTROLLERDEVICEREMOVED: closePad(e.cdevice.which); break;
            case SDL_CONTROLLERBUTTONDOWN: { Dev* p = devById(e.cbutton.which); int b = padButton(e.cbutton.button); if (p && b >= 0) p->hit[b] = true; break; }
            case SDL_MOUSEBUTTONDOWN: { Dev* k = devById(-1); if (k) { if (e.button.button == SDL_BUTTON_LEFT) k->click = true; else if (e.button.button == SDL_BUTTON_RIGHT) k->hit[B_B] = true; } break; }
            case SDL_MOUSEMOTION: { Dev* k = devById(-1); if (k) k->mouseMoved = true; break; }
            case SDL_KEYDOWN: { Dev* k = devById(-1); if (k && !e.key.repeat) keyHit(*k, e.key.keysym.scancode, G.state == ST_GAME && !G.paused && !G.result); break; }
            case SDL_MOUSEWHEEL: { Dev* k = devById(-1); if (k) k->wheel += (float)e.wheel.y; break; }
            case SDL_WINDOWEVENT: if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) { setupTarget(); if (G.state == ST_GAME) layoutViews(); } break;
            }
        }
        Uint64 now = SDL_GetPerformanceCounter(); double dt = (double)(now - last) / SDL_GetPerformanceFrequency(); last = now;
        if (G.shotFrame >= 0) dt = 1 / 60.0;
        // BC_PADTEST=N: plug in a virtual pad, give it to P1 and tap RB N times (down one frame, up the next),
        // with BC_DT=seconds per frame (e.g. 0.00694 = 144 Hz). Prints how many taps the game acted on.
        static int padTest = getenv("BC_PADTEST") ? atoi(getenv("BC_PADTEST")) : 0;
        if (getenv("BC_DT")) dt = atof(getenv("BC_DT"));
        if (padTest && G.state == ST_GAME) {
            static SDL_Joystick* vj = nullptr; static int sent = 0, ph = 0, wait = 0;
            if (!vj) { int vi = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, SDL_CONTROLLER_BUTTON_MAX, 0); vj = SDL_JoystickOpen(vi); wait = 30; printf("padtest: virtual pad index %d %s\n", vi, vj ? "open" : SDL_GetError()); }
            else if (wait > 0) { if (--wait == 0) { for (auto& d : G.devs) if (d.gc) G.pl[0].dev = d.id; G.pl[0].cpu = false; printf("padtest: P1 uses pad %d\n", G.pl[0].dev); } }
            else if (sent < padTest) { if (ph == 0) { SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 1); sent++; } else if (ph == 1) SDL_JoystickSetVirtualButton(vj, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 0); ph = (ph + 1) % 3; }
            else if (ph++ > 20) { printf("padtest: sent %d taps, game used %d\n", sent, PADTEST_HITS); G.quit = true; }
        }
        if (dt > 0.1) dt = 0.1;
        fpsT += (float)dt; fpsN++; if (fpsT > 30) { printf("fps: %.1f (enemies %d, structures %d, particles %d)\n", fpsN / fpsT, (int)G.en.size(), (int)G.st.size(), (int)G.parts.size()); fpsT = 0; fpsN = 0; }
        readDevs((float)dt, G.state == ST_GAME && !G.paused && !G.result);
        // BACK + START anywhere returns to the title (Batocera's HOTKEY + START exits the game)
        for (auto& d : G.devs) if (d.id != -1 && d.b[B_BACK] && d.b[B_START] && (pr(d, B_START) || pr(d, B_BACK))) { if (G.state == ST_GAME) { G.state = ST_TITLE; G.menuSel = 0; G.paused = false; } }
        if (G.state == ST_GAME) {
            if (G.paused || G.result) { updateOverlay((float)dt); usePresses(); }
            if (!G.paused && !G.result) for (auto& p : G.pl) if (p.joined && !p.cpu && !p.devLost) { Dev* d = devById(p.dev); if (d && pr(*d, B_START) && !d->b[B_BACK]) { G.paused = true; G.pauseSel = 0; G.pauser = p.idx; AU.play(SND_UI_OK, 0.6f); usePresses(); break; } }
            if (!G.paused) {
                acc += dt;
                while (acc >= 1 / 60.0) {
                    acc -= 1 / 60.0;
                    if (!G.result) { for (auto& p : G.pl) if (p.joined) updatePlayer(p, 1 / 60.f); usePresses(); } // presses wait for a step, and fire once
                    stepWorld(1 / 60.f);
                }
            }
        } else { updateMenus((float)dt); usePresses(); }
        G.frame++;
        static int benchN = getenv("BC_FRAMES") ? atoi(getenv("BC_FRAMES")) : 0; static Uint64 benchT0 = 0;
        if (benchN && G.frame == 10) benchT0 = SDL_GetPerformanceCounter();
        if (benchN && G.frame == 10 + benchN) { double s = (double)(SDL_GetPerformanceCounter() - benchT0) / SDL_GetPerformanceFrequency(); printf("bench: %d frames in %.2fs = %.1f fps\n", benchN, s, benchN / s); G.quit = true; }
        { static int shown = -1; int want = !(G.state == ST_GAME && !G.paused && !G.result) && G.state != ST_ICON && G.state != ST_LOGO;
          if (want != shown) { SDL_ShowCursor(want ? SDL_ENABLE : SDL_DISABLE); shown = want; } }
        bool shot = G.shotFrame >= 0 && G.frame == G.shotFrame;
        renderFrame(shot);
        if (shot) return false;
    }
    return true;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    setvbuf(stdout, nullptr, _IOLBF, 0);
    printf("Belt Command starting\n");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) { printf("SDL_Init failed: %s\n", SDL_GetError()); return 1; }
    SDL_version lv; SDL_GetVersion(&lv); printf("sdl: %d.%d.%d, video driver %s\n", lv.major, lv.minor, lv.patch, SDL_GetCurrentVideoDriver());
    const char* home = getenv("BC_HOME"); char* base = SDL_GetBasePath();
    G.cfgPath = std::string(home ? home : (base ? base : "./")) + (home ? "/" : "") + "beltcommand.cfg"; if (base) SDL_free(base);
    loadCfg();
    if (getenv("BC_SHOT")) G.shotFrame = atoi(getenv("BC_SHOT"));
    if (getenv("BC_SHOT_PATH")) G.shotPath = getenv("BC_SHOT_PATH");
    if (getenv("BC_BOTS")) G.bots = true;
    if (getenv("BC_RES")) G.opt.res = atoi(getenv("BC_RES"));
    if (getenv("BC_GLOW")) G.opt.glow = atoi(getenv("BC_GLOW")) != 0;
#ifdef __EMSCRIPTEN__
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
    Uint32 wf = SDL_WINDOW_RESIZABLE; int ww = EM_ASM_INT({ return window.innerWidth; }), wh = EM_ASM_INT({ return window.innerHeight; });
#else
    Uint32 wf = SDL_WINDOW_FULLSCREEN_DESKTOP; int ww = 1280, wh = 720;
#endif
    if (getenv("BC_WINDOW")) { wf = 0; sscanf(getenv("BC_WINDOW"), "%dx%d", &ww, &wh); }
    G.win = SDL_CreateWindow("Belt Command", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, wf | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!G.win) { printf("window failed: %s\n", SDL_GetError()); return 1; }
    G.ren = SDL_CreateRenderer(G.win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (!G.ren) G.ren = SDL_CreateRenderer(G.win, -1, SDL_RENDERER_TARGETTEXTURE);
    if (!G.ren) { printf("renderer failed: %s\n", SDL_GetError()); return 1; }
    SDL_RendererInfo ri; SDL_GetRendererInfo(G.ren, &ri); printf("renderer: %s\n", ri.name);
    setupTarget();
    if (!getenv("BC_MUTE")) AU.open();
    applyAudioVol();
    Dev kb; kb.id = -1; kb.name = "KEYBOARD"; G.devs.push_back(kb);
    testSetup();
    LOOP.last = SDL_GetPerformanceCounter();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop([] { frame(); }, 0, 1); // the browser calls frame() once per display refresh
#else
    while (!G.quit && frame()) {}
#endif
    saveCfg();
    AU.close();
    SDL_DestroyRenderer(G.ren); SDL_DestroyWindow(G.win); SDL_Quit();
    printf("Belt Command exit\n");
    return 0;
}

