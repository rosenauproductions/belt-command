// Belt Command - rendering (included from main.cpp)

struct View {
    SDL_Rect vp; V2 cam; float s = 1;
    V2 w2s(V2 w) const { return V2(vp.x + vp.w * 0.5f + (w.x - cam.x) * s, vp.y + vp.h * 0.5f + (w.y - cam.y) * s); }
    V2 s2w(V2 p) const { return V2((p.x - vp.x - vp.w * 0.5f) / s + cam.x, (p.y - vp.y - vp.h * 0.5f) / s + cam.y); }
    bool vis(V2 w, float r) const { V2 q = w2s(w); float rr = r * s + 6; return q.x + rr >= vp.x && q.x - rr <= vp.x + vp.w && q.y + rr >= vp.y && q.y - rr <= vp.y + vp.h; }
};
static float viewScale(const Player& p) { return p.vp.h / 900.f * p.zoom * (p.vp.h < G.H * 0.75f ? 1.25f : 1.0f); }
static View viewOf(const Player& p) { View v; v.vp = p.vp; v.cam = p.cam; v.s = viewScale(p); if (p.shake > 0.1f) v.cam += V2(R.range(-1, 1), R.range(-1, 1)) * p.shake / v.s * G.cv.px; return v; }
static float hudU(const SDL_Rect& vp) { return G.H / 1080.f * (vp.h < G.H * 0.75f ? 0.62f : 1.0f); }

