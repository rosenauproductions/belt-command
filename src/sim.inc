// Belt Command - simulation (included from main.cpp)

static Enemy* enemyByUid(int uid) { for (auto& e : G.en) if (!e.dead && e.uid == uid) return &e; return nullptr; }
static Fighter* fighterByUid(int uid) { for (auto& f : G.fi) if (!f.dead && f.uid == uid) return &f; return nullptr; }
static void markFound(int t, int o);
static void discoverCrystal(int team) {
    Team& T = G.teams[team]; if (T.crystal) return;
    T.crystal = true;
    bool first = !G.crystalOut;
    if (first) { // the first discovery refills the whole belt
        G.crystalOut = true;
        for (auto& a : G.ast) {
            if (a.dead) { bool blocked = false; for (auto& s : G.st) if (!s.dead && dist(s.p, a.p) < a.r * 0.95f + s.r() + 4) blocked = true; if (blocked) continue; a.dead = false; a.minerals = 0; }
            a.crystal = a.maxMin * 0.7f;
        }
    }
    for (auto& a : G.ast) if (!a.dead && a.crystal > 0) ring(a.p, a.r * 0.5f, a.r * 1.4f, withA(C_CRY, 0.7f), 0.8f);
    for (auto& q : G.pl) if (q.joined && q.team == team) toast(q, first ? "NEW MINERAL: CRYSTAL - ALL ASTEROIDS REFRESHED" : "CRYSTAL FOUND - MINE IT FOR 1.5X");
    bool human = false; for (auto& q : G.pl) if (q.joined && q.team == team && !q.cpu) human = true;
    if (human) AU.play(SND_WIN, 0.5f);
    printf("research: team %d found crystal at %.0fs%s\n", team, G.time, first ? " (belt refreshed)" : "");
}

static void destroyStruct(int id, bool explodeIt = true) {
    Struct& s = G.st[id]; if (s.dead) return;
    s.dead = true; Team& T = G.teams[s.team]; T.dirty = true;
    if (explodeIt) {
        explode(s.p, s.r(), T.col); shakeAt(s.p, s.t == S_HQ ? 18 : 5 + s.r() * 0.2f);
        sfxAt(s.t == S_HQ ? SND_BOOM_L : SND_BOOM_S, s.p, s.t == S_HQ ? 1.0f : 0.7f);
    }
    if (s.t == S_HQ && T.alive) {
        T.alive = false; T.lostAt = G.time;
        explode(s.p, 70, C_WHITE);
        for (auto& o : G.st) if (!o.dead && o.team == s.team) o.doomT = R.range(0.3f, 3.2f);
    }
}
static void damageStruct(int id, float dmg, V2 from, int byTeam = -1) {
    Struct& s = G.st[id]; if (s.dead) return;
    if (byTeam >= 0 && byTeam != s.team) markFound(s.team, byTeam); // raid: being hit tells you where they are
    for (auto& sh : G.st) {
        if (sh.dead || sh.t != S_SHIELD || sh.team != s.team || !sh.powered || sh.charge <= 1) continue;
        if (dist(sh.p, s.p) > rangeOf(S_SHIELD, sh.lv)) continue;
        float ab = std::min(sh.charge, dmg); sh.charge -= ab; dmg -= ab;
        V2 hit = s.p + norm(from - s.p) * (s.r() + 6);
        ring(hit, 2, 16, withA(G.teams[s.team].col, 0.9f), 0.25f); sfxAt(SND_SHIELD, hit, 0.4f, 60);
        if (dmg <= 0) return;
    }
    s.hp -= dmg; s.flash = 0.12f;
    if (s.hp <= 0) destroyStruct(id);
}
static void damageEnemy(Enemy& e, float dmg, int team) {
    if (e.dead) return;
    e.hp -= dmg; e.flash = 0.08f;
    if (e.hp <= 0) {
        e.dead = true; float sz = ED[e.t].r;
        Col c = e.t == E_BOMBER ? Col(255, 130, 40) : e.t == E_SWARM ? Col(255, 80, 180) : C_ALIEN;
        explode(e.p, sz, c); sfxAt(e.t == E_MOTHER ? SND_BOOM_L : SND_BOOM_S, e.p, e.t == E_MOTHER ? 1.0f : 0.45f, 40);
        if (e.t == E_MOTHER) { shakeAt(e.p, 25); for (int i = 0; i < 6; i++) explode(e.p + fromAng(i * 1.05f) * 40, 30, c); }
        if (team >= 0 && team < 4) G.teams[team].kills++;
    }
}
static void damageFighter(Fighter& f, float dmg, int team) {
    if (f.dead) return;
    f.hp -= dmg; f.flash = 0.08f;
    if (f.hp <= 0) {
        f.dead = true; explode(f.p, 9, G.teams[f.team].col); sfxAt(SND_BOOM_S, f.p, 0.4f, 40);
        G.teams[f.team].ftrLost++;
        if (team >= 0 && team < 4) G.teams[team].kills++;
    }
}

// ---------------------------------------------------------------- player actions
static int hoverAt(int team, V2 p, bool anyTeam) {
    int best = -1; float bd = 1e9;
    for (int i = 0; i < (int)G.st.size(); i++) {
        auto& s = G.st[i]; if (s.dead || (!anyTeam && s.team != team)) continue;
        float d = dist(s.p, p); if (d < s.r() + 10 && d < bd) { bd = d; best = i; }
    }
    return best;
}
static bool tryBuild(Player& p, int t, V2 q, bool quiet) {
    const char* why = canPlace(p.team, t, q);
    if (why) {
        if (!quiet) { if (std::string(why) == "NOT ENOUGH MINERALS") toast(p, "NEED " + std::to_string(SD[t].cost) + " MINERALS"); else toast(p, why); AU.play(SND_ERR, 0.6f); }
        return false;
    }
    Team& T = G.teams[p.team]; T.minerals -= SD[t].cost; T.built++;
    int id = addStruct(t, p.team, q, 0); G.st[id].builder = p.idx;
    recomputeNet(p.team);
    ring(q, SD[t].r, SD[t].r + 18, withA(T.col, 0.8f), 0.3f);
    sfxAt(SND_PLACE, q, 0.7f);
    return true;
}
static bool tryUpgrade(Player& p, int id, bool quiet) {
    if (id < 0) return false;
    Struct& s = G.st[id]; Team& T = G.teams[p.team];
    if (s.dead || s.team != p.team) return false;
    if (s.built < 1) { if (!quiet) { toast(p, "STILL BUILDING"); AU.play(SND_ERR, 0.5f); } return false; }
    if (s.lv >= 3) { if (!quiet) { toast(p, "MAX LEVEL"); AU.play(SND_ERR, 0.5f); } return false; }
    int c = upgCost(s.t, s.lv);
    if (T.minerals < c) { if (!quiet) { toast(p, "NEED " + std::to_string(c) + " MINERALS"); AU.play(SND_ERR, 0.5f); } return false; }
    T.minerals -= c; s.invested += c; s.lv++;
    float nm = SD[s.t].hp * lvm(s.lv); s.hp += nm - s.maxhp; s.maxhp = nm;
    ring(s.p, s.r(), s.r() + 30, C_WHITE, 0.4f); sparks(s.p, 12, T.col, 120);
    sfxAt(SND_UPGRADE, s.p, 0.8f);
    return true;
}
static float recycleValue(const Struct& s) { return s.built < 1 ? s.invested * 0.9f : s.invested * 0.5f * clampf(s.hp / s.maxhp, 0, 1); }
static void recycle(Player& p, int id) {
    Struct& s = G.st[id]; if (s.dead || s.t == S_HQ || s.team != p.team) return;
    G.teams[p.team].minerals += recycleValue(s);
    s.dead = true; G.teams[p.team].dirty = true;
    ring(s.p, s.r() + 10, 2, C_MIN, 0.35f); sparks(s.p, 10, C_MIN, 80);
    sfxAt(SND_RECYCLE, s.p, 0.8f);
}