static uint32_t hash3(int a, int b, int c) { uint32_t h = (uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u ^ (uint32_t)c * 83492791u; h ^= h >> 13; h *= 0x5bd1e995; h ^= h >> 15; return h; }
static void drawStars(Canvas& cv, const View& v, float timeScroll = 0) {
    const float layers[3] = { 0.18f, 0.4f, 0.7f };
    for (int L = 0; L < 3; L++) {
        float f = layers[L], T = 380;
        V2 pc = v.cam * f + V2(timeScroll * (L + 1) * 6, 0);
        float hx = v.vp.w * 0.5f / v.s, hy = v.vp.h * 0.5f / v.s;
        int tx0 = (int)std::floor((pc.x - hx) / T), tx1 = (int)std::floor((pc.x + hx) / T), ty0 = (int)std::floor((pc.y - hy) / T), ty1 = (int)std::floor((pc.y + hy) / T);
        if ((tx1 - tx0 + 1) * (ty1 - ty0 + 1) > 400) continue;
        for (int ty = ty0; ty <= ty1; ty++) for (int tx = tx0; tx <= tx1; tx++) {
            uint32_t h = hash3(tx, ty, L + 7); int n = 3 + (h & 7);
            for (int k = 0; k < n; k++) {
                h = h * 1664525u + 1013904223u; float sx = (h >> 8 & 0xffff) / 65536.f; h = h * 1664525u + 1013904223u; float sy = (h >> 8 & 0xffff) / 65536.f;
                h = h * 1664525u + 1013904223u; float br = 0.25f + 0.75f * ((h >> 12) & 255) / 255.f;
                V2 w(tx * T + sx * T, ty * T + sy * T);
                V2 q(v.vp.x + v.vp.w * 0.5f + (w.x - pc.x) * v.s, v.vp.y + v.vp.h * 0.5f + (w.y - pc.y) * v.s);
                float sz = (0.8f + L * 0.6f) * G.cv.px * (0.6f + br);
                Col c = (h & 0x300) == 0x300 ? Col(255, 220, 190) : (h & 0x300) == 0x100 ? Col(180, 210, 255) : Col(225, 230, 255);
                cv.rectAdd(q.x - sz * 0.5f, q.y - sz * 0.5f, sz, sz, withA(c, br * (0.35f + L * 0.25f)));
            }
        }
    }
}

static void drawStruct(Canvas& cv, const View& v, const Struct& s, Col tc, float alpha, bool ghost) {
    V2 c = v.w2s(s.p); float k = v.s; float lw = std::max(1.2f, 1.9f * k * G.cv.px / std::max(G.cv.px, 0.5f));
    lw = std::max(1.0f, 1.7f * k);
    bool unp = !ghost && s.built >= 1 && !s.powered;
    Col lc = tc;
    if (unp) lc = withA(C_DIM, 0.6f + 0.25f * std::sin(G.time * 9 + s.p.x));
    if (!ghost && s.built < 1) lc = withA(tc, 0.5f);
    if (s.flash > 0) lc = mixc(lc, C_WHITE, clampf(s.flash * 8, 0, 1));
    lc = withA(lc, alpha);
    Col fc((int)(tc.r * 0.10f + 6), (int)(tc.g * 0.10f + 8), (int)(tc.b * 0.10f + 12), (int)(235 * alpha));
    if (unp) fc = Col(14, 16, 20, (int)(235 * alpha));
    auto P = [&](float x, float y) { return c + V2(x, y) * k; };
    auto PR = [&](float x, float y, float a) { return c + rotv(V2(x, y), a) * k; };
    float gm = unp ? 0.3f : 1.0f;
    switch (s.t) {
    case S_HQ: {
        V2 o[6], in[6];
        for (int i = 0; i < 6; i++) { o[i] = c + fromAng(i * PI / 3 + PI / 6) * 34 * k; in[i] = c + fromAng(i * PI / 3 + G.time * 0.4f) * 19 * k; }
        cv.fillPoly(o, 6, fc); cv.poly(o, 6, lw * 1.4f, lc, gm); cv.poly(in, 6, lw, withA(lc, 0.8f), gm);
        for (int i = 0; i < 3; i++) { float a = i * 2 * PI / 3 - G.time * 0.7f; cv.line(c + fromAng(a) * 22 * k, c + fromAng(a) * 31 * k, lw, withA(lc, 0.7f), gm); }
        if (!unp && !ghost) cv.glowDisc(c, 16 * k, withA(tc, 0.55f + 0.15f * std::sin(G.time * 3)));
        break;
    }
    case S_RELAY: {
        V2 d[4] = { P(0, -11), P(9, 0), P(0, 11), P(-9, 0) };
        cv.fillPoly(d, 4, fc); cv.poly(d, 4, lw, lc, gm);
        if (s.powered) cv.glowDisc(c, 6 * k, withA(tc, 0.5f + 0.5f * std::sin(G.time * 4 + s.p.y)));
        break;
    }
    case S_SOLAR: {
        V2 m[4] = { P(-5, -5), P(5, -5), P(5, 5), P(-5, 5) };
        for (int sd = -1; sd <= 1; sd += 2) {
            V2 pnl[4] = { P(sd * 8.f, -10), P(sd * 23.f, -10), P(sd * 23.f, 10), P(sd * 8.f, 10) };
            cv.fillPoly(pnl, 4, fc); cv.poly(pnl, 4, lw, lc, gm);
            cv.line(P(sd * 13.f, -10), P(sd * 13.f, 10), lw * 0.6f, withA(lc, 0.6f), gm * 0.5f);
            cv.line(P(sd * 18.f, -10), P(sd * 18.f, 10), lw * 0.6f, withA(lc, 0.6f), gm * 0.5f);
            cv.line(P(sd * 8.f, 0), P(sd * 23.f, 0), lw * 0.6f, withA(lc, 0.6f), gm * 0.5f);
        }
        cv.fillPoly(m, 4, fc); cv.poly(m, 4, lw, lc, gm);
        if (s.powered) cv.glowDisc(c, 7 * k, withA(C_EN, 0.4f));
        break;
    }
    case S_BATTERY: {
        V2 b[4] = { P(-9, -11), P(9, -11), P(9, 13), P(-9, 13) }, cap[4] = { P(-4, -15), P(4, -15), P(4, -11), P(-4, -11) };
        cv.fillPoly(b, 4, fc); cv.poly(b, 4, lw, lc, gm); cv.poly(cap, 4, lw, lc, gm);
        const Team& T = G.teams[s.team]; float fr = T.ecap > 0 ? T.energy / T.ecap : 0;
        int bars = s.powered ? (int)std::ceil(fr * 4 - 0.01f) : 0;
        for (int i = 0; i < 4; i++) { float y = 9 - i * 5.f; Col bc = i < bars ? withA(C_EN, alpha) : withA(C_DIM, 0.25f * alpha); cv.line(P(-5, y), P(5, y), lw * 1.3f, bc, i < bars ? 0.8f : 0); }
        break;
    }
    case S_MINER: {
        float a = s.aim;
        V2 t[3] = { PR(16, 0, a), PR(-10, 11, a), PR(-10, -11, a) };
        cv.fillPoly(t, 3, fc); cv.poly(t, 3, lw, lc, gm);
        for (int i = 0; i < 3; i++) { float b = s.spin * 2 + i * 2 * PI / 3; cv.line(c + fromAng(b) * 2.5f * k, c + fromAng(b) * 7.5f * k, lw, withA(s.mining ? C_MIN : lc, alpha), gm); }
        break;
    }
    case S_LASER: {
        V2 o[8]; for (int i = 0; i < 8; i++) o[i] = c + fromAng(i * PI / 4 + PI / 8) * 13 * k;
        cv.fillPoly(o, 8, fc); cv.poly(o, 8, lw, lc, gm);
        float a = s.aim;
        cv.line(PR(0, -2.5f, a), PR(22, -2.5f, a), lw, lc, gm); cv.line(PR(0, 2.5f, a), PR(22, 2.5f, a), lw, lc, gm);
        cv.circle(c, 5 * k, lw * 0.8f, withA(lc, 0.8f), gm, 10);
        if (s.beamT > 0) cv.glowDisc(PR(23, 0, a), 9 * k, withA(tc, 0.9f));
        break;
    }
    case S_MISSILE: {
        V2 b[4] = { P(-13, -13), P(13, -13), P(13, 13), P(-13, 13) };
        cv.fillPoly(b, 4, fc); cv.poly(b, 4, lw, lc, gm);
        float a = s.aim;
        for (int sd = -1; sd <= 1; sd += 2) {
            V2 tb[4] = { PR(-6, sd * 3.f, a), PR(18, sd * 3.f, a), PR(18, sd * 8.f, a), PR(-6, sd * 8.f, a) };
            cv.fillPoly(tb, 4, fc); cv.poly(tb, 4, lw * 0.9f, lc, gm);
            if (s.cd < 0.4f && s.powered) cv.line(PR(12, sd * 5.5f, a), PR(17, sd * 5.5f, a), lw * 1.2f, withA(Col(255, 200, 120), alpha), 0.8f);
        }
        if (s.beamT > 0) cv.glowDisc(PR(20, 0, a), 12 * k, withA(Col(255, 190, 110), 0.8f));
        break;
    }
    case S_REPAIR: {
        cv.disc(c, 15 * k, fc); cv.circle(c, 15 * k, lw, lc, gm);
        Col g = withA(unp ? C_DIM : mixc(tc, C_GOOD, 0.7f), alpha);
        cv.line(P(-8, 0), P(8, 0), lw * 1.5f, g, gm); cv.line(P(0, -8), P(0, 8), lw * 1.5f, g, gm);
        break;
    }
    case S_SHIELD: {
        cv.disc(c, 14 * k, fc); cv.circle(c, 14 * k, lw, lc, gm); cv.circle(c, 6 * k, lw * 0.8f, lc, gm, 12);
        for (int i = 0; i < 3; i++) { float b = G.time * 1.2f + i * 2 * PI / 3; cv.arc(c, 21 * k, b, b + 1.2f, lw, withA(lc, 0.85f), gm); }
        break;
    }
    case S_RESEARCH: {
        // dish on a mast, scanning; turns crystal-coloured once the mineral is found
        bool done = !ghost && G.teams[s.team].crystal; Col dc = done ? withA(C_CRY, alpha) : lc;
        V2 b[4] = { P(-10, 8), P(10, 8), P(6, 14), P(-6, 14) }; cv.fillPoly(b, 4, fc); cv.poly(b, 4, lw, lc, gm);
        cv.line(P(0, 8), P(0, -2), lw, lc, gm);
        float a = ghost ? -PI / 2 : -PI / 2 + 0.6f * std::sin(s.spin);
        V2 d0 = PR(-1, -14, a + PI / 2), d1 = PR(-1, 14, a + PI / 2); V2 cc = c + fromAng(a) * 2 * k + V2(0, -2) * k;
        cv.arc(cc, 13 * k, a - 1.0f, a + 1.0f, lw * 1.2f, dc, gm); cv.line(cc, cc + fromAng(a) * 12 * k, lw * 0.8f, dc, gm); (void)d0; (void)d1;
        if (!ghost && s.powered && !done) cv.arc(c, (s.r() + 4) * k, -PI / 2, -PI / 2 + 2 * PI * s.charge, lw, withA(C_CRY, 0.8f));
        if (done) cv.glowDisc(cc + fromAng(a) * 12 * k, 5 * k, withA(C_CRY, 0.7f));
        break;
    }
    case S_CARRIER: {
        // long hull with a flight deck; lit bays show stored fighters
        V2 h[8] = { P(-24, -7), P(-14, -12), P(16, -12), P(26, -4), P(26, 4), P(16, 12), P(-14, 12), P(-24, 7) };
        cv.fillPoly(h, 8, fc); cv.poly(h, 8, lw, lc, gm);
        cv.line(P(-18, 0), P(22, 0), lw * 0.7f, withA(lc, 0.5f), gm * 0.5f);
        V2 br[4] = { P(-8, -12), P(2, -12), P(0, -18), P(-6, -18) }; cv.fillPoly(br, 4, fc); cv.poly(br, 4, lw * 0.9f, lc, gm);
        int cap = carrierCap(s.lv), have = ghost ? cap : std::max(0, s.ast);
        for (int i = 0; i < cap; i++) { float x = -15 + i * 30.f / std::max(1, cap - 1); Col bc = i < have ? withA(tc, alpha) : withA(C_DIM, 0.35f * alpha); cv.line(P(x - 2.5f, 5), P(x + 2.5f, 5), lw * 1.2f, bc, i < have ? 0.8f : 0); }
        if (!ghost && s.powered && s.charge > 0 && have < cap) cv.arc(c + V2(20, 0) * k, 4 * k, -PI / 2, -PI / 2 + 2 * PI * s.charge, lw, withA(C_WHITE, 0.8f * alpha));
        break;
    }
    }
    if (ghost) return;
    if (s.lv > 1) for (int i = 0; i < s.lv; i++) { V2 q = P(-5.f * (s.lv - 1) + 10.f * i, s.r() + 7); cv.glowDisc(q, std::max(1.6f, 2.6f * k), withA(C_WHITE, 0.9f * alpha)); }
    if (s.built < 1) cv.arc(c, (s.r() + 6) * k, -PI / 2, -PI / 2 + 2 * PI * s.built, lw * 1.2f, withA(tc, 0.9f));
    else if (s.hp < s.maxhp - 0.5f) {
        float w = (s.r() * 2 + 4) * k, h = std::max(2.f, 3 * k); V2 o = P(0, -s.r() - 9) - V2(w / 2, 0); float fr = clampf(s.hp / s.maxhp, 0, 1);
        cv.rect(o.x, o.y, w, h, Col(0, 0, 0, 180)); cv.rect(o.x, o.y, w * fr, h, fr > 0.5f ? Col(110, 230, 120, 230) : fr > 0.25f ? Col(240, 200, 60, 230) : Col(250, 70, 60, 240));
    }
}

static void drawEnemy(Canvas& cv, const View& v, const Enemy& e) {
    V2 c = v.w2s(e.p); float k = v.s, a = e.ang, lw = std::max(1.0f, 1.7f * k);
    Col col = e.t == E_BOMBER ? Col(255, 130, 40) : e.t == E_SWARM ? Col(255, 80, 180) : e.t == E_MOTHER ? Col(255, 50, 90) : C_ALIEN;
    if (e.flash > 0) col = C_WHITE;
    Col fc(40, 6, 12, 235);
    auto PR = [&](float x, float y) { return c + rotv(V2(x, y), a) * k; };
    switch (e.t) {
    case E_SCOUT: { V2 p[4] = { PR(12, 0), PR(-8, 7), PR(-4, 0), PR(-8, -7) }; cv.fillPoly(p, 4, fc); cv.poly(p, 4, lw, col); break; }
    case E_FIGHTER: { V2 p[7] = { PR(15, 0), PR(0, 5), PR(-7, 13), PR(-11, 4), PR(-11, -4), PR(-7, -13), PR(0, -5) }; cv.fillPoly(p, 7, fc); cv.poly(p, 7, lw, col); cv.line(PR(-4, 0), PR(8, 0), lw * 0.8f, withA(col, 0.7f)); break; }
    case E_BOMBER: { V2 p[5]; for (int i = 0; i < 5; i++) p[i] = c + rotv(fromAng(i * 2 * PI / 5) * 18, a) * k; cv.fillPoly(p, 5, fc); cv.poly(p, 5, lw * 1.2f, col); cv.circle(c, 7 * k, lw, withA(col, 0.8f + 0.2f * std::sin(G.time * 12)), 1, 10); cv.glowDisc(c, 9 * k, withA(col, 0.5f)); break; }
    case E_SWARM: { V2 p[3] = { PR(7, 0), PR(-5, 5), PR(-5, -5) }; cv.fillPoly(p, 3, fc); cv.poly(p, 3, lw * 0.9f, col); break; }
    case E_MOTHER: {
        const float hull[10][2] = { { 1, 0 }, { 0.5f, 0.35f }, { 0.1f, 0.45f }, { -0.6f, 0.8f }, { -0.9f, 0.4f }, { -0.7f, 0 }, { -0.9f, -0.4f }, { -0.6f, -0.8f }, { 0.1f, -0.45f }, { 0.5f, -0.35f } };
        V2 p[10]; for (int i = 0; i < 10; i++) p[i] = PR(hull[i][0] * 62, hull[i][1] * 62);
        cv.fillPoly(p, 10, fc); cv.poly(p, 10, lw * 1.6f, col);
        cv.circle(c, 22 * k, lw, withA(col, 0.7f)); for (int i = 0; i < 4; i++) { float b = G.time + i * PI / 2; cv.arc(c, 30 * k, b, b + 0.8f, lw, withA(col, 0.8f)); }
        cv.glowDisc(c, 20 * k, withA(col, 0.6f + 0.2f * std::sin(G.time * 4)));
        float w = 120 * k, h = std::max(3.f, 4 * k); V2 o = c - V2(w / 2, 80 * k); float fr = e.hp / e.maxhp;
        cv.rect(o.x, o.y, w, h, Col(0, 0, 0, 180)); cv.rect(o.x, o.y, w * fr, h, Col(255, 60, 90, 230));
        break;
    }
    }
    if (e.t != E_MOTHER && e.hp < e.maxhp && e.t != E_SWARM) {
        float w = ED[e.t].r * 2 * k, h = std::max(1.5f, 2 * k); V2 o = c - V2(w / 2, (ED[e.t].r + 6) * k);
        cv.rect(o.x, o.y, w, h, Col(0, 0, 0, 160)); cv.rect(o.x, o.y, w * e.hp / e.maxhp, h, withA(col, 0.9f));
    }
}

static void drawFighter(Canvas& cv, const View& v, const Fighter& f) {
    V2 c = v.w2s(f.p); float k = v.s * 1.4f, lw = std::max(1.0f, 1.5f * v.s);
    Col col = f.flash > 0 ? C_WHITE : G.teams[f.team].col;
    auto PR = [&](float x, float y) { return c + rotv(V2(x, y), f.ang) * k; };
    V2 p[5] = { PR(9, 0), PR(-5, 6), PR(-2, 0), PR(-5, -6), PR(9, 0) };
    cv.fillPoly(p, 4, Col(8, 12, 20, 235)); cv.poly(p, 4, lw, col, 0.9f);
    cv.glowDisc(PR(-4, 0), 3.5f * k, withA(mixc(col, C_WHITE, 0.4f), 0.7f));
    if (f.hp < f.maxhp - 0.5f) { float w = 16 * k, h = std::max(1.5f, 2 * k); V2 o = c - V2(w / 2, 13 * k); cv.rect(o.x, o.y, w, h, Col(0, 0, 0, 160)); cv.rect(o.x, o.y, w * f.hp / f.maxhp, h, withA(col, 0.9f)); }
}

static void drawAsteroid(Canvas& cv, const View& v, const Asteroid& a, bool showCry = false) {
    float fr = a.maxMin > 0 ? std::min(1.f, (a.minerals + (showCry ? a.crystal : 0)) / a.maxMin) : 0;
    float r = a.r * (0.55f + 0.45f * fr);
    V2 c = v.w2s(a.p); int n = (int)a.shape.size();
    std::vector<V2> pts(n); for (int i = 0; i < n; i++) pts[i] = c + rotv(a.shape[i], a.rot) * r * v.s;
    cv.fillPoly(pts.data(), n, Col(16, 19, 27, 245));
    cv.poly(pts.data(), n, std::max(1.0f, 1.6f * v.s), withA(C_AST, 0.85f), 0.5f);
    for (int i = 0; i < (int)pts.size(); i += 3) { V2 in = c + (pts[i] - c) * 0.55f; cv.line(pts[i], in, std::max(0.8f, v.s), withA(C_AST, 0.18f), 0); }
    int nf = (int)a.flecks.size(), lit = (int)std::ceil(fr * nf - 0.001f);
    for (int i = 0; i < nf; i++) {
        V2 q = c + rotv(a.flecks[i], a.rot) * r * v.s; float sz = std::max(1.2f, 2.6f * v.s);
        Col fc2 = showCry && a.minerals <= 0 ? C_CRY : C_MIN;
        if (i < lit) { cv.line(q - V2(sz, 0), q + V2(sz, 0), sz * 0.8f, withA(fc2, 0.9f), 0.9f); cv.line(q - V2(0, sz), q + V2(0, sz), sz * 0.8f, withA(fc2, 0.9f), 0); }
        else cv.line(q - V2(sz * 0.6f, 0), q + V2(sz * 0.6f, 0), sz * 0.6f, withA(C_AST, 0.3f), 0);
    }
}

static bool fighterSeen(int vt, const Fighter& f) { return !raid() || vt < 0 || f.team == vt || inSight(vt, f.p); }
static void drawWorld(Canvas& cv, const View& v, const Player* me) {
    float k = v.s, lw = std::max(1.0f, 1.4f * k); int vt = me ? me->team : -1;
    // world border
    { V2 a = v.w2s(V2(0, 0)), b = v.w2s(V2(G.WW, G.WH)); Col bc = withA(Col(80, 110, 160), 0.45f); float ph = G.time * 20 * k;
      cv.dash(a, V2(b.x, a.y), 14 * k, 10 * k, lw, bc, ph); cv.dash(V2(b.x, a.y), b, 14 * k, 10 * k, lw, bc, ph); cv.dash(b, V2(a.x, b.y), 14 * k, 10 * k, lw, bc, ph); cv.dash(V2(a.x, b.y), a, 14 * k, 10 * k, lw, bc, ph); }
    bool cryView = vt >= 0 ? G.teams[vt].crystal : G.teams[0].crystal;
    for (auto& a : G.ast) if (!a.dead && v.vis(a.p, a.r * 1.2f)) drawAsteroid(cv, v, a, cryView);
    // shields
    for (auto& s : G.st) if (!s.dead && s.t == S_SHIELD && s.powered && s.charge > 1 && seen(vt, s.team)) {
        float R0 = rangeOf(S_SHIELD, s.lv); if (!v.vis(s.p, R0)) continue; float fr = s.charge / (160 * lvm(s.lv));
        Col tc = G.teams[s.team].col; cv.circle(v.w2s(s.p), R0 * k, lw, withA(tc, 0.12f + 0.3f * fr), 0.8f);
        cv.glowDisc(v.w2s(s.p), R0 * k, withA(tc, 0.05f + 0.05f * fr));
    }
    // network links with energy pulses
    for (int t = 0; t < 4; t++) {
        Team& T = G.teams[t]; if (!T.used || !seen(vt, t)) continue;
        for (size_t i = 0; i < T.links.size(); i++) {
            const Struct& a = G.st[T.links[i].first]; const Struct& b = G.st[T.links[i].second]; if (a.dead || b.dead) continue;
            if (!v.vis((a.p + b.p) * 0.5f, dist(a.p, b.p) * 0.5f + 20)) continue;
            V2 d = norm(b.p - a.p); V2 pa = v.w2s(a.p + d * (a.r() + 3)), pb = v.w2s(b.p - d * (b.r() + 3));
            bool live = a.powered && b.built >= 1 && T.alive;
            // busier links glow brighter, thicker and pulse faster
            float fl = i < T.linkFlow.size() ? T.linkFlow[i] : 0, f = live ? fl / (fl + 14) : 0;
            Col lcol = live ? mixc(T.col, C_WHITE, 0.35f * f) : C_DIM;
            if (live && f > 0.05f) cv.line(pa, pb, std::max(1.f, (2.5f + 6 * f) * k), withA(T.col, 0.06f + 0.16f * f), 0.3f + 1.2f * f);
            cv.line(pa, pb, std::max(0.8f, (1.0f + 1.6f * f) * k), withA(lcol, live ? 0.38f + 0.5f * f : 0.25f), 0.4f + 1.3f * f);
            if (b.built < 1 && a.powered) { float ph = std::fmod(G.time * 1.8f, 1.f); cv.glowDisc(pa + (pb - pa) * ph, 4 * k, withA(T.col, 0.8f)); }
            else if (live && T.eprod > 0) {
                float L = dist(a.p, b.p); int np = 1 + (int)(f * 3);
                for (int q = 0; q < np; q++) {
                    float ph = std::fmod(G.time * (90 + 260 * f) / std::max(L, 40.f) + (hash3(T.links[i].second, 0, 1) & 255) / 255.f + q / (float)np, 1.f);
                    cv.glowDisc(pa + (pb - pa) * ph, (3.0f + 3.5f * f) * k, withA(mixc(T.col, C_WHITE, 0.4f), T.energy > 2 ? 0.55f + 0.4f * f : 0.3f));
                }
            }
        }
    }
    // mining beams
    for (auto& s : G.st) if (!s.dead && s.t == S_MINER && s.mining && s.ast >= 0 && seen(vt, s.team)) {
        const Asteroid& a = G.ast[s.ast]; if (!v.vis((s.p + a.p) * 0.5f, dist(s.p, a.p))) continue;
        V2 d = norm(a.p - s.p); V2 p0 = s.p + d * 16, p1 = a.p - d * a.r * 0.6f; V2 n = perp(d); V2 prev = v.w2s(p0);
        Col bc = s.cryBeam ? C_CRY : C_MIN; float fl = 0.75f + 0.25f * std::sin(G.time * 13 + s.p.y);
        cv.line(v.w2s(p0), v.w2s(p1), std::max(2.f, 7 * k), withA(bc, 0.13f * fl), 1.6f); // soft halo
        for (int i = 1; i <= 6; i++) { float t = i / 6.f; V2 q = p0 + (p1 - p0) * t + n * std::sin(G.time * 20 + t * 9 + s.p.x) * 3 * (1 - std::fabs(t - 0.5f) * 2); V2 qs = v.w2s(q); cv.line(prev, qs, std::max(0.8f, 1.5f * k), withA(mixc(bc, C_WHITE, 0.25f), 0.9f), 1.5f); prev = qs; }
        cv.glowDisc(v.w2s(p1), 9 * k * fl, withA(bc, 0.75f)); cv.glowDisc(v.w2s(p0), 5 * k, withA(bc, 0.5f));
    }
    // range rings for the hovered structure and the selected build
    if (me) {
        if (me->hover >= 0) { const Struct& s = G.st[me->hover]; float rg = rangeOf(s.t, s.lv); if (rg > 0) cv.circle(v.w2s(s.p), rg * k, lw, withA(G.teams[s.team].col, 0.35f), 0.4f); }
    }
    for (auto& f : G.fi) if (!f.dead && v.vis(f.p, 14) && fighterSeen(vt, f)) drawFighter(cv, v, f);
    for (auto& s : G.st) if (!s.dead && v.vis(s.p, s.r() + 10) && seen(vt, s.team)) drawStruct(cv, v, s, G.teams[s.team].col, 1, false);
    for (auto& b : G.beams) { float fr = b.life / b.max; V2 pa = v.w2s(b.a), pb = v.w2s(b.b); cv.line(pa, pb, b.w * k * (0.5f + fr), withA(b.c, fr), 1.2f); cv.line(pa, pb, std::max(0.8f, b.w * k * 0.4f), withA(C_WHITE, fr * 0.8f), 0); }
    for (auto& e : G.en) if (!e.dead && v.vis(e.p, ED[e.t].r + 10)) drawEnemy(cv, v, e);
    for (auto& b : G.bl) {
        if (!v.vis(b.p, 20)) continue;
        if (b.missile) { V2 d = norm(b.v); V2 q = v.w2s(b.p); cv.line(q, v.w2s(b.p - d * 9), std::max(1.f, 2.2f * k), Col(255, 220, 160), 1); cv.glowDisc(q, 5 * k, withA(G.teams[b.team].col, 0.7f)); }
        else { V2 d = norm(b.v); cv.line(v.w2s(b.p), v.w2s(b.p - d * 10), std::max(1.f, 2.0f * k), Col(255, 110, 90), 1); }
    }
    for (auto& q : G.parts) {
        if (!v.vis(q.p, 60)) continue;
        float fr = q.life / q.max; V2 p = v.w2s(q.p);
        switch (q.kind) {
        case P_SPARK: cv.line(p, v.w2s(q.p - q.v * 0.05f), std::max(0.8f, 1.3f * k), withA(q.c, fr), 0.6f); break;
        case P_DOT: cv.glowDisc(p, std::max(1.5f, q.a * k * (0.4f + fr * 0.6f)), withA(q.c, fr * 0.9f)); break;
        case P_RING: cv.circle(p, lerpf(q.a, q.b, 1 - fr * fr) * k, std::max(0.8f, 1.6f * k * fr + 0.5f), withA(q.c, fr), 0.8f); break;
        case P_DEBRIS: { V2 d = fromAng(q.rot) * q.a * 0.5f; cv.line(v.w2s(q.p - d), v.w2s(q.p + d), std::max(0.8f, 1.3f * k), withA(q.c, std::min(1.f, fr * 2)), 0.4f); break; }
        }
    }
    // incoming warnings at the border
    for (auto& w : G.warns) {
        V2 p = v.w2s(w.p); float bl = 0.5f + 0.5f * std::sin(G.time * 10);
        cv.circle(p, (40 + 20 * bl) * k * (w.mother ? 2 : 1), lw * 1.5f, withA(C_ALIEN, 0.5f + 0.5f * bl), 1);
        FONT.draw(cv, w.mother ? "MOTHERSHIP" : "INCOMING", p + V2(0, -70 * k), std::max(10.f, 22 * k), withA(C_ALIEN, 0.6f + 0.4f * bl), 1);
    }
    for (auto& pg : G.pings) {
        const Player& o = G.pl[pg.player]; if (me && o.team != me->team) continue;
        V2 p = v.w2s(pg.p); float fr = pg.t / 3.0f; float ph = std::fmod(G.time * 1.5f, 1.f);
        cv.circle(p, (10 + 40 * ph) * k, lw * 1.5f, withA(o.col, (1 - ph) * fr), 1);
        FONT.draw(cv, "!", p - V2(0, 36 * k), std::max(12.f, 28 * k), withA(o.col, fr), 1);
    }
}

static void drawCursorAndGhost(Canvas& cv, const View& v, const Player& p, bool isMe) {
    float k = v.s, lw = std::max(1.2f, 1.8f * G.cv.px * (isMe ? 1 : 0.8f));
    V2 c = v.w2s(p.cur);
    if (isMe && p.hover < 0 && !p.bigMap) {
        int t = BUILDS[p.sel];
        const char* why = canPlace(p.team, t, p.cur);
        bool ok = !why || std::string(why) == "NOT ENOUGH MINERALS";
        Col gc = why ? (ok ? Col(255, 200, 90) : C_BAD) : G.teams[p.team].col;
        for (auto& s : G.st) if (!s.dead && s.team == p.team && s.connected && dist(s.p, p.cur) <= linkDist(t, s.t)) cv.dash(c, v.w2s(s.p), 6 * k, 5 * k, std::max(0.8f, 1.1f * k), withA(gc, 0.5f), G.time * 30 * k);
        float rg = rangeOf(t, 1); if (rg > 0) cv.circle(c, rg * k, std::max(0.8f, 1.2f * k), withA(gc, 0.35f), 0.4f);
        if (t == S_MINER) { int a = astInRange(p.cur, rg, p.team); if (a >= 0) cv.dash(c, v.w2s(G.ast[a].p), 5 * k, 4 * k, std::max(0.8f, 1.2f * k), withA(C_MIN, 0.7f), G.time * 30); }
        Struct g; g.t = t; g.p = p.cur; g.team = p.team; g.built = 1; g.powered = true; g.aim = -PI / 2; g.maxhp = g.hp = 1;
        drawStruct(cv, v, g, gc, 0.55f, true);
    }
    float r = (isMe ? 16 : 11) * G.cv.px * (v.vp.h < G.H * 0.75f ? 0.75f : 1);
    Col cc = p.col; if (!isMe) cc = withA(cc, 0.8f);
    float rot = G.time * 1.2f;
    for (int i = 0; i < 4; i++) { float a = rot + i * PI / 2; cv.line(c + fromAng(a) * r * 0.45f, c + fromAng(a) * r, lw, cc); }
    cv.circle(c, r * 0.18f, lw, cc, 1, 8);
    char lab[8]; snprintf(lab, sizeof lab, p.cpu && !p.view ? "CPU" : "P%d", p.idx + 1);
    FONT.draw(cv, lab, c + V2(r * 0.9f, r * 0.6f), r * 0.75f, cc, 0, 0, 0.4f);
    if (isMe && p.yHold > 0) cv.arc(c, r * 1.4f, -PI / 2, -PI / 2 + 2 * PI * clampf(p.yHold / 0.6f, 0, 1), lw * 1.5f, C_MIN);
}

static void drawMinimap(Canvas& cv, float x, float y, float w, const Player* me, float u) {
    int vt = me ? me->team : -1;
    float h = w * G.WH / G.WW, s = w / G.WW;
    cv.rect(x, y, w, h, Col(6, 9, 16, 210)); cv.rectLine(x, y, w, h, std::max(1.f, 1.2f * u), withA(Col(90, 120, 170), 0.7f), 0.3f);
    auto M = [&](V2 p) { return V2(x + clampf(p.x, 0, G.WW) * s, y + clampf(p.y, 0, G.WH) * s); };
    float d = std::max(1.5f, 2.2f * u);
    bool cry = vt >= 0 && G.teams[vt].crystal;
    for (auto& a : G.ast) if (!a.dead) { V2 q = M(a.p); float rr = std::max(1.2f, a.r * s); cv.rect(q.x - rr / 2, q.y - rr / 2, rr, rr, a.minerals > 0 ? withA(C_MIN, 0.55f) : cry && a.crystal > 0 ? withA(C_CRY, 0.55f) : withA(C_AST, 0.4f)); }
    for (int t = 0; t < 4; t++) { Team& T = G.teams[t]; if (!T.used || !seen(vt, t)) continue; for (auto& l : T.links) { const Struct& a = G.st[l.first]; const Struct& b = G.st[l.second]; if (!a.dead && !b.dead) cv.line(M(a.p), M(b.p), std::max(0.8f, u * 0.8f), withA(T.col, 0.35f), 0); } }
    for (auto& f : G.fi) if (!f.dead && fighterSeen(vt, f)) { V2 q = M(f.p); cv.rect(q.x - d * 0.4f, q.y - d * 0.4f, d * 0.8f, d * 0.8f, withA(mixc(G.teams[f.team].col, C_WHITE, 0.3f), 0.9f)); }
    for (auto& st : G.st) if (!st.dead && seen(vt, st.team)) { V2 q = M(st.p); float sz = st.t == S_HQ ? d * 2.2f : d; cv.rect(q.x - sz / 2, q.y - sz / 2, sz, sz, withA(G.teams[st.team].col, st.powered ? 1 : 0.4f)); }
    for (auto& e : G.en) if (!e.dead) { V2 q = M(e.p); float sz = e.t == E_MOTHER ? d * 3 : d; cv.rect(q.x - sz / 2, q.y - sz / 2, sz, sz, C_ALIEN); }
    float bl = 0.5f + 0.5f * std::sin(G.time * 10);
    for (auto& wn : G.warns) { V2 q = M(wn.p); cv.circle(q, 5 * u, std::max(1.f, u), withA(C_ALIEN, bl), 0.5f, 10); }
    for (auto& pg : G.pings) { if (me && G.pl[pg.player].team != me->team) continue; V2 q = M(pg.p); cv.circle(q, 4 * u + 4 * u * std::fmod(G.time * 2, 1.f), std::max(1.f, u), G.pl[pg.player].col, 0.5f, 10); }
    for (auto& p : G.pl) if (p.joined && (!me || p.team == me->team)) {
        V2 q = M(p.cur); cv.rect(q.x - d, q.y - d, d * 2, d * 2, p.col);
        if (p.view) { float vs = viewScale(p); float hw = p.vp.w * 0.5f / vs, hh = p.vp.h * 0.5f / vs; V2 a = M(p.cam - V2(hw, hh)), b = M(p.cam + V2(hw, hh)); cv.rectLine(a.x, a.y, b.x - a.x, b.y - a.y, std::max(0.8f, u * 0.8f), withA(p.col, p.idx == (me ? me->idx : -1) ? 0.8f : 0.35f), 0); }
    }
}

static std::string mmss(float t) { int s = (int)t; char b[16]; snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60); return b; }

// Button label for prompts, matching the device (Xbox letters, Nintendo letters by position, keyboard keys)
static const char* btnLab(int devId, int b) {
    const Dev* d = nullptr; for (auto& x : G.devs) if (x.id == devId) d = &x;
    if (devId == -1) { switch (b) { case B_A: return "SPACE"; case B_B: return "BKSP"; case B_X: return "U"; case B_Y: return "R"; case B_LB: return "Q"; case B_RB: return "E"; case B_START: return "ESC"; case B_BACK: return "TAB"; } return "?"; }
    bool nin = d && d->nintendo;
    switch (b) { case B_A: return nin ? "B" : "A"; case B_B: return nin ? "A" : "B"; case B_X: return nin ? "Y" : "X"; case B_Y: return nin ? "X" : "Y"; case B_LB: return nin ? "L" : "LB"; case B_RB: return nin ? "R" : "RB"; case B_START: return nin ? "+" : "START"; case B_BACK: return nin ? "-" : "BACK"; }
    return "?";
}

// build bar layout (shared by drawing and mouse clicks)
struct Bar { float slot, gap, sx, sy; };
static Bar barOf(const SDL_Rect& vp) {
    float u = hudU(vp); Bar b; b.slot = 66 * u; b.gap = 7 * u; float total = NBUILD * b.slot + (NBUILD - 1) * b.gap;
    if (total > vp.w * 0.94f) { float sc = vp.w * 0.94f / total; b.slot *= sc; b.gap *= sc; total = vp.w * 0.94f; }
    b.sx = vp.x + (vp.w - total) / 2; b.sy = vp.y + vp.h - b.slot - 34 * u; return b;
}
static int barSlotAt(const SDL_Rect& vp, V2 m) {
    Bar b = barOf(vp); if (m.y < b.sy || m.y > b.sy + b.slot) return -1;
    for (int i = 0; i < NBUILD; i++) { float x = b.sx + i * (b.slot + b.gap); if (m.x >= x && m.x <= x + b.slot) return i; }
    return -1;
}