// ---------------------------------------------------------------- spawning
static void spawnEnemy(int t, V2 p) {
    if (G.en.size() > 260 && t != E_MOTHER) return;
    Enemy e; e.t = t; e.uid = G.uidNext++; e.p = p; e.hp = e.maxhp = ED[t].hp; e.cd = R.range(0.5f, 1.5f) * std::max(ED[t].cd, 0.5f);
    V2 c(G.WW / 2, G.WH / 2); e.v = norm(c - p) * ED[t].speed * 0.6f; e.ang = std::atan2(e.v.y, e.v.x); e.orbit = R.f() < 0.5f ? -1.f : 1.f;
    e.retarget = 0; e.spawnCd = 6;
    if (t == E_MOTHER) G.motherUid = e.uid;
    G.en.push_back(e);
}
static V2 edgePoint() {
    int side = R.i(4); float m = 70;
    switch (side) {
    case 0: return V2(R.range(0, G.WW), -m);
    case 1: return V2(G.WW + m, R.range(0, G.WH));
    case 2: return V2(R.range(0, G.WW), G.WH + m);
    default: return V2(-m, R.range(0, G.WH));
    }
}
static int humansAndCoopCpus() { int n = 0; for (auto& q : G.pl) if (q.joined) n++; return std::max(1, n); }
static void spawnGroup(V2 p, float budget) {
    int w = G.waveN;
    while (budget > 0) {
        float roll = R.f(); int t = E_SCOUT;
        if (G.M.swarmy && w >= 1 && roll < 0.5f) t = E_SWARM;
        else if (w >= 4 && roll < 0.14f) t = E_BOMBER;
        else if (w >= 2 && roll < 0.42f) t = E_FIGHTER;
        else if (w >= 1 && roll < 0.62f) t = E_SWARM;
        float cost = t == E_SCOUT ? 2 : t == E_FIGHTER ? 4 : t == E_BOMBER ? 9 : 1;
        if (t == E_SWARM) { int k = 4 + R.i(3); for (int i = 0; i < k; i++) spawnEnemy(E_SWARM, p + fromAng(R.range(0, 6.28f)) * R.range(0, 50)); budget -= cost * k; }
        else { spawnEnemy(t, p + fromAng(R.range(0, 6.28f)) * R.range(0, 70)); budget -= cost; }
    }
}
static void startWaveWarnings() {
    float scale = G.mode == MODE_VERSUS ? 1 + 0.3f * (G.nTeams - 2) : 1 + 0.55f * (humansAndCoopCpus() - 1);
    int w = G.waveN;
    float budget = (5 + w * 4.2f + w * w * 0.15f) * G.M.diff * scale;
    int npts = budget > 40 ? 3 : budget > 18 ? 2 : 1;
    for (int i = 0; i < npts; i++) G.warns.push_back({ edgePoint(), 6.0f, budget / npts, false });
    AU.play(SND_ALERT, 0.55f); G.alertT = 2.5f;
}