static void drawObjective(Canvas& cv, const Player& p, float u, float leftEdge, float rightEdge) {
    const SDL_Rect& vp = p.vp; float cx = (leftEdge + rightEdge) * 0.5f, y = vp.y + 14 * u; Team& T = G.teams[p.team]; float room = rightEdge - leftEdge;
    std::string line; float prog = -1;
    switch (G.M.obj) {
    case OBJ_MINE: line = "MINE " + std::to_string((int)T.mined) + " / " + std::to_string((int)G.M.target); prog = T.mined / G.M.target; break;
    case OBJ_SURVIVE: line = "SURVIVE " + mmss(std::max(0.f, G.M.target - G.time)); prog = G.time / G.M.target; break;
    case OBJ_MOTHER: if (!G.motherSpawned) line = "MOTHERSHIP IN " + mmss(std::max(0.f, G.M.target - G.time)); else { line = "DESTROY THE MOTHERSHIP"; Enemy* m = enemyByUid(G.motherUid); if (m) prog = 1 - m->hp / m->maxhp; } break;
    case OBJ_ENDLESS: line = "TIME " + mmss(G.time) + "   WAVE " + std::to_string(G.waveN); break;
    case OBJ_RAID: { int alive = 0, found = 0; for (int t = 0; t < G.nTeams; t++) if (G.teams[t].alive && t != p.team) { alive++; if (T.found >> t & 1) found++; }
        line = "RIVALS " + std::to_string(alive) + "   FOUND " + std::to_string(found) + " / " + std::to_string(alive); break; }
    case OBJ_VERSUS: { int alive = 0; for (int t = 0; t < G.nTeams; t++) if (G.teams[t].alive && t != p.team) alive++; line = "RIVALS " + std::to_string(alive) + "   MINED " + std::to_string((int)T.mined) + " / " + std::to_string((int)G.M.target); prog = T.mined / G.M.target; break; }
    }
    float fs = std::min(20 * u, room / std::max(1.f, (float)line.size()));
    FONT.draw(cv, line.c_str(), V2(cx, y), fs, C_WHITE, 1);
    if (prog >= 0) { float w = std::min(260 * u, room * 0.9f); cv.rect(cx - w / 2, y + 28 * u, w, 4 * u, Col(255, 255, 255, 40)); cv.rect(cx - w / 2, y + 28 * u, w * clampf(prog, 0, 1), 4 * u, withA(T.col, 0.9f)); }
    if (raid()) {
        int nf = 0; for (auto& f : G.fi) if (!f.dead && f.team == p.team) nf++;
        char pw[16]; snprintf(pw, sizeof pw, "%.1f", ftrPower());
        std::string fl = "FIGHTERS " + std::to_string(nf) + "   POWER X" + pw + "   TIME " + mmss(G.time);
        if (T.order >= 0) fl += "   ATTACK ORDER";
        FONT.draw(cv, fl.c_str(), V2(cx, y + 40 * u), 13 * u, withA(T.order >= 0 ? T.col : C_DIM, 0.9f), 1);
        return;
    }
    std::string wl = G.waveT < 6 || G.alertT > 0 ? "WAVE INCOMING" : "NEXT WAVE " + mmss(G.waveT);
    Col wc = G.waveT < 6 || G.alertT > 0 ? withA(C_ALIEN, 0.6f + 0.4f * std::sin(G.time * 10)) : withA(C_DIM, 0.9f);
    FONT.draw(cv, wl.c_str(), V2(cx, y + 40 * u), 13 * u, wc, 1);
}

static void drawHud(Canvas& cv, Player& p) {
    const SDL_Rect& vp = p.vp; float u = hudU(vp); Team& T = G.teams[p.team];
    float x = vp.x + 18 * u, y = vp.y + 16 * u;
    // resources panel
    cv.rect(x - 8 * u, y - 8 * u, 290 * u, 76 * u, Col(5, 8, 14, 170));
    V2 dm(x + 9 * u, y + 11 * u); V2 dmp[4] = { dm + V2(0, -9) * u, dm + V2(7, 0) * u, dm + V2(0, 9) * u, dm + V2(-7, 0) * u };
    cv.poly(dmp, 4, 1.6f * u, C_MIN); cv.line(dmp[1], dmp[3], u, withA(C_MIN, 0.6f), 0);
    char buf[64]; snprintf(buf, sizeof buf, "%d", (int)T.minerals);
    FONT.draw(cv, buf, V2(x + 26 * u, y), 22 * u, C_WHITE);
    FONT.draw(cv, "MINERALS", V2(x + 38 * u + Font::width(buf, 22 * u), y + 8 * u), 11 * u, withA(C_MIN, 0.8f));
    float by = y + 38 * u;
    V2 bolt[5] = { V2(x + 11, by - 3) , V2(x + 5, by + 9), V2(x + 11, by + 8), V2(x + 7, by + 19), V2(x + 15, by + 5) };
    for (auto& q : bolt) q = V2(x + (q.x - x) * u, by + (q.y - by) * u - 6 * u);
    cv.poly(bolt, 5, 1.5f * u, C_EN, 1, false);
    float bw = 200 * u, bh = 10 * u, bx = x + 26 * u; float fr = T.ecap > 0 ? T.energy / T.ecap : 0;
    cv.rect(bx, by - 2 * u, bw, bh, Col(255, 255, 255, 30)); cv.rect(bx, by - 2 * u, bw * fr, bh, fr < 0.15f ? withA(C_BAD, 0.85f + 0.15f * std::sin(G.time * 12)) : withA(C_EN, 0.85f));
    float net = T.eprod - T.drainRate;
    snprintf(buf, sizeof buf, "%d/%d", (int)T.energy, (int)T.ecap); FONT.draw(cv, buf, V2(bx, by + 13 * u), 12 * u, withA(C_WHITE, 0.85f));
    snprintf(buf, sizeof buf, "%s%.1f/S", net >= 0 ? "+" : "", net); FONT.draw(cv, buf, V2(bx + bw, by + 13 * u), 12 * u, net >= 0 ? C_GOOD : C_BAD, 2);
    // minimap
    float mw = std::min(vp.w * 0.2f, 300 * u * 1.3f); float mh = mw * G.WH / G.WW;
    drawObjective(cv, p, u, x + 290 * u, vp.x + vp.w - mw - 24 * u);
    if (p.bigMap) { mw = vp.w * 0.86f; mh = mw * G.WH / G.WW; if (mh > vp.h * 0.8f) { mh = vp.h * 0.8f; mw = mh * G.WW / G.WH; } drawMinimap(cv, vp.x + (vp.w - mw) / 2, vp.y + (vp.h - mh) / 2, mw, &p, u * 1.4f); }
    else drawMinimap(cv, vp.x + vp.w - mw - 14 * u, vp.y + 14 * u, mw, &p, u);
    // off-screen threat arrows
    View v = viewOf(p); V2 cen(vp.x + vp.w * 0.5f, vp.y + vp.h * 0.5f);
    auto edgeArrow = [&](V2 w, Col c, float sz) {
        V2 q = v.w2s(w); if (q.x > vp.x && q.x < vp.x + vp.w && q.y > vp.y && q.y < vp.y + vp.h) return;
        V2 d = q - cen; float mx = vp.w * 0.5f - 30 * u, my = vp.h * 0.5f - 30 * u; float t = std::min(mx / std::max(std::fabs(d.x), 1e-3f), my / std::max(std::fabs(d.y), 1e-3f));
        V2 e = cen + d * t, n = norm(d), pp = perp(n);
        V2 tri[3] = { e + n * sz, e - n * sz * 0.6f + pp * sz * 0.7f, e - n * sz * 0.6f - pp * sz * 0.7f };
        cv.fillPoly(tri, 3, withA(c, 0.5f)); cv.poly(tri, 3, 1.5f * u, c, 0.6f);
    };
    int shown = 0;
    for (auto& w : G.warns) edgeArrow(w.p, withA(C_ALIEN, 0.6f + 0.4f * std::sin(G.time * 10)), 16 * u);
    for (auto& e : G.en) if (!e.dead && shown < 24 && dist(e.p, p.cam) < 1600) { edgeArrow(e.p, C_ALIEN, (e.t == E_MOTHER ? 16 : 9) * u); shown++; }
    shown = 0;
    for (auto& f : G.fi) if (!f.dead && f.team != p.team && shown < 16 && dist(f.p, p.cam) < 1600 && fighterSeen(p.team, f)) { edgeArrow(f.p, G.teams[f.team].col, 8 * u); shown++; }
    for (auto& pg : G.pings) if (G.pl[pg.player].team == p.team && pg.player != p.idx) edgeArrow(pg.p, G.pl[pg.player].col, 12 * u);
    // build bar
    Bar bar = barOf(vp); float slot = bar.slot, gap = bar.gap, sx = bar.sx, sy = bar.sy;
    for (int i = 0; i < NBUILD; i++) {
        int t = BUILDS[i]; float ox = sx + i * (slot + gap); bool sel = i == p.sel; bool afford = T.minerals >= SD[t].cost;
        cv.rect(ox, sy, slot, slot, sel ? Col(20, 30, 45, 230) : Col(6, 9, 15, 200));
        cv.rectLine(ox, sy, slot, slot, (sel ? 2.2f : 1.0f) * u, sel ? p.col : withA(C_DIM, 0.5f), sel ? 1 : 0);
        View iv; iv.vp = { (int)ox, (int)sy, (int)slot, (int)(slot * 0.8f) }; iv.cam = V2(0, 0); iv.s = slot * 0.36f / (SD[t].r + 8);
        Struct g; g.t = t; g.p = V2(0, 0); g.team = p.team; g.built = 1; g.powered = true; g.aim = -PI / 2; g.maxhp = g.hp = 1;
        drawStruct(cv, iv, g, afford ? (sel ? p.col : T.col) : C_DIM, afford ? 1 : 0.55f, true);
        snprintf(buf, sizeof buf, "%d", SD[t].cost);
        FONT.draw(cv, buf, V2(ox + slot / 2, sy + slot - 15 * u * slot / (66 * u)), 11 * u * slot / (66 * u), afford ? C_MIN : C_BAD, 1, 0, 0.3f);
        snprintf(buf, sizeof buf, "%d", (i + 1) % 10);
        if (p.dev == -1) FONT.draw(cv, buf, V2(ox + 4 * u, sy + 4 * u), 9 * u, withA(C_DIM, 0.8f), 0, 0, 0);
    }
    int t = BUILDS[p.sel];
    snprintf(buf, sizeof buf, "%s - %s", SD[t].name, SD[t].blurb);
    FONT.draw(cv, buf, V2(vp.x + vp.w * 0.5f, sy - 22 * u), 14 * u, withA(C_WHITE, 0.9f), 1);
    char hint[200];
    if (p.dev == -9) snprintf(hint, sizeof hint, "CPU");
    else if (p.dev == -1) snprintf(hint, sizeof hint, raid() ? "WASD SCROLL   CLICK BUILD   CLICK ICON OR 1-0 PICK   U UPGRADE   HOLD R RECYCLE   RIGHT-CLICK ATTACK   TAB MAP" : "WASD SCROLL   CLICK BUILD   CLICK ICON OR 1-0 PICK   U UPGRADE   HOLD R RECYCLE   RIGHT-CLICK PING   TAB MAP");
    else snprintf(hint, sizeof hint, raid() ? "%s/%s SELECT   %s BUILD   %s UPGRADE   HOLD %s RECYCLE   %s PING / ATTACK   %s MAP" : "%s/%s SELECT   %s BUILD   %s UPGRADE   HOLD %s RECYCLE   %s PING   %s MAP", btnLab(p.dev, B_LB), btnLab(p.dev, B_RB), btnLab(p.dev, B_A), btnLab(p.dev, B_X), btnLab(p.dev, B_Y), btnLab(p.dev, B_B), btnLab(p.dev, B_BACK));
    float hs = std::min(11 * u, vp.w * 0.95f / std::max(1.f, (float)strlen(hint)));
    FONT.draw(cv, hint, V2(vp.x + vp.w * 0.5f, vp.y + vp.h - 20 * u), hs, withA(C_DIM, 0.95f), 1, 0, 0);
    // hover info
    if (p.hover >= 0) {
        const Struct& s = G.st[p.hover]; bool own = s.team == p.team;
        float pw = 300 * u, ph = 112 * u, px = vp.x + vp.w - pw - 14 * u, py = sy - ph - 44 * u;
        cv.rect(px, py, pw, ph, Col(5, 8, 14, 215)); cv.rectLine(px, py, pw, ph, 1.2f * u, withA(G.teams[s.team].col, 0.7f), 0.3f);
        snprintf(buf, sizeof buf, "%s  LV %d", SD[s.t].name, s.lv); FONT.draw(cv, buf, V2(px + 12 * u, py + 10 * u), 17 * u, G.teams[s.team].col);
        snprintf(buf, sizeof buf, "HP %d/%d", (int)s.hp, (int)s.maxhp); FONT.draw(cv, buf, V2(px + 12 * u, py + 36 * u), 12 * u, C_WHITE);
        if (s.t == S_CARRIER && s.built >= 1) { int fl = 0; for (auto& f : G.fi) if (f.home == p.hover) fl++; snprintf(buf, sizeof buf, "FIGHTERS %d/%d", fl, carrierCap(s.lv)); FONT.draw(cv, buf, V2(px + pw * 0.5f, py + 36 * u), 12 * u, withA(C_WHITE, 0.85f), 1); }
        const char* stt = s.built < 1 ? (s.connected ? "BUILDING" : "NOT CONNECTED") : !s.powered ? "NO POWER" : s.t == S_MINER ? (s.mining ? "MINING" : (s.ast < 0 ? "NO ASTEROID" : "LOW ENERGY")) : s.t == S_SHIELD ? "SHIELD UP" : "ONLINE";
        if (s.t == S_RESEARCH && s.built >= 1) { if (G.teams[s.team].crystal) stt = "CRYSTAL FOUND"; else if (s.powered) { snprintf(buf, sizeof buf, "RESEARCH %d%%", (int)(100 * s.charge)); stt = buf; } }
        if (s.t == S_SHIELD && s.powered) { snprintf(buf, sizeof buf, "SHIELD %d%%", (int)(100 * s.charge / (160 * lvm(s.lv)))); stt = buf; }
        FONT.draw(cv, stt, V2(px + pw - 12 * u, py + 36 * u), 12 * u, !s.powered && s.built >= 1 ? C_BAD : C_GOOD, 2);
        if (own) {
            char l1[64], l2[64];
            if (s.lv < 3 && s.built >= 1) snprintf(l1, sizeof l1, "%s UPGRADE  %d", btnLab(p.dev, B_X), upgCost(s.t, s.lv)); else snprintf(l1, sizeof l1, s.lv >= 3 ? "MAX LEVEL" : "UPGRADE AFTER BUILD");
            FONT.draw(cv, l1, V2(px + 12 * u, py + 60 * u), 12 * u, s.lv < 3 && s.built >= 1 && T.minerals >= upgCost(s.t, s.lv) ? C_MIN : C_DIM);
            if (s.t != S_HQ) { snprintf(l2, sizeof l2, "HOLD %s RECYCLE  +%d", btnLab(p.dev, B_Y), (int)recycleValue(s)); FONT.draw(cv, l2, V2(px + 12 * u, py + 84 * u), 12 * u, withA(C_WHITE, 0.7f)); }
        } else { FONT.draw(cv, "RIVAL STRUCTURE", V2(px + 12 * u, py + 60 * u), 12 * u, C_BAD);
            if (raid()) { snprintf(buf, sizeof buf, "%s SEND FIGHTERS HERE", btnLab(p.dev, B_B)); FONT.draw(cv, buf, V2(px + 12 * u, py + 84 * u), 12 * u, withA(C_WHITE, 0.7f)); } }
    }
    if (p.toastT > 0) FONT.draw(cv, p.toast.c_str(), V2(vp.x + vp.w * 0.5f, sy - 52 * u), 18 * u, withA(Col(255, 210, 120), std::min(1.f, p.toastT * 2)), 1);
    if (!T.alive && rivals()) FONT.draw(cv, "COMMAND LOST", V2(vp.x + vp.w * 0.5f, vp.y + vp.h * 0.4f), 46 * u, withA(C_BAD, 0.9f), 1);
    if (p.devLost) FONT.draw(cv, "CONTROLLER DISCONNECTED", V2(vp.x + vp.w * 0.5f, vp.y + vp.h * 0.5f), 26 * u, C_BAD, 1);
}