// ---------------------------------------------------------------- world update
static void updateStructs(float dt) {
    for (int t = 0; t < 4; t++) if (G.teams[t].used && G.teams[t].dirty) recomputeNet(t);
    // energy production and capacity
    for (int t = 0; t < 4; t++) {
        Team& T = G.teams[t]; if (!T.used) continue;
        T.eprod = 0; T.ecap = 0;
        for (auto& s : G.st) if (!s.dead && s.team == t && s.powered) {
            if (s.t == S_HQ) { T.eprod += 3 * lvm(s.lv); T.ecap += 150 * lvm(s.lv); }
            else if (s.t == S_SOLAR) T.eprod += 2.4f * lvm(s.lv) * s.supply;
            else if (s.t == S_BATTERY) T.ecap += 120 * lvm(s.lv);
        }
        if (!T.alive) T.eprod = 0;
        T.energy = std::min(T.ecap, T.energy + T.eprod * dt);
    }
    for (auto& a : G.ast) a.miners = 0;
    for (auto& s : G.st) if (!s.dead && s.t == S_MINER && s.ast >= 0) G.ast[s.ast].miners++;

    for (int i = 0; i < (int)G.st.size(); i++) {
        Struct& s = G.st[i]; if (s.dead) continue;
        Team& T = G.teams[s.team]; SPENDER = i;
        if (s.t == S_SOLAR && s.powered) s.useAcc += 2.4f * lvm(s.lv) * s.supply * dt;
        s.flash = std::max(0.f, s.flash - dt); s.beamT = std::max(0.f, s.beamT - dt); s.cd -= dt; s.spin += dt;
        if (s.doomT > 0) { s.doomT -= dt; if (s.doomT <= 0) { destroyStruct(i); continue; } }
        if (s.built < 1) {
            if (s.connected && T.alive && spend(T, 1.5f * dt * s.supply)) {
                s.built += dt * s.supply / SD[s.t].build; s.hp = std::min(s.maxhp, s.hp + s.maxhp * 0.9f * dt * s.supply / SD[s.t].build);
                if (s.built >= 1) { s.built = 1; T.dirty = true; ring(s.p, s.r(), s.r() + 22, T.col, 0.4f); sfxAt(SND_DONE, s.p, 0.5f, 80); }
            }
            continue;
        }
        if (!s.powered) { s.mining = false; continue; }
        float m = lvm(s.lv), sup = s.supply; // sup < 1: the links feeding this structure are overloaded
        switch (s.t) {
        case S_MINER: {
            s.chk -= dt;
            if (s.ast < 0 || astLeft(G.ast[s.ast], s.team) <= 0 || s.chk <= 0) { s.ast = astInRange(s.p, rangeOf(S_MINER, 1), s.team); s.chk = 2; }
            s.mining = false;
            if (s.ast >= 0) {
                Asteroid& a = G.ast[s.ast];
                s.aim = std::atan2(a.p.y - s.p.y, a.p.x - s.p.x);
                float rate = 3.5f * m * dt * sup;
                if (spend(T, 1.6f * m * dt * sup)) {
                    bool cry = a.minerals <= 0; float& pool = cry ? a.crystal : a.minerals;
                    float got = std::min(rate, pool); pool -= got; if (cry) got *= CRYSTAL_VALUE;
                    T.minerals += got; T.mined += got; s.mining = true; s.cryBeam = cry; s.useAcc += got; s.spin += dt * 6;
                    if (R.f() < dt * 4) { V2 sp = a.p + fromAng(R.range(0, 6.28f)) * a.r * 0.8f; addPart(P_DOT, sp, (s.p - sp) * 1.4f, 0.6f, cry ? C_CRY : C_MIN, 3); }
                    if (a.minerals <= 0 && a.crystal <= 0 && (G.crystalOut || !cry)) { a.dead = true; explode(a.p, a.r * 0.6f, C_AST); for (int k = 0; k < 6; k++) addPart(P_DEBRIS, a.p, fromAng(R.range(0, 6.28f)) * R.range(10, 50), 2.0f, C_AST, R.range(6, 14)); sfxAt(SND_BOOM_S, a.p, 0.5f); }
                }
            }
            break;
        }
        case S_LASER: case S_MISSILE: {
            float range = rangeOf(s.t, s.lv);
            V2 tp; bool has = false; int eIdx = -1, sIdx = -1; float bd = 1e9;
            for (int k = 0; k < (int)G.en.size(); k++) { auto& e = G.en[k]; if (e.dead) continue; float d = dist(e.p, s.p) - ED[e.t].r; if (d < range && d < bd) { bd = d; eIdx = k; } }
            int fIdx = -1;
            if (eIdx < 0) for (int k = 0; k < (int)G.fi.size(); k++) { auto& f = G.fi[k]; if (f.dead || f.team == s.team) continue; float d = dist(f.p, s.p) - 7; if (d < range && d < bd) { bd = d; fIdx = k; } }
            if (eIdx >= 0) { tp = G.en[eIdx].p; has = true; }
            else if (fIdx >= 0) { tp = G.fi[fIdx].p; has = true; }
            else if (rivals()) {
                for (int k = 0; k < (int)G.st.size(); k++) { auto& o = G.st[k]; if (o.dead || o.team == s.team || !G.teams[o.team].alive) continue; float d = dist(o.p, s.p) - o.r(); if (d < range && d < bd) { bd = d; sIdx = k; } }
                if (sIdx >= 0) { tp = G.st[sIdx].p; has = true; }
            }
            if (!has) break;
            float want = std::atan2(tp.y - s.p.y, tp.x - s.p.x); float da = angDiff(s.aim, want);
            float turn = 7 * dt; s.aim += clampf(da, -turn, turn);
            if (std::fabs(da) > 0.35f || s.cd > 0) break;
            if (s.t == S_LASER) {
                if (!spend(T, 4)) break;
                s.cd = 0.5f / sup; s.beamT = 0.1f;
                V2 muzzle = s.p + fromAng(s.aim) * 22;
                G.beams.push_back({ muzzle, tp, 0.12f, 0.12f, 2.2f, T.col });
                sparks(tp, 3, T.col, 90); sfxAt(SND_LASER, s.p, 0.35f, 45);
                if (eIdx >= 0) damageEnemy(G.en[eIdx], 9 * m, s.team); else if (fIdx >= 0) damageFighter(G.fi[fIdx], 9 * m, s.team); else damageStruct(sIdx, 9 * m * 0.7f, s.p, s.team);
            } else {
                if (T.minerals < 3 || !spend(T, 2)) break;
                T.minerals -= 3; s.cd = 1.8f / sup; s.beamT = 0.2f;
                for (int side = -1; side <= 1; side += 2) {
                    Bullet b; b.p = s.p + rotv(V2(16, side * 5.f), s.aim); b.v = fromAng(s.aim + side * 0.25f) * 160; b.dmg = 34 * m * 0.5f; b.life = 4; b.splash = 55; b.team = s.team; b.missile = true;
                    if (eIdx >= 0) b.tgtUid = G.en[eIdx].uid; else if (fIdx >= 0) b.tgtUid = G.fi[fIdx].uid; else b.tgtStruct = sIdx;
                    G.bl.push_back(b);
                }
                sfxAt(SND_MISSILE, s.p, 0.5f, 60);
            }
            break;
        }
        case S_REPAIR: {
            if (s.cd > 0) break; s.cd = 0.3f / sup;
            float range = rangeOf(S_REPAIR, s.lv); int first = -1;
            for (int pass = 0; pass < 2; pass++) {
                int bi = -1; float br = 0.999f;
                for (int k = 0; k < (int)G.st.size(); k++) { auto& o = G.st[k]; if (o.dead || o.team != s.team || o.built < 1 || k == i || k == first) continue; float rr = o.hp / o.maxhp; if (rr < br && dist(o.p, s.p) < range) { br = rr; bi = k; } }
                if (bi < 0) break;
                float amt = 12 * m * 0.3f;
                if (!spend(T, amt * 0.25f)) break;
                Struct& o = G.st[bi]; o.hp = std::min(o.maxhp, o.hp + amt); first = bi;
                G.beams.push_back({ s.p, o.p, 0.25f, 0.25f, 1.4f, C_GOOD });
            }
            break;
        }
        case S_SHIELD: {
            float cap = 160 * m;
            if (s.charge < cap) { float want = std::min(cap - s.charge, 15 * m * dt * sup); if (spend(T, want)) s.charge += want; }
            break;
        }
        case S_RESEARCH: {
            if (T.crystal) break;
            if (spend(T, 2.0f * dt * sup)) { s.charge += dt * sup / researchTime(s.lv); s.spin += dt * 3; }
            if (s.charge >= 1) discoverCrystal(s.team);
            break;
        }
        case S_CARRIER: {
            int have = 0, out = 0; for (auto& f : G.fi) if (!f.dead && f.home == i) { have++; if (f.sortie) out++; }
            s.ast = have - out; // fighters on deck, shown on the carrier
            // launch the squadron together once it is full (or right away on an attack order)
            if (rivals() && out == 0 && have > 0 && (have >= carrierCap(s.lv) || T.order >= 0)) for (auto& f : G.fi) if (!f.dead && f.home == i) { f.sortie = true; f.retarget = 0; }
            if (have >= carrierCap(s.lv)) { s.charge = 0; break; }
            if (s.charge < 1) { if (spend(T, 1.2f * dt * sup)) s.charge = std::min(1.f, s.charge + dt * sup / FTR_BUILD); }
            else if (T.minerals >= FTR_COST) {
                T.minerals -= FTR_COST; s.charge = 0;
                Fighter f; f.team = s.team; f.uid = G.uidNext++; f.home = i; f.ang = s.aim; f.p = s.p + fromAng(f.ang) * 26; f.v = fromAng(f.ang) * 80;
                f.hp = f.maxhp = FTR_HP * m; f.dmg = FTR_DMG * m; f.orbitA = R.range(0, 6.28f);
                G.fi.push_back(f); ring(f.p, 4, 18, withA(T.col, 0.8f), 0.3f); sfxAt(SND_PLACE, f.p, 0.4f, 80);
            }
            break;
        }
        }
    }
    SPENDER = -1;
    // multipath flow: every structure sends what it uses, makes and mines toward Command over all its downhill links,
    // splitting by each link's spare room (so busy links shed load to parallel ones); then overload -> supply
    for (auto& s : G.st) { if (s.dead) continue; s.flow += (s.useAcc / dt - s.flow) * std::min(1.f, dt * 2.5f); s.useAcc = 0; }
    static std::vector<float> inflow, raw, cong;
    for (int t = 0; t < 4; t++) {
        Team& T = G.teams[t]; if (!T.used || T.links.empty()) continue;
        inflow.assign(G.st.size(), 0.f); raw.assign(T.links.size(), 0.f); cong.assign(G.st.size(), 0.f);
        for (int u : T.flowOrder) {
            if (u == T.hq) continue;
            Struct& su = G.st[u]; float tot = su.flow + inflow[u], sw = 0;
            for (auto& e : T.adj[u]) if (G.st[e.first].hqDist < su.hqDist - 0.01f) { const Struct& v = G.st[e.first]; float cap = linkCap(su, v); sw += cap / (1 + T.linkFlow[e.second] / cap); }
            if (sw <= 0 || tot <= 0) continue;
            for (auto& e : T.adj[u]) if (G.st[e.first].hqDist < su.hqDist - 0.01f) {
                const Struct& v = G.st[e.first]; float cap = linkCap(su, v); float share = tot * (cap / (1 + T.linkFlow[e.second] / cap)) / sw;
                raw[e.second] += share; inflow[e.first] += share;
            }
        }
        float k = std::min(1.f, dt * 3);
        for (size_t j = 0; j < raw.size(); j++) T.linkFlow[j] += (raw[j] - T.linkFlow[j]) * k;
        // congestion, from Command outward: a structure is as starved as the (flow-weighted) worst link on its way in
        for (int q = (int)T.flowOrder.size() - 1; q >= 0; q--) {
            int u = T.flowOrder[q]; Struct& su = G.st[u];
            if (u == T.hq) { su.supply = 1; continue; }
            float wsum = 0, c = 0, cmax = 0; int nd = 0;
            for (auto& e : T.adj[u]) if (G.st[e.first].hqDist < su.hqDist - 0.01f) {
                const Struct& v = G.st[e.first]; float over = std::max(T.linkFlow[e.second] / linkCap(su, v), cong[e.first]);
                c += raw[e.second] * over; wsum += raw[e.second]; cmax = std::max(cmax, over); nd++;
            }
            cong[u] = wsum > 0 ? c / wsum : (nd ? cmax : 0);
            su.supply = clampf(1 / std::max(1.f, cong[u]), 0.35f, 1);
        }
    }
    for (int t = 0; t < 4; t++) { Team& T = G.teams[t]; T.drainRate += (T.drainAcc / dt - T.drainRate) * std::min(1.f, dt * 1.5f); T.drainAcc = 0; }
}