static void renderGame(Canvas& cv) {
    std::vector<Player*> views; for (auto& q : G.pl) if (q.joined && q.view) views.push_back(&q);
    for (Player* p : views) {
        View v = viewOf(*p);
        SDL_RenderSetClipRect(G.ren, &p->vp);
        drawStars(cv, v); cv.flush(G.ren);
        drawWorld(cv, v, p);
        for (auto& o : G.pl) if (o.joined && o.team == p->team && &o != p) drawCursorAndGhost(cv, v, o, false);
        drawCursorAndGhost(cv, v, *p, true);
        cv.flush(G.ren);
        drawHud(cv, *p); cv.flush(G.ren);
    }
    if (views.size() == 3) { // fourth quadrant shows the whole belt
        SDL_Rect q = { G.W / 2 + 2, G.H / 2 + 2, G.W - G.W / 2 - 2, G.H - G.H / 2 - 2 };
        SDL_RenderSetClipRect(G.ren, &q);
        float u = hudU(q); float mw = q.w * 0.86f; float mh = mw * G.WH / G.WW; if (mh > q.h * 0.72f) { mh = q.h * 0.72f; mw = mh * G.WW / G.WH; }
        cv.rect((float)q.x, (float)q.y, (float)q.w, (float)q.h, Col(3, 5, 10, 255));
        drawMinimap(cv, q.x + (q.w - mw) / 2, q.y + (q.h - mh) / 2 + 14 * u, mw, raid() ? views[0] : nullptr, u * 1.3f);
        FONT.draw(cv, G.M.name, V2(q.x + q.w * 0.5f, q.y + 18 * u), 24 * u, C_WHITE, 1);
        cv.flush(G.ren);
    }
    SDL_RenderSetClipRect(G.ren, nullptr);
    if (views.size() >= 2) { // split lines
        Col sc(60, 80, 120, 255); float g = (float)std::max(2, G.H / 360);
        if (views.size() == 2) cv.rect(G.W / 2 - g / 2, 0, g, (float)G.H, sc);
        else { cv.rect(G.W / 2 - g / 2, 0, g, (float)G.H, sc); cv.rect(0, G.H / 2 - g / 2, (float)G.W, g, sc); }
        cv.flush(G.ren);
    }
}