static int pickTarget(const Enemy& e) {
    int best = -1; float bd = 1e18f;
    for (int i = 0; i < (int)G.st.size(); i++) {
        auto& s = G.st[i]; if (s.dead || !G.teams[s.team].alive) continue;
        float d = dist2(s.p, e.p);
        if (e.t == E_BOMBER && (s.t == S_RELAY || s.t == S_SOLAR)) d *= 2.5f;
        if (e.t == E_MOTHER && s.t == S_HQ) d *= 0.5f;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
static void updateEnemies(float dt) {
    std::vector<V2> pending;
    for (auto& e : G.en) {
        if (e.dead) continue;
        const EDef& D = ED[e.t];
        e.flash = std::max(0.f, e.flash - dt); e.cd -= dt; e.retarget -= dt;
        if (e.retarget <= 0 || e.tgt < 0 || G.st[e.tgt].dead) { e.tgt = pickTarget(e); e.retarget = R.range(0.6f, 1.2f); }
        V2 want(0, 0); float dTo = 1e9;
        if (e.tgt >= 0) {
            Struct& s = G.st[e.tgt]; V2 to = s.p - e.p; dTo = len(to); V2 dir = to / std::max(dTo, 1.f);
            if (e.t == E_BOMBER) want = dir;
            else if (dTo > D.range * 0.85f) want = dir;
            else want = perp(dir) * e.orbit * 0.9f + dir * ((dTo - D.range * 0.7f) / D.range);
            if (e.t == E_BOMBER && dTo < s.r() + D.r + 4) {
                e.dead = true; explode(e.p, 30, Col(255, 140, 40)); shakeAt(e.p, 9); sfxAt(SND_BOOM_L, e.p, 0.7f);
                for (int i = 0; i < (int)G.st.size(); i++) { auto& o = G.st[i]; if (!o.dead && dist(o.p, e.p) < 75 + o.r()) damageStruct(i, D.dmg, e.p); }
                continue;
            }
            if (D.range > 0 && dTo < D.range && e.cd <= 0) {
                e.cd = D.cd * R.range(0.85f, 1.15f);
                V2 aimp = s.p; float fd = std::min(dTo, D.range * 0.8f);
                for (auto& f : G.fi) if (!f.dead && dist(f.p, e.p) < fd) { fd = dist(f.p, e.p); aimp = f.p + f.v * (fd / 330); }
                auto shoot = [&](float spread) {
                    Bullet b; b.p = e.p + fromAng(e.ang) * D.r; b.v = rotv(norm(aimp - b.p), spread) * 330; b.dmg = D.dmg; b.life = D.range / 330 * 1.6f; b.team = -1; G.bl.push_back(b);
                };
                if (e.t == E_MOTHER) { shoot(-0.18f); shoot(0); shoot(0.18f); } else shoot(R.range(-0.05f, 0.05f));
                sfxAt(SND_ALIEN, e.p, e.t == E_MOTHER ? 0.5f : 0.25f, 50);
            }
        } else want = norm(V2(G.WW / 2, G.WH / 2) - e.p) * 0.3f;
        if (e.t == E_MOTHER) { e.spawnCd -= dt; if (e.spawnCd <= 0) { e.spawnCd = 9; for (int k = 0; k < 4; k++) pending.push_back(e.p + fromAng(R.range(0, 6.28f)) * 70); } }
        V2 sep(0, 0);
        for (auto& o : G.en) { if (&o == &e || o.dead) continue; V2 d = e.p - o.p; float l2 = len2(d); float mn = D.r + ED[o.t].r + 10; if (l2 < mn * mn && l2 > 0.01f) sep += d / std::sqrt(l2) * (1 - std::sqrt(l2) / mn); }
        V2 dv = (want + sep * 1.5f); if (len(dv) > 1) dv = norm(dv);
        e.v += (dv * D.speed - e.v) * std::min(1.f, dt * (e.t == E_MOTHER ? 0.6f : 2.5f));
        e.p += e.v * dt;
        if (len2(e.v) > 4) { float a = std::atan2(e.v.y, e.v.x); if (e.tgt >= 0 && dTo < D.range && e.t != E_BOMBER) { V2 to = G.st[e.tgt].p - e.p; a = std::atan2(to.y, to.x); } e.ang += clampf(angDiff(e.ang, a), -6 * dt, 6 * dt); }
        if (R.f() < dt * 20) { V2 tail = e.p - fromAng(e.ang) * D.r; addPart(P_DOT, tail, -fromAng(e.ang) * 40 + V2(R.range(-10, 10), R.range(-10, 10)), 0.3f, Col(255, 140, 60), D.r * 0.25f); }
    }
    for (auto& p : pending) spawnEnemy(E_SWARM, p);
}
// ---------------------------------------------------------------- carrier fighters
static void markFound(int t, int o) {
    if (!raid() || t == o || t < 0 || o < 0 || (G.teams[t].found >> o & 1)) return;
    G.teams[t].found |= 1u << o;
    for (auto& q : G.pl) if (q.joined && q.team == t) { toast(q, "RIVAL BASE FOUND - P" + std::to_string(o + 1)); }
    bool human = false; for (auto& q : G.pl) if (q.joined && q.team == t && !q.cpu) human = true;
    if (human) AU.play(SND_ALERT, 0.5f);
    printf("raid: team %d found team %d at %.0fs\n", t, o, G.time);
}
static void updateSight(float dt) {
    if (!raid()) return;
    G.seeT -= dt; if (G.seeT > 0) return; G.seeT = 0.3f;
    for (int t = 0; t < G.nTeams; t++) {
        if (!G.teams[t].alive) continue;
        for (int o = 0; o < G.nTeams; o++) {
            if (o == t || !G.teams[o].alive || (G.teams[t].found >> o & 1)) continue;
            bool hit = false;
            for (auto& a : G.st) { if (a.dead || a.team != o) continue;
                for (auto& b : G.st) if (!b.dead && b.team == t && dist2(a.p, b.p) < SIGHT * SIGHT) { hit = true; break; }
                if (!hit) for (auto& f : G.fi) if (!f.dead && f.team == t && dist2(a.p, f.p) < FTR_SIGHT * FTR_SIGHT) { hit = true; break; }
                if (hit) break; }
            if (hit) markFound(t, o);
        }
    }
}
// can team t see this point right now (its structures or fighters nearby)?
static bool inSight(int t, V2 p) {
    for (auto& b : G.st) if (!b.dead && b.team == t && dist2(p, b.p) < SIGHT * SIGHT) return true;
    for (auto& f : G.fi) if (!f.dead && f.team == t && dist2(p, f.p) < FTR_SIGHT * FTR_SIGHT) return true;
    return false;
}
static void updateFighters(float dt) {
    for (int t = 0; t < 4; t++) { Team& T = G.teams[t]; T.orderT -= dt; if (T.order >= 0 && (T.orderT <= 0 || G.st[T.order].dead)) T.order = -1; }
    for (auto& f : G.fi) {
        if (f.dead) continue;
        Struct& home = G.st[f.home];
        if (home.dead || !G.teams[f.team].alive) { damageFighter(f, 1e9f, -1); continue; }
        Team& T = G.teams[f.team];
        f.flash = std::max(0.f, f.flash - dt); f.cd -= dt; f.retarget -= dt;
        // is the current target still good?
        V2 tp; bool has = false; float tr = 7;
        if (f.tk == 1) { Enemy* e = enemyByUid(f.tid); if (e) { tp = e->p; tr = ED[e->t].r; has = true; } }
        else if (f.tk == 2) { Fighter* o = fighterByUid(f.tid); if (o) { tp = o->p; has = true; } }
        else if (f.tk == 3) { Struct& o = G.st[f.tid]; if (!o.dead && G.teams[o.team].alive) { tp = o.p; tr = o.r(); has = true; } }
        if (!has) f.tk = 0;
        if (f.retarget <= 0 || !has) {
            f.retarget = R.range(0.35f, 0.6f); f.tk = 0; has = false;
            float defR = rangeOf(S_CARRIER, home.lv), bd = 260;
            for (auto& o : G.fi) if (!o.dead && o.team != f.team && dist(o.p, f.p) < bd && (!raid() || inSight(f.team, o.p))) { bd = dist(o.p, f.p); f.tk = 2; f.tid = o.uid; tp = o.p; tr = 7; has = true; }
            if (!has) { bd = 1e9f; for (auto& e : G.en) if (!e.dead && dist(e.p, home.p) < defR) { float d = dist(e.p, f.p); if (d < bd) { bd = d; f.tk = 1; f.tid = e.uid; tp = e.p; tr = ED[e.t].r; has = true; } } }
            if (!has && rivals() && f.sortie) {
                if (T.order >= 0 && seen(f.team, G.st[T.order].team)) { f.tk = 3; f.tid = T.order; tp = G.st[T.order].p; tr = G.st[T.order].r(); has = true; }
                else {
                    bd = 1e18f;
                    for (int k = 0; k < (int)G.st.size(); k++) {
                        auto& o = G.st[k]; if (o.dead || o.team == f.team || !G.teams[o.team].alive) continue;
                        if (raid() ? !seen(f.team, o.team) : dist(o.p, home.p) > defR) continue;
                        float d = dist2(o.p, f.p); if (o.t == S_LASER || o.t == S_MISSILE) d *= 0.7f; /* knock out the guns first */ if (o.t == S_RESEARCH && !G.teams[o.team].crystal) d *= 0.5f; /* and stop their research */
                        if (d < bd) { bd = d; f.tk = 3; f.tid = k; tp = o.p; tr = o.r(); has = true; }
                    }
                }
            }
        }
        V2 want;
        float dTo = has ? dist(tp, f.p) : 0;
        if (has) {
            V2 dir = (tp - f.p) / std::max(dTo, 1.f);
            if (dTo > FTR_RANGE * 0.9f + tr) want = dir;
            else want = perp(dir) * 0.9f + dir * ((dTo - tr - FTR_RANGE * 0.55f) / FTR_RANGE);
            float face = std::atan2(dir.y, dir.x);
            if (dTo < FTR_RANGE + tr && f.cd <= 0 && std::fabs(angDiff(f.ang, face)) < 0.9f) {
                f.cd = FTR_CD * R.range(0.85f, 1.15f);
                V2 hit = tp - dir * tr * 0.7f;
                G.beams.push_back({ f.p + fromAng(f.ang) * 8, hit, 0.09f, 0.09f, 1.5f, T.col });
                sparks(hit, 2, T.col, 70); sfxAt(SND_LASER, f.p, 0.18f, 70);
                float dmg = f.dmg * ftrPower();
                if (f.tk == 1) { Enemy* e = enemyByUid(f.tid); if (e) damageEnemy(*e, dmg, f.team); }
                else if (f.tk == 2) { Fighter* o = fighterByUid(f.tid); if (o) damageFighter(*o, dmg, f.team); }
                else if (f.tk == 3) damageStruct(f.tid, dmg, f.p, f.team);
            }
        } else {
            f.sortie = false; // nothing left to hit: fly home and wait for the squadron
            f.orbitA += dt * 1.1f; V2 spot = home.p + fromAng(f.orbitA) * 62;
            want = (spot - f.p) / 60.f; if (len(want) > 1) want = norm(want);
            f.hp = std::min(f.maxhp, f.hp + 6 * dt); // repairs on deck
        }
        V2 sep;
        for (auto& o : G.fi) { if (&o == &f || o.dead) continue; V2 d = f.p - o.p; float l2 = len2(d); if (l2 < 400 && l2 > 0.01f) sep += d / std::sqrt(l2) * (1 - std::sqrt(l2) / 20); }
        V2 dv = want + sep; if (len(dv) > 1) dv = norm(dv);
        f.v += (dv * FTR_SPEED - f.v) * std::min(1.f, dt * 3.2f);
        f.p += f.v * dt;
        f.p.x = clampf(f.p.x, 20, G.WW - 20); f.p.y = clampf(f.p.y, 20, G.WH - 20);
        float a = len2(f.v) > 4 ? std::atan2(f.v.y, f.v.x) : f.ang;
        if (has && dTo < FTR_RANGE + tr) a = std::atan2(tp.y - f.p.y, tp.x - f.p.x);
        f.ang += clampf(angDiff(f.ang, a), -7 * dt, 7 * dt);
        if (R.f() < dt * 14) addPart(P_DOT, f.p - fromAng(f.ang) * 7, -fromAng(f.ang) * 30, 0.25f, mixc(T.col, C_WHITE, 0.3f), 2.2f);
    }
    G.fi.erase(std::remove_if(G.fi.begin(), G.fi.end(), [](const Fighter& f) { return f.dead; }), G.fi.end());
}

static void updateBullets(float dt) {
    for (auto& b : G.bl) {
        if (b.dead) continue;
        b.life -= dt; if (b.life <= 0) { b.dead = true; if (b.missile) { explode(b.p, 6, G.teams[b.team].col); } continue; }
        if (b.missile) {
            V2 tp; bool has = false;
            if (b.tgtUid >= 0) { Enemy* e = enemyByUid(b.tgtUid); Fighter* f = e ? nullptr : fighterByUid(b.tgtUid); if (e) { tp = e->p; has = true; } else if (f) { tp = f->p; has = true; } else b.tgtUid = -1; }
            if (b.tgtUid < 0 && b.tgtStruct >= 0) { if (!G.st[b.tgtStruct].dead) { tp = G.st[b.tgtStruct].p; has = true; } else b.tgtStruct = -1; }
            if (!has && b.tgtUid < 0 && b.tgtStruct < 0) {
                float bd = 350; for (auto& e : G.en) if (!e.dead && dist(e.p, b.p) < bd) { bd = dist(e.p, b.p); b.tgtUid = e.uid; }
            }
            float spd = len(b.v); spd = std::min(spd + 500 * dt, 330.f);
            V2 dir = norm(b.v);
            if (has) { V2 want = norm(tp - b.p); float a = std::atan2(dir.y, dir.x), w = std::atan2(want.y, want.x); a += clampf(angDiff(a, w), -5 * dt, 5 * dt); dir = fromAng(a); }
            b.v = dir * spd; b.p += b.v * dt;
            if (R.f() < dt * 40) addPart(P_DOT, b.p, b.v * -0.1f, 0.35f, Col(255, 200, 120), 2.5f);
            bool hit = false;
            for (auto& e : G.en) if (!e.dead && dist(e.p, b.p) < ED[e.t].r + 4) { hit = true; break; }
            if (!hit) for (auto& f : G.fi) if (!f.dead && f.team != b.team && dist(f.p, b.p) < 11) { hit = true; break; }
            if (!hit && rivals()) for (auto& s : G.st) if (!s.dead && s.team != b.team && G.teams[s.team].alive && dist(s.p, b.p) < s.r() + 2) { hit = true; break; }
            if (hit) {
                b.dead = true; explode(b.p, 12, G.teams[b.team].col); sfxAt(SND_HIT, b.p, 0.5f);
                for (auto& e : G.en) if (!e.dead && dist(e.p, b.p) < b.splash + ED[e.t].r) damageEnemy(e, b.dmg, b.team);
                for (auto& f : G.fi) if (!f.dead && f.team != b.team && dist(f.p, b.p) < b.splash + 7) damageFighter(f, b.dmg, b.team);
                if (rivals()) for (int i = 0; i < (int)G.st.size(); i++) { auto& s = G.st[i]; if (!s.dead && s.team != b.team && dist(s.p, b.p) < b.splash + s.r()) damageStruct(i, b.dmg * 0.7f, b.p, b.team); }
            }
        } else {
            b.p += b.v * dt;
            for (auto& f : G.fi) if (!f.dead && dist2(f.p, b.p) < 100) { b.dead = true; damageFighter(f, b.dmg, -1); sparks(b.p, 3, C_ALIEN, 70); break; }
            if (b.dead) continue;
            for (int i = 0; i < (int)G.st.size(); i++) {
                auto& s = G.st[i]; if (s.dead) continue;
                if (dist2(s.p, b.p) < s.r() * s.r()) { b.dead = true; damageStruct(i, b.dmg, b.p - b.v * 0.05f); sparks(b.p, 3, C_ALIEN, 70); break; }
            }
        }
    }
}
static void updateFx(float dt) {
    for (auto& q : G.parts) { q.life -= dt; q.p += q.v * dt; q.rot += q.spin * dt; if (q.kind == P_SPARK || q.kind == P_DEBRIS) q.v *= std::pow(0.15f, dt); }
    G.parts.erase(std::remove_if(G.parts.begin(), G.parts.end(), [](const Part& q) { return q.life <= 0; }), G.parts.end());
    for (auto& b : G.beams) b.life -= dt;
    G.beams.erase(std::remove_if(G.beams.begin(), G.beams.end(), [](const Beam& b) { return b.life <= 0; }), G.beams.end());
    for (auto& p : G.pings) p.t -= dt;
    G.pings.erase(std::remove_if(G.pings.begin(), G.pings.end(), [](const Ping& p) { return p.t <= 0; }), G.pings.end());
    for (auto& a : G.ast) a.rot += a.spin * dt;
}
static void updateWaves(float dt) {
    if (G.M.obj == OBJ_MOTHER && !G.motherWarned && G.time >= G.M.target - 10) {
        G.motherWarned = true; G.warns.push_back({ edgePoint(), 10, 0, true }); AU.play(SND_ALERT, 0.8f); G.alertT = 4;
    }
    G.waveT -= dt;
    if (G.waveT <= 6 && G.waveT + dt > 6) startWaveWarnings();
    if (G.waveT <= 0) { G.waveN++; G.waveT = std::max(16.f, G.M.interval - G.waveN * 1.1f); }
    for (auto& w : G.warns) {
        w.t -= dt;
        if (w.t <= 0) { if (w.mother) { spawnEnemy(E_MOTHER, w.p); G.motherSpawned = true; } else spawnGroup(w.p, w.budget); }
    }
    G.warns.erase(std::remove_if(G.warns.begin(), G.warns.end(), [](const Warn& w) { return w.t <= 0; }), G.warns.end());
}

// ---------------------------------------------------------------- CPU players
static bool findSpot(Player& p, int t, V2& out, V2 focus, bool hasFocus) {
    Team& T = G.teams[p.team]; std::vector<int> mine;
    for (int i = 0; i < (int)G.st.size(); i++) if (!G.st[i].dead && G.st[i].team == p.team && G.st[i].connected) mine.push_back(i);
    if (mine.empty()) return false;
    V2 hq = G.st[T.hq].p; float best = -1e18f; bool found = false;
    for (int k = 0; k < 70; k++) {
        Struct& s = G.st[mine[R.i((int)mine.size())]];
        float maxd = linkDist(t, s.t) * 0.93f, mind = s.r() + SD[t].r + 8; if (maxd <= mind) continue;
        V2 q = s.p + fromAng(R.range(0, 6.28f)) * R.range(mind, maxd);
        if (canPlace(p.team, t, q)) continue;
        float sc = R.range(0, 30);
        switch (t) {
        case S_MINER: { int a = astInRange(q, rangeOf(S_MINER, 1), p.team); if (a < 0) continue; sc += std::min(astLeft(G.ast[a], p.team), 800.f) / (1 + G.ast[a].miners * 2.0f) - dist(q, G.ast[a].p) * 0.5f; break; }
        case S_SOLAR: case S_BATTERY: sc -= dist(q, hq); for (auto& o : G.st) if (!o.dead && o.team == p.team && (o.t == S_LASER || o.t == S_MISSILE) && dist(o.p, q) < rangeOf(o.t, o.lv)) sc += 25; break;
        case S_LASER: case S_MISSILE: {
            int cover = 0, near = 0;
            for (auto& o : G.st) if (!o.dead && o.team == p.team) { float d = dist(o.p, q); if (d < rangeOf(t, 1)) cover++; if ((o.t == S_LASER || o.t == S_MISSILE) && d < 110) near++; }
            sc += cover * 15 - near * 70 + dist(q, hq) * 0.15f;
            if (hasFocus) sc -= dist(q, focus) * 0.4f;
            break;
        }
        case S_RESEARCH: sc -= dist(q, hq); break;
        case S_REPAIR: case S_SHIELD: { int cover = 0; for (auto& o : G.st) if (!o.dead && o.team == p.team && dist(o.p, q) < rangeOf(t, 1)) cover++; sc += cover * 20; break; }
        case S_RELAY: sc -= hasFocus ? dist(q, focus) : -dist(q, hq) * 0.2f; break;
        case S_CARRIER: { int near = 0; for (auto& o : G.st) if (!o.dead && o.team == p.team && (o.t == S_LASER || o.t == S_MISSILE) && dist(o.p, q) < 200) near++; sc += near * 30 - dist(q, hq) * 0.2f; if (hasFocus) sc -= dist(q, focus) * 0.3f; break; }
        }
        if (sc > best) { best = sc; out = q; found = true; }
    }
    return found;
}
static void cpuThink(Player& p, float dt) {
    Team& T = G.teams[p.team]; if (!T.alive || T.hq < 0) return;
    if (p.hasGoal) {
        p.cur += (p.goal - p.cur) * std::min(1.f, dt * 6); p.goalWait -= dt;
        if (dist(p.cur, p.goal) < 8 || p.goalWait <= 0) {
            if (p.goalType == -2) tryUpgrade(p, p.goalStruct, true);
            else { for (int k = 0; k < NBUILD; k++) if (BUILDS[k] == p.goalType) p.sel = k; tryBuild(p, p.goalType, p.goal, true); }
            p.hasGoal = false; p.thinkT = R.range(0.3f, 0.9f);
        }
        return;
    }
    p.thinkT -= dt; if (p.thinkT > 0) return; p.thinkT = 0.5f;
    int cnt[S_N] = {}; for (auto& s : G.st) if (!s.dead && s.team == p.team) cnt[s.t]++;
    int turrets = cnt[S_LASER] + cnt[S_MISSILE]; float tmin = G.time / 60; float net = T.eprod - T.drainRate;
    bool threat = G.waveN > 0 || G.waveT < 25 || !G.en.empty();
    V2 focus; bool hasFocus = false; float fd = 1e9;
    V2 hq = G.st[T.hq].p;
    for (auto& e : G.en) if (!e.dead && dist(e.p, hq) < fd) { fd = dist(e.p, hq); focus = e.p; hasFocus = true; }
    for (auto& w : G.warns) if (dist(w.p, hq) < fd) { fd = dist(w.p, hq); focus = w.p; hasFocus = true; }
    // raid: guard toward found rivals, explore toward the nearest rival not found yet
    V2 explore; bool hasExplore = false;
    if (raid()) {
        threat = tmin > 1.5f || T.found;
        for (auto& f : G.fi) if (!f.dead && f.team != p.team && dist(f.p, hq) < 1200 && dist(f.p, hq) < fd) { fd = dist(f.p, hq); focus = f.p; hasFocus = true; threat = true; }
        float ed = 1e9;
        for (int o = 0; o < G.nTeams; o++) { if (o == p.team || !G.teams[o].alive) continue; V2 oh = G.st[G.teams[o].hq].p; float d = dist(oh, hq);
            if (T.found >> o & 1) { if (!hasFocus) { focus = hq + norm(oh - hq) * 300; hasFocus = true; } }
            else if (d < ed) { ed = d; explore = oh; hasExplore = true; } } // CPUs guess the corners, same as a person would
        p.exploreT -= 0.5f;
    }
    int carrierWant = raid() ? (cnt[S_MINER] >= 3 ? 1 + (int)(tmin / 2.5f) : 0) : (turrets >= 5 ? turrets / 6 : 0);
    int choice;
    if (cnt[S_MINER] == 0) choice = S_MINER;
    else if (raid() && hasExplore && p.exploreT <= 0 && T.minerals >= 120 && cnt[S_MINER] >= 3) { p.exploreT = R.range(6, 11); /* keep a reserve so miners still get built */ V2 spot; if (findSpot(p, S_RELAY, spot, explore, true)) { p.goal = spot; p.goalType = S_RELAY; p.hasGoal = true; p.goalWait = 2.5f; return; } choice = S_MINER; }
    else if (net < 1.5f && cnt[S_SOLAR] < 2 + cnt[S_MINER] * 1.5f + turrets * 0.7f) choice = S_SOLAR;
    else if (threat && turrets < 1 + (int)(tmin * (raid() ? 1.0f : 1.7f))) choice = (turrets % 3 == 2 && T.minerals > 220) ? S_MISSILE : S_LASER;
    else if (cnt[S_CARRIER] < carrierWant) choice = S_CARRIER;
    else if (cnt[S_RESEARCH] == 0 && !T.crystal && cnt[S_MINER] >= 4 && tmin > 2.5f) choice = S_RESEARCH;
    else if (cnt[S_RESEARCH] > 0 && !T.crystal && T.minerals > 300) { for (int i = 0; i < (int)G.st.size(); i++) { auto& s = G.st[i]; if (!s.dead && s.team == p.team && s.t == S_RESEARCH && s.built >= 1 && s.lv < 3) { p.goal = s.p; p.goalType = -2; p.goalStruct = i; p.hasGoal = true; p.goalWait = 2; return; } } choice = -2; }
    else if (cnt[S_MINER] < 2 + (int)(tmin * 0.9f)) choice = S_MINER;
    else if (cnt[S_BATTERY] < cnt[S_SOLAR] / 4) choice = S_BATTERY;
    else if (turrets >= 4 && cnt[S_REPAIR] < turrets / 5) choice = S_REPAIR;
    else if (turrets >= 6 && cnt[S_SHIELD] < turrets / 7) choice = S_SHIELD;
    else choice = -2;
    if (choice == -2) {
        int bi = -1; int bl = 9;
        for (int i = 0; i < (int)G.st.size(); i++) { auto& s = G.st[i]; if (s.dead || s.team != p.team || s.built < 1 || s.lv >= 3) continue; if (s.t != S_LASER && s.t != S_MISSILE && s.t != S_MINER && s.t != S_SOLAR && s.t != S_CARRIER) continue; if (s.lv < bl && T.minerals - 80 >= upgCost(s.t, s.lv)) { bl = s.lv; bi = i; } }
        if (bi >= 0) { p.goal = G.st[bi].p; p.goalType = -2; p.goalStruct = bi; p.hasGoal = true; p.goalWait = 2; return; }
        choice = S_MINER;
    }
    if (T.minerals < SD[choice].cost) { p.thinkT = 0.8f; return; }
    V2 spot;
    bool ok = findSpot(p, choice, spot, focus, hasFocus && (choice == S_LASER || choice == S_MISSILE || choice == S_CARRIER));
    if (!ok && choice == S_MINER && T.minerals >= SD[S_RELAY].cost) {
        int ta = -1; float bd = 1e9;
        for (int i = 0; i < (int)G.ast.size(); i++) { auto& a = G.ast[i]; if (astLeft(a, p.team) < 100 || a.miners > 0) continue; float d = dist(a.p, hq); if (d < bd) { bd = d; ta = i; } }
        if (ta >= 0 && findSpot(p, S_RELAY, spot, G.ast[ta].p, true)) { choice = S_RELAY; ok = true; }
    }
    if (ok) { p.goal = spot; p.goalType = choice; p.hasGoal = true; p.goalWait = 2.5f; }
}

// ---------------------------------------------------------------- results
static void finish(int result, int winner, const std::string& msg) {
    if (G.result) return;
    G.result = result; G.winner = winner; G.resultMsg = msg; G.resultT = 0; G.resultSel = 0;
    printf("result: %d winner %d \"%s\" at %.0fs\n", result, winner, msg.c_str(), G.time);
    AU.play(result == 1 ? SND_WIN : SND_LOSE, 0.9f);
    if (result == 1 && G.mode == MODE_COOP) { G.opt.done |= 1u << G.mission; saveCfg(); }
    if (G.mode == MODE_ENDLESS && (int)G.time > G.opt.bestEndless) { G.opt.bestEndless = (int)G.time; saveCfg(); }
}
static void checkEnd() {
    if (G.result) return;
    if (rivals()) {
        int alive = 0, last = -1;
        for (int t = 0; t < G.nTeams; t++) if (G.teams[t].alive) { alive++; last = t; }
        if (!raid()) for (int t = 0; t < G.nTeams; t++) if (G.teams[t].alive && G.teams[t].mined >= G.M.target) { finish(1, t, "ECONOMIC VICTORY"); return; }
        if (alive <= 1) finish(1, last, alive == 1 ? "LAST COMMAND STANDING" : "NO SURVIVORS");
        return;
    }
    Team& T = G.teams[0];
    if (!T.alive) { finish(2, -1, G.mode == MODE_ENDLESS ? "SURVIVED " + std::to_string((int)G.time / 60) + ":" + (((int)G.time % 60) < 10 ? "0" : "") + std::to_string((int)G.time % 60) : "COMMAND DESTROYED"); return; }
    switch (G.M.obj) {
    case OBJ_MINE: if (T.mined >= G.M.target) finish(1, 0, "MINING QUOTA MET"); break;
    case OBJ_SURVIVE: if (G.time >= G.M.target) finish(1, 0, "YOU HELD THE BELT"); break;
    case OBJ_MOTHER: if (G.motherSpawned && !enemyByUid(G.motherUid)) finish(1, 0, "MOTHERSHIP DESTROYED"); break;
    }
}

static void stepWorld(float dt) {
    if (G.result) dt *= std::max(0.15f, 1 - G.resultT);
    G.time += dt;
    updateStructs(dt);
    if (!raid()) updateWaves(dt);
    updateEnemies(dt);
    updateFighters(dt);
    updateSight(dt);
    updateBullets(dt);
    updateFx(dt);
    G.en.erase(std::remove_if(G.en.begin(), G.en.end(), [](const Enemy& e) { return e.dead || e.p.x < -900 || e.p.y < -900 || e.p.x > G.WW + 900 || e.p.y > G.WH + 900; }), G.en.end());
    G.bl.erase(std::remove_if(G.bl.begin(), G.bl.end(), [](const Bullet& b) { return b.dead; }), G.bl.end());
    G.alertT = std::max(0.f, G.alertT - dt);
    checkEnd();
}

// ---------------------------------------------------------------- setup
static void layoutViews() {
    std::vector<Player*> v; for (auto& q : G.pl) if (q.joined && q.view) v.push_back(&q);
    int W = G.W, H = G.H, g = std::max(2, H / 360);
    int n = (int)v.size();
    if (n == 1) v[0]->vp = { 0, 0, W, H };
    else if (n == 2) { v[0]->vp = { 0, 0, W / 2 - g / 2, H }; v[1]->vp = { W / 2 + g / 2, 0, W - W / 2 - g / 2, H }; }
    else if (n >= 3) {
        SDL_Rect q[4] = { { 0, 0, W / 2 - g / 2, H / 2 - g / 2 }, { W / 2 + g / 2, 0, W - W / 2 - g / 2, H / 2 - g / 2 }, { 0, H / 2 + g / 2, W / 2 - g / 2, H - H / 2 - g / 2 }, { W / 2 + g / 2, H / 2 + g / 2, W - W / 2 - g / 2, H - H / 2 - g / 2 } };
        for (int i = 0; i < n; i++) v[i]->vp = q[i];
    }
}
static void startGame() {
    G.M = G.mode == MODE_RAID ? M_RAID : G.mode == MODE_VERSUS ? M_VERSUS : G.mode == MODE_ENDLESS ? M_ENDLESS : MISSIONS[G.mission];
    G.st.clear(); G.en.clear(); G.bl.clear(); G.parts.clear(); G.beams.clear(); G.warns.clear(); G.pings.clear(); G.fi.clear(); G.seeT = 0; G.crystalOut = false;
    G.time = 0; G.waveN = 0; G.waveT = G.M.first; G.motherUid = -1; G.motherWarned = G.motherSpawned = false;
    G.result = 0; G.paused = false; G.uidNext = 1;
    for (auto& t : G.teams) t = Team();
    R.s = (uint32_t)(SDL_GetTicks() * 2654435761u) | 1;
    if (getenv("BC_SEED")) R.s = (uint32_t)atoi(getenv("BC_SEED")) | 1;
    int idx = 0, nj = 0;
    for (auto& q : G.pl) if (q.joined) { q.team = rivals() ? idx : 0; idx++; nj++; }
    G.nTeams = rivals() ? idx : 1;
    for (int t = 0; t < G.nTeams; t++) {
        Team& T = G.teams[t]; T.used = true;
        T.col = PCOL[0];
        if (rivals()) for (auto& q : G.pl) if (q.joined && q.team == t) T.col = q.col;
        T.minerals = rivals() ? 220 : 200 + 80.f * (nj - 1); T.energy = 150;
    }
    genWorld();
    for (int t = 0; t < G.nTeams; t++) recomputeNet(t);
    for (auto& q : G.pl) if (q.joined) {
        V2 hq = G.st[G.teams[q.team].hq].p;
        q.cur = hq + V2(60.f * (q.idx % 2 ? 1 : -1), 60.f * (q.idx / 2 ? 1 : -1)); q.cam = hq; q.zoom = 0.85f; q.sel = 1; q.hover = -1; q.toastT = 0; q.shake = 0; q.hasGoal = false; q.thinkT = 1 + q.idx * 0.3f; q.yHold = 0; q.exploreT = R.range(15, 25);
    }
    layoutViews();
    G.state = ST_GAME; G.stateT = 0;
    printf("game: mode %s mission %s players %d teams %d world %.0fx%.0f asteroids %d\n", MODENAMES[G.mode], G.M.name, nj, G.nTeams, G.WW, G.WH, (int)G.ast.size());
}
