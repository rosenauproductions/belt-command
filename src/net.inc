// Belt Command - online play (included from main.cpp).
//
// Host-authoritative: the Batocera box runs the only real game. Online guests (the browser build, or another box)
// send their input; the host sends snapshots of what drawing needs plus effect events, about 15 times a second.
// The box also serves the browser build itself (http://<box>:8095/#join), and for internet play it can open a
// Cloudflare quick tunnel (cloudflared next to the game) so guests join through https://belt-command.vercel.app/#join=<code>.
// See docs/ONLINE_MULTIPLAYER_HANDOVER.md for the design.

#include <map>
#ifndef __EMSCRIPTEN__
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <dlfcn.h>
#include <cerrno>
#include <thread>
#include <mutex>
#include <atomic>
#include <fstream>
#include <sstream>
#else
#include <emscripten/websocket.h>
#endif

static const int NET_PROTO = 1, NET_PORT = 8095, NET_DISC_PORT = 8096, REMOTE_DEV = 2000;
static const char* WEB_SITE = "https://belt-command.vercel.app";
enum { MSG_JOIN = 'J', MSG_INPUT = 'I', MSG_WELCOME = 'W', MSG_SNAP = 'S' };

// from ui.inc
static Dev* devById(int id); static Dev* kbd(); static int joinedCount(); static void enterLobby(); static bool pr(const Dev& d, int b); static void usePresses();
static void drawPanel(Canvas& cv, float x, float y, float w, float h, Col edge, float u);
static void drawTitleBackdrop(Canvas& cv, float u);
static void menuList(Canvas& cv, const char** items, int n, int sel, float cx, float y, float size, float gap);
static int menuMouse(int& sel, bool& click);
static void layoutViews();
static std::string GAMEDIR = ".";

static std::string cleanName(const std::string& s, size_t maxLen = 12) {
    std::string o; for (char ch : s) { if (std::isalnum((unsigned char)ch) || ch == ' ' || ch == '-' || ch == '_' || ch == '.') o += (char)std::toupper((unsigned char)ch); if (o.size() >= maxLen) break; }
    while (!o.empty() && o.back() == ' ') o.pop_back(); while (!o.empty() && o[0] == ' ') o.erase(0, 1);
    return o;
}
static std::string randomToken() { static const char* H = "0123456789abcdef"; std::string t; uint32_t s = (uint32_t)SDL_GetPerformanceCounter() ^ (uint32_t)(uintptr_t)&t; Rng r; r.s = s | 1; for (int i = 0; i < 16; i++) t += H[r.i(16)]; return t; }
static std::string myName() { if (G.opt.name.empty()) return "GUEST"; return G.opt.name; }

// ================================================================ snapshot encoding (host) / decoding (guest)
static void recStruct(WBuf& w, const Struct& s) {
    w.u8(s.t); w.u8(s.team); w.u8(s.lv);
    w.u8((s.dead ? 1 : 0) | (s.connected ? 2 : 0) | (s.powered ? 4 : 0) | (s.mining ? 8 : 0) | (s.cryBeam ? 16 : 0));
    w.f32(s.p.x); w.f32(s.p.y);
    w.u16((int)std::lround(clampf(s.hp, 0, 65535))); w.u16((int)std::lround(clampf(s.maxhp, 0, 65535)));
    w.frac(s.built); w.f32(s.charge); w.ang(s.aim); w.i16(s.ast);
    w.u8((int)clampf(s.beamT * 1000, 0, 255)); w.u8((int)clampf(s.flash * 1000, 0, 255)); w.u8((int)clampf(s.cd * 100, 0, 255)); w.u16((int)clampf(s.invested, 0, 65535));
}
static void readStruct(RBuf& r, Struct& s) {
    s.t = std::min(r.u8(), S_N - 1); s.team = r.u8() & 3; s.lv = r.u8(); int f = r.u8();
    s.dead = f & 1; s.connected = f & 2; s.powered = f & 4; s.mining = f & 8; s.cryBeam = f & 16;
    s.p.x = r.f32(); s.p.y = r.f32(); s.hp = (float)r.u16(); s.maxhp = (float)std::max(1, r.u16());
    s.built = r.frac(); s.charge = r.f32(); s.aim = r.ang(); s.ast = r.i16();
    s.beamT = r.u8() / 1000.f; s.flash = r.u8() / 1000.f; s.cd = r.u8() / 100.f; s.invested = (float)r.u16();
}
static void recAstFull(WBuf& w, const Asteroid& a) {
    w.f32(a.p.x); w.f32(a.p.y); w.f32(a.r); w.f32(a.maxMin); w.f32(a.rot); w.f32(a.spin);
    w.u8((int)a.shape.size()); for (auto& v : a.shape) { w.i16((int)(v.x * 10000)); w.i16((int)(v.y * 10000)); }
    w.u8((int)a.flecks.size()); for (auto& v : a.flecks) { w.i16((int)(v.x * 10000)); w.i16((int)(v.y * 10000)); }
}
static void recAstDyn(WBuf& w, const Asteroid& a) { w.f32(a.minerals); w.f32(a.crystal); w.u8(a.dead ? 1 : 0); }
static void readAstFull(RBuf& r, Asteroid& a) {
    a.p.x = r.f32(); a.p.y = r.f32(); a.r = r.f32(); a.maxMin = r.f32(); a.rot = r.f32(); a.spin = r.f32();
    a.shape.resize(r.u8()); for (auto& v : a.shape) { v.x = r.i16() / 10000.f; v.y = r.i16() / 10000.f; }
    a.flecks.resize(r.u8()); for (auto& v : a.flecks) { v.x = r.i16() / 10000.f; v.y = r.i16() / 10000.f; }
}
static void readAstDyn(RBuf& r, Asteroid& a) { a.minerals = r.f32(); a.crystal = r.f32(); a.dead = r.u8() != 0; }
static int colIndex(Col c) { for (int i = 0; i < 4; i++) if (PCOL[i].r == c.r && PCOL[i].g == c.g && PCOL[i].b == c.b) return i; return 0; }
static std::string playerLabel(const Player& p) {
    if (p.remote) return p.name;
    if (p.cpu) return "CPU";
    if (p.dev == -1) return "KEYBOARD + MOUSE";
    Dev* d = devById(p.dev); std::string n = d ? d->name : "PAD"; for (auto& ch : n) ch = (char)toupper((unsigned char)ch); return n.substr(0, 20);
}
// the screen the host is on, as guests see it
static int hostScreen() { return G.state == ST_LOBBY || G.state == ST_MISSIONS || G.state == ST_GAME ? G.state : ST_TITLE; }

static struct { std::vector<std::string> st; std::vector<std::string> ast; std::vector<std::pair<int, int>> links[4]; int game = -1; } NS;

// common head: screen, lobby, players; world globals and teams when in a game
static void writeHead(WBuf& w) {
    w.u8(hostScreen()); w.u8(G.mode); w.u8(G.mission); w.u8(G.menuSel); w.u32(G.opt.done);
    w.u8(G.paused ? 1 : 0); w.u8(G.result); w.u8((uint8_t)(int8_t)G.winner); w.f32(G.resultT); w.u8(G.resultSel); w.str(G.resultMsg);
    w.u16(G.gameId & 0xffff);
    for (auto& p : G.pl) {
        w.u8((p.joined ? 1 : 0) | (p.cpu ? 2 : 0) | (p.remote ? 4 : 0) | (p.devLost ? 8 : 0));
        if (!p.joined) continue;
        w.u8(p.team); w.u8(p.sel); w.v2(p.cur); w.str(playerLabel(p)); w.str(p.toastT > 0 ? p.toast : ""); w.u8((int)clampf(p.toastT * 50, 0, 255));
    }
}
static void writeWorldHead(WBuf& w) {
    w.f32(G.WW); w.f32(G.WH); w.u8(G.nTeams); w.f32(G.time); w.u16(G.waveN); w.f32(G.waveT); w.f32(G.alertT);
    w.u32((uint32_t)G.motherUid); w.u8(G.motherSpawned ? 1 : 0); w.u8(G.crystalOut ? 1 : 0);
    for (int t = 0; t < G.nTeams; t++) {
        Team& T = G.teams[t];
        w.u8((T.used ? 1 : 0) | (T.alive ? 2 : 0) | (T.crystal ? 4 : 0)); w.i16(T.hq); w.u8(colIndex(T.col));
        w.f32(T.minerals); w.f32(T.energy); w.f32(T.ecap); w.f32(T.eprod); w.f32(T.drainRate); w.f32(T.mined);
        w.u16(T.kills); w.u16(T.built); w.u16(T.ftrLost); w.u8(T.found); w.i16(T.order);
    }
}
static void writeWorldTail(WBuf& w) { // everything that moves: sent whole every snapshot
    w.u16((int)std::min<size_t>(G.en.size(), 4000));
    int ne = 0; for (auto& e : G.en) { if (ne++ >= 4000) break; w.u8(e.t | (e.uid == G.motherUid ? 0x80 : 0)); w.pos(e.p); w.pos(e.v); w.ang(e.ang); w.frac(e.hp / e.maxhp); w.u8((int)clampf(e.flash * 1000, 0, 255)); }
    w.u16((int)G.fi.size());
    for (auto& f : G.fi) { w.u8(f.team); w.u16(f.home); w.pos(f.p); w.pos(f.v); w.ang(f.ang); w.frac(f.hp / f.maxhp); w.u8((int)clampf(f.flash * 1000, 0, 255)); }
    w.u16((int)std::min<size_t>(G.bl.size(), 4000));
    int nb = 0; for (auto& b : G.bl) { if (nb++ >= 4000) break; w.u8((b.missile ? 1 : 0) | ((b.team + 1) << 1)); w.pos(b.p); w.pos(b.v); }
    w.u8((int)std::min<size_t>(G.warns.size(), 255));
    for (size_t i = 0; i < G.warns.size() && i < 255; i++) { auto& x = G.warns[i]; w.pos(x.p); w.u8((int)clampf(x.t * 10, 0, 255)); w.u8(x.mother ? 1 : 0); }
    w.u8((int)std::min<size_t>(G.pings.size(), 255));
    for (size_t i = 0; i < G.pings.size() && i < 255; i++) { auto& x = G.pings[i]; w.pos(x.p); w.u8((int)clampf(x.t * 50, 0, 255)); w.u8(x.player); }
}

// ================================================================ host (native only)
#ifndef __EMSCRIPTEN__
static std::string sha1(const std::string& s) {
    uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    std::string m = s; uint64_t bits = (uint64_t)s.size() * 8; m += (char)0x80; while (m.size() % 64 != 56) m += (char)0;
    for (int i = 7; i >= 0; i--) m += (char)((bits >> (i * 8)) & 0xff);
    auto rol = [](uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
    for (size_t c = 0; c < m.size(); c += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) w[i] = ((uint8_t)m[c + i * 4] << 24) | ((uint8_t)m[c + i * 4 + 1] << 16) | ((uint8_t)m[c + i * 4 + 2] << 8) | (uint8_t)m[c + i * 4 + 3];
        for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], cc = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) { f = (b & cc) | (~b & d); k = 0x5A827999; } else if (i < 40) { f = b ^ cc ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & cc) | (b & d) | (cc & d); k = 0x8F1BBCDC; } else { f = b ^ cc ^ d; k = 0xCA62C1D6; }
            uint32_t t = rol(a, 5) + f + e + k + w[i]; e = d; d = cc; cc = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += cc; h[3] += d; h[4] += e;
    }
    std::string out; for (int i = 0; i < 5; i++) for (int j = 3; j >= 0; j--) out += (char)((h[i] >> (j * 8)) & 0xff);
    return out;
}
static std::string b64(const std::string& s) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; std::string o; int val = 0, bits = -6;
    for (unsigned char c : s) { val = (val << 8) + c; bits += 8; while (bits >= 0) { o += T[(val >> bits) & 0x3F]; bits -= 6; } }
    if (bits > -6) o += T[((val << 8) >> (bits + 8)) & 0x3F];
    while (o.size() % 4) o += '='; return o;
}
static std::string wsFrame(const std::string& msg, bool masked) {
    std::string f; f += (char)0x82; size_t n = msg.size(); uint8_t mb = masked ? 0x80 : 0;
    if (n < 126) f += (char)(mb | n);
    else if (n < 65536) { f += (char)(mb | 126); f += (char)(n >> 8); f += (char)(n & 0xff); }
    else { f += (char)(mb | 127); for (int i = 7; i >= 0; i--) f += (char)((uint64_t)n >> (i * 8) & 0xff); }
    if (!masked) return f + msg;
    uint8_t key[4]; uint32_t r = (uint32_t)SDL_GetPerformanceCounter() * 2654435761u; memcpy(key, &r, 4);
    f.append((const char*)key, 4); std::string body = msg; for (size_t i = 0; i < body.size(); i++) body[i] ^= key[i & 3];
    return f + body;
}
// parse one frame from buf; returns 1 = got a message (op/data set), 0 = need more, -1 = bad
static int wsParse(std::string& buf, int& op, std::string& data, size_t maxLen) {
    if (buf.size() < 2) return 0;
    uint8_t b0 = buf[0], b1 = buf[1]; op = b0 & 15; bool masked = b1 & 0x80; uint64_t n = b1 & 0x7f; size_t p = 2;
    if (n == 126) { if (buf.size() < 4) return 0; n = ((uint8_t)buf[2] << 8) | (uint8_t)buf[3]; p = 4; }
    else if (n == 127) { if (buf.size() < 10) return 0; n = 0; for (int i = 0; i < 8; i++) n = (n << 8) | (uint8_t)buf[2 + i]; p = 10; }
    if (n > maxLen) return -1;
    size_t need = p + (masked ? 4 : 0) + n; if (buf.size() < need) return 0;
    data = buf.substr(p + (masked ? 4 : 0), n);
    if (masked) for (size_t i = 0; i < n; i++) data[i] ^= buf[p + (i & 3)];
    buf.erase(0, need);
    return 1;
}

struct Conn { int fd = -1; std::string in, out; bool ws = false, closeAfter = false, dead = false, joined = false, needFull = true; int slot = -1; std::string name, token; float idle = 0; };
static struct {
    int lfd = -1, udp = -1, port = NET_PORT; std::vector<Conn> conns; float snapT = 0, beaconT = 0; std::string lanIp, hostName;
    // Cloudflare quick tunnel
    double sentBytes = 0; float statT = 30;
    pid_t tpid = -1; int terr = -1; std::string tbuf, turl, tcode; int tstate = 0; std::string tmsg; // tstate 0 off, 1 starting, 2 ready, 3 failed
} NH;

static std::string lanIp() {
    std::string ip; ifaddrs* ifs = nullptr;
    if (getifaddrs(&ifs) == 0) {
        for (ifaddrs* i = ifs; i; i = i->ifa_next) {
            if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || (i->ifa_flags & IFF_LOOPBACK) || !(i->ifa_flags & IFF_UP)) continue;
            char b[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &((sockaddr_in*)i->ifa_addr)->sin_addr, b, sizeof b);
            std::string s = b; if (s.rfind("169.254.", 0) == 0) continue;
            bool priv = s.rfind("192.168.", 0) == 0 || s.rfind("10.", 0) == 0 || s.rfind("172.", 0) == 0;
            if (ip.empty() || priv) { ip = s; if (priv) break; }
        }
        freeifaddrs(ifs);
    }
    return ip.empty() ? "127.0.0.1" : ip;
}
static std::string lanJoinUrl() { return "http://" + NH.lanIp + ":" + std::to_string(NH.port) + "/#join"; }
static bool netHosting() { return NH.lfd >= 0; }

static void tunnelStart();
static void netStart() {
    signal(SIGPIPE, SIG_IGN);
    if (getenv("BC_PORT")) NH.port = atoi(getenv("BC_PORT"));
    if (getenv("BC_NOSERVER")) return;
    NH.lfd = socket(AF_INET, SOCK_STREAM, 0); if (NH.lfd < 0) return;
    int one = 1; setsockopt(NH.lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons((uint16_t)NH.port);
    if (bind(NH.lfd, (sockaddr*)&a, sizeof a) < 0 || listen(NH.lfd, 16) < 0) { printf("net: port %d unavailable (%s), online play off\n", NH.port, strerror(errno)); close(NH.lfd); NH.lfd = -1; return; }
    fcntl(NH.lfd, F_SETFL, fcntl(NH.lfd, F_GETFL) | O_NONBLOCK);
    NH.udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (NH.udp >= 0) { setsockopt(NH.udp, SOL_SOCKET, SO_BROADCAST, &one, sizeof one); fcntl(NH.udp, F_SETFL, fcntl(NH.udp, F_GETFL) | O_NONBLOCK); }
    char hn[64] = "BELT COMMAND"; gethostname(hn, sizeof hn - 1); NH.hostName = cleanName(hn, 16); if (NH.hostName.empty()) NH.hostName = "BELT COMMAND";
    NH.lanIp = lanIp();
    printf("net: hosting on %s (web files from %s/web)\n", lanJoinUrl().c_str(), GAMEDIR.c_str());
    if (getenv("BC_TUNNEL")) tunnelStart(); // test switch: open the internet link right away
}

// ---- Cloudflare quick tunnel (internet play) ----
static void tunnelStop() {
    if (NH.tpid > 0) { kill(NH.tpid, SIGTERM); int st; waitpid(NH.tpid, &st, 0); printf("net: tunnel stopped\n"); }
    if (NH.terr >= 0) close(NH.terr);
    NH.tpid = -1; NH.terr = -1; NH.tstate = 0; NH.turl.clear(); NH.tcode.clear(); NH.tbuf.clear();
}
static void tunnelStart() {
    if (NH.tstate == 1 || NH.tstate == 2) return;
    if (!netHosting()) { NH.tstate = 3; NH.tmsg = "ONLINE PLAY IS OFF (PORT IN USE)"; return; }
    std::string bin = GAMEDIR + "/cloudflared";
    if (access(bin.c_str(), F_OK) != 0) { NH.tstate = 3; NH.tmsg = "CLOUDFLARED IS NOT IN THE GAME FOLDER"; printf("net: %s missing\n", bin.c_str()); return; }
    chmod(bin.c_str(), 0755);
    int fds[2]; if (pipe(fds) != 0) { NH.tstate = 3; NH.tmsg = "TUNNEL FAILED"; return; }
    pid_t pid = fork();
    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM); // dies with the game
        dup2(fds[1], 2); dup2(fds[1], 1); close(fds[0]); close(fds[1]);
        std::string url = "http://localhost:" + std::to_string(NH.port);
        execl(bin.c_str(), "cloudflared", "tunnel", "--url", url.c_str(), "--no-autoupdate", (char*)nullptr);
        _exit(127);
    }
    close(fds[1]);
    if (pid < 0) { close(fds[0]); NH.tstate = 3; NH.tmsg = "TUNNEL FAILED"; return; }
    NH.tpid = pid; NH.terr = fds[0]; fcntl(NH.terr, F_SETFL, fcntl(NH.terr, F_GETFL) | O_NONBLOCK);
    NH.tstate = 1; NH.tmsg = "OPENING INTERNET LINK..."; NH.tbuf.clear();
    printf("net: starting tunnel (%s)\n", bin.c_str());
}
static void tunnelPoll() {
    if (NH.tpid <= 0) return;
    char buf[4096];
    for (;;) { ssize_t n = read(NH.terr, buf, sizeof buf); if (n <= 0) break; NH.tbuf.append(buf, n); }
    if (NH.tstate == 1) {
        size_t a = NH.tbuf.find("https://");
        while (a != std::string::npos) {
            size_t e = a + 8; while (e < NH.tbuf.size() && (isalnum((unsigned char)NH.tbuf[e]) || NH.tbuf[e] == '-' || NH.tbuf[e] == '.')) e++;
            std::string host = NH.tbuf.substr(a + 8, e - a - 8);
            const std::string suf = ".trycloudflare.com";
            if (host.size() > suf.size() && host.compare(host.size() - suf.size(), suf.size(), suf) == 0 && host != "api.trycloudflare.com") {
                NH.turl = "https://" + host; NH.tcode = host.substr(0, host.size() - suf.size()); NH.tstate = 2; NH.tmsg.clear();
                printf("net: internet link %s (code %s)\n", NH.turl.c_str(), NH.tcode.c_str()); break;
            }
            a = NH.tbuf.find("https://", e);
        }
        if (NH.tbuf.size() > 65536) NH.tbuf.erase(0, NH.tbuf.size() - 8192);
    }
    int st; if (waitpid(NH.tpid, &st, WNOHANG) == NH.tpid) {
        printf("net: tunnel exited (%d). Its output:\n%s\n", WIFEXITED(st) ? WEXITSTATUS(st) : -1, NH.tbuf.substr(NH.tbuf.size() > 3000 ? NH.tbuf.size() - 3000 : 0).c_str());
        NH.tpid = -1; close(NH.terr); NH.terr = -1; NH.tstate = 3; NH.tmsg = "INTERNET LINK FAILED - SEE THE LOG";
    }
}

// ---- seats for online players ----
static Dev* remoteDev(int slot, const std::string& name) {
    int id = REMOTE_DEV + slot; Dev* d = devById(id);
    if (!d) { Dev nd; nd.id = id; nd.remote = true; G.devs.push_back(nd); d = &G.devs.back(); }
    d->name = name; memset(d->rheld, 0, sizeof d->rheld); d->rmv = V2(); d->rsel = -1; return d;
}
static void dropRemoteDev(int slot) { for (size_t i = 0; i < G.devs.size(); i++) if (G.devs[i].id == REMOTE_DEV + slot) { G.devs.erase(G.devs.begin() + i); return; } }
static int seatRemote(Conn& c) {
    for (auto& p : G.pl) if (p.joined && p.remote && p.token == c.token) { // the same guest coming back: their seat (a CPU kept it warm)
        for (auto& o : NH.conns) if (&o != &c && o.slot == p.idx) o.slot = -1;
        p.cpu = false; p.dev = REMOTE_DEV + p.idx; p.name = c.name; remoteDev(p.idx, c.name); return p.idx;
    }
    if (G.state != ST_LOBBY || joinedCount() >= 4) return -1;
    for (int i = 0; i < 4; i++) if (!G.pl[i].joined) {
        Player& p = G.pl[i]; p = Player(); p.joined = true; p.view = false; p.remote = true; p.idx = i; p.col = PCOL[i];
        p.dev = REMOTE_DEV + i; p.name = c.name; p.token = c.token; remoteDev(i, c.name);
        AU.play(SND_UI_OK, 0.7f); printf("net: %s joined as P%d\n", c.name.c_str(), i + 1);
        return i;
    }
    return -1;
}
static void sendTo(Conn& c, const std::string& msg) { if (c.out.size() > (4u << 20)) { c.dead = true; return; } c.out += wsFrame(msg, false); }
static void sendWelcome(Conn& c, const std::string& note) { WBuf w; w.u8(MSG_WELCOME); w.u8(NET_PROTO); w.u8((uint8_t)(int8_t)c.slot); w.str(note); w.str(NH.hostName); sendTo(c, w.s); }

static void handleGuestMsg(Conn& c, const std::string& m) {
    if (m.empty()) return;
    RBuf r(m, 1);
    if (m[0] == MSG_JOIN) {
        int proto = r.u8(); std::string nm = cleanName(r.str()), tok = r.str().substr(0, 32);
        if (proto != NET_PROTO) { c.slot = -1; WBuf w; w.u8(MSG_WELCOME); w.u8(NET_PROTO); w.u8(0xff); w.str("VERSION MISMATCH - UPDATE THE GAME"); w.str(NH.hostName); sendTo(c, w.s); c.closeAfter = true; return; }
        if (nm.empty()) nm = "GUEST"; if (tok.empty()) tok = randomToken();
        c.name = nm; c.token = tok; c.joined = true; c.needFull = true;
        c.slot = seatRemote(c);
        sendWelcome(c, c.slot >= 0 ? "" : G.state == ST_LOBBY ? "THE GAME IS FULL - WATCHING" : "GAME IN PROGRESS - YOU JOIN NEXT ROUND");
        printf("net: guest \"%s\" connected, %s\n", nm.c_str(), c.slot >= 0 ? ("seat P" + std::to_string(c.slot + 1)).c_str() : "watching");
        return;
    }
    if (m[0] == MSG_INPUT && c.slot >= 0) {
        Dev* d = devById(REMOTE_DEV + c.slot); if (!d) return;
        uint32_t edges = r.u32(), held = r.u32(); float mx = (int8_t)r.u8() / 127.f, my = (int8_t)r.u8() / 127.f; float tl = r.frac(), tr = r.frac(); V2 cur = r.v2(); int sel = (int8_t)r.u8();
        if (r.bad) return;
        for (int k = 0; k < NB; k++) { if (edges >> k & 1) d->hit[k] = true; d->rheld[k] = held >> k & 1; }
        d->rmv = V2(mx, my); d->rtl = tl; d->rtr = tr; d->rcur = cur; d->rsel = sel;
    }
}
static void handleHttp(Conn& c) {
    size_t e = c.in.find("\r\n\r\n"); if (e == std::string::npos) { if (c.in.size() > 16384) c.dead = true; return; }
    std::string head = c.in.substr(0, e); c.in.erase(0, e + 4);
    std::istringstream is(head); std::string method, path; is >> method >> path;
    std::string low = head; std::transform(low.begin(), low.end(), low.begin(), ::tolower);
    size_t k = low.find("sec-websocket-key:");
    if (k != std::string::npos && low.find("websocket") != std::string::npos) {
        size_t a = k + 18, b = head.find("\r\n", a); std::string key = head.substr(a, b == std::string::npos ? std::string::npos : b - a);
        key.erase(0, key.find_first_not_of(" \t")); key.erase(key.find_last_not_of(" \t") + 1);
        c.out += "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + b64(sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")) + "\r\n\r\n";
        c.ws = true; return;
    }
    size_t q = path.find('?'); if (q != std::string::npos) path = path.substr(0, q);
    if (path == "/") path = "/index.html";
    std::string body, type = "text/plain", code = "200 OK", cache = "no-cache";
    static const char* OK[] = { "/index.html", "/index.js", "/index.wasm", "/icon.png" };
    bool allowed = false; for (auto o : OK) if (path == o) allowed = true;
    if (allowed) {
        std::ifstream f(GAMEDIR + "/web" + path, std::ios::binary); std::stringstream ss; ss << f.rdbuf(); body = ss.str();
        std::string ext = path.substr(path.find_last_of('.') + 1);
        type = ext == "html" ? "text/html; charset=utf-8" : ext == "js" ? "text/javascript" : ext == "wasm" ? "application/wasm" : "image/png";
        if (body.empty()) { code = "404 Not Found"; type = "text/html; charset=utf-8"; body = "<h1>Belt Command</h1><p>The browser version is not installed on this box (the web folder next to the game is missing).</p>"; }
    } else if (path == "/favicon.ico") code = "204 No Content";
    else { code = "404 Not Found"; body = "not found"; }
    c.out += "HTTP/1.1 " + code + "\r\nContent-Type: " + type + "\r\nCache-Control: " + cache + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    c.closeAfter = true;
}
static bool GUEST_ON(); // guest mode stops this box from hosting
// before the game step: new connections, guest input
static void netHostRecv(float dt) {
    if (!netHosting()) return;
    tunnelPoll();
    for (;;) {
        int fd = accept(NH.lfd, nullptr, nullptr); if (fd < 0) break;
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK); int one = 1; setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
        if (NH.conns.size() >= 32) { close(fd); continue; }
        Conn c; c.fd = fd; NH.conns.push_back(c);
    }
    char buf[16384];
    for (auto& c : NH.conns) {
        c.idle += dt;
        for (;;) { ssize_t n = recv(c.fd, buf, sizeof buf, 0); if (n > 0) { c.in.append(buf, n); c.idle = 0; } else { if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) c.dead = true; break; } }
        if (c.dead) continue;
        if (!c.ws) handleHttp(c);
        if (c.ws) for (;;) {
            int op; std::string data; int r = wsParse(c.in, op, data, 1 << 16);
            if (r < 0) { c.dead = true; break; } if (r == 0) break;
            if (op == 2 || op == 1) { if (GUEST_ON()) { c.closeAfter = true; break; } handleGuestMsg(c, data); }
            else if (op == 8) { c.out += std::string("\x88\x00", 2); c.closeAfter = true; break; }
            else if (op == 9) { std::string f; f += (char)0x8A; f += (char)data.size(); c.out += f + data; }
        }
        if (c.ws && c.joined && c.idle > 10) c.dead = true; // guests send input many times a second; 10 s of silence is a lost link
    }
    // waiting guests take free seats when the host is in the lobby; dropped online players leave the lobby
    if (G.state == ST_LOBBY) {
        for (auto& c : NH.conns) if (c.joined && !c.dead && c.slot < 0 && joinedCount() < 4) { c.slot = seatRemote(c); if (c.slot >= 0) { c.needFull = true; sendWelcome(c, ""); } }
        for (auto& p : G.pl) if (p.joined && p.remote) { bool live = false; for (auto& c : NH.conns) if (!c.dead && c.slot == p.idx) live = true; if (!live) { printf("net: %s left the lobby\n", p.name.c_str()); p.joined = false; dropRemoteDev(p.idx); } }
    }
}
// after the game step: snapshots and effect events to every guest, LAN beacon, flush
static void netHostSend(float dt) {
    if (!netHosting()) return;
    // a guest that dropped mid-game: a CPU keeps their seat warm until they come back
    for (auto& c : NH.conns) if (c.dead && c.slot >= 0 && c.slot < 4) {
        Player& p = G.pl[c.slot];
        if (p.joined && p.remote) { if (G.state == ST_GAME) { p.cpu = true; p.hasGoal = false; printf("net: %s dropped, CPU takes P%d\n", p.name.c_str(), p.idx + 1); } else { p.joined = false; dropRemoteDev(p.idx); } }
        c.slot = -1;
    }
    int guests = 0; for (auto& c : NH.conns) if (c.ws && c.joined && !c.dead) guests++;
    NETREC = guests > 0 && !GUEST_ON();
    bool inGame = G.state == ST_GAME;
    NH.snapT -= dt;
    if (NETREC && NH.snapT <= 0) {
        NH.snapT = inGame ? 1.0f / 15 : 0.2f;
        if (G.gameId != NS.game) { NS.game = G.gameId; NS.st.clear(); NS.ast.clear(); for (auto& l : NS.links) l.clear(); for (auto& c : NH.conns) c.needFull = true; }
        bool anyFull = false, anyDelta = false; for (auto& c : NH.conns) if (c.joined && !c.dead) { if (c.needFull) anyFull = true; else anyDelta = true; }
        WBuf head; writeHead(head);
        std::string full, delta;
        if (!inGame) { WBuf w; w.u8(MSG_SNAP); w.u8(1); w.s += head.s; w.u16(0); full = delta = w.s; }
        else {
            WBuf wh; writeWorldHead(wh); WBuf tail; writeWorldTail(tail);
            // structures: records that changed since the last snapshot
            std::vector<std::string> cur(G.st.size()); for (size_t i = 0; i < G.st.size(); i++) { WBuf r; recStruct(r, G.st[i]); cur[i] = r.s; }
            std::vector<std::string> acur(G.ast.size()); for (size_t i = 0; i < G.ast.size(); i++) { WBuf r; recAstDyn(r, G.ast[i]); acur[i] = r.s; }
            auto build = [&](bool isFull) {
                WBuf w; w.u8(MSG_SNAP); w.u8((isFull ? 1 : 0) | 2); w.s += head.s; w.s += wh.s;
                for (int t = 0; t < G.nTeams; t++) { // power links (only when they changed) and how busy each one is
                    Team& T = G.teams[t]; bool ch = isFull || T.links != NS.links[t];
                    w.u8(ch ? 1 : 0); if (ch) { w.u16((int)T.links.size()); for (auto& l : T.links) { w.u16(l.first); w.u16(l.second); } }
                    w.u16((int)T.links.size()); for (size_t i = 0; i < T.links.size(); i++) { float fl = i < T.linkFlow.size() ? T.linkFlow[i] : 0; w.frac(fl / (fl + 14)); }
                }
                w.u16((int)G.ast.size());
                if (isFull) { w.u8(1); for (size_t i = 0; i < G.ast.size(); i++) { recAstFull(w, G.ast[i]); w.s += acur[i]; } }
                else { w.u8(0); int n = 0; for (size_t i = 0; i < acur.size(); i++) if (i >= NS.ast.size() || acur[i] != NS.ast[i]) n++;
                    w.u16(n); for (size_t i = 0; i < acur.size(); i++) if (i >= NS.ast.size() || acur[i] != NS.ast[i]) { w.u16((int)i); w.s += acur[i]; } }
                w.u16((int)G.st.size());
                int n = 0; for (size_t i = 0; i < cur.size(); i++) if (isFull || i >= NS.st.size() || cur[i] != NS.st[i]) n++;
                w.u16(n); for (size_t i = 0; i < cur.size(); i++) if (isFull || i >= NS.st.size() || cur[i] != NS.st[i]) { w.u16((int)i); w.s += cur[i]; }
                w.s += tail.s;
                w.u32((uint32_t)NETEV.s.size()); w.s += NETEV.s;
                return w.s;
            };
            if (anyFull) full = build(true);
            if (anyDelta) delta = build(false);
            NS.st = cur; NS.ast = acur; for (int t = 0; t < G.nTeams; t++) NS.links[t] = G.teams[t].links;
            NETEV.s.clear();
        }
        for (auto& c : NH.conns) if (c.joined && !c.dead) {
            if (c.out.size() > (512u << 10)) { c.needFull = true; continue; } // slow link: skip this one, start fresh with a full snapshot
            sendTo(c, c.needFull ? full : delta); c.needFull = false;
            NH.sentBytes += (c.needFull ? full : delta).size();
        }
    }
    if (!NETREC) NETEV.s.clear();
    if ((NH.statT -= dt) <= 0) { NH.statT = 30; if (guests) printf("net: %d guest%s, %.1f KB/s sent\n", guests, guests == 1 ? "" : "s", NH.sentBytes / 30 / 1024); NH.sentBytes = 0; }
    // LAN beacon so other boxes can list this game
    NH.beaconT -= dt;
    if (NH.udp >= 0 && NH.beaconT <= 0 && !GUEST_ON()) {
        NH.beaconT = 1;
        char msg[160]; snprintf(msg, sizeof msg, "BELTCMD %d %d %d %d %s", NET_PROTO, NH.port, joinedCount(), hostScreen(), NH.hostName.c_str());
        sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(NET_DISC_PORT); a.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        sendto(NH.udp, msg, strlen(msg), 0, (sockaddr*)&a, sizeof a);
    }
    for (auto& c : NH.conns) {
        while (!c.out.empty() && !c.dead) { ssize_t n = send(c.fd, c.out.data(), c.out.size(), MSG_NOSIGNAL); if (n > 0) c.out.erase(0, n); else { if (errno != EAGAIN && errno != EWOULDBLOCK) c.dead = true; break; } }
        if (c.closeAfter && c.out.empty()) c.dead = true;
    }
    for (auto& c : NH.conns) if (c.dead) { close(c.fd); if (c.joined) printf("net: guest \"%s\" disconnected\n", c.name.c_str()); }
    NH.conns.erase(std::remove_if(NH.conns.begin(), NH.conns.end(), [](const Conn& c) { return c.dead; }), NH.conns.end());
}
static int onlineGuests() { int n = 0; for (auto& c : NH.conns) if (c.joined && !c.dead) n++; return n; }
static void netShutdown() { tunnelStop(); for (auto& c : NH.conns) close(c.fd); NH.conns.clear(); if (NH.lfd >= 0) close(NH.lfd); NH.lfd = -1; }
#else
static bool netHosting() { return false; }
static void netStart() {}
static void netHostRecv(float) {}
static void netHostSend(float) { NETREC = false; NETEV.s.clear(); }
static void netShutdown() {}
static int onlineGuests() { return 0; }
static void tunnelStart() {}
static void tunnelStop() {}
#endif

// ================================================================ guest link (native: thread with plain or TLS socket; web: browser WebSocket)
static struct {
    std::string url, err; int state = 0; // 0 idle, 1 connecting, 2 open, 3 closed
    std::vector<std::string> inbox, outbox;
#ifndef __EMSCRIPTEN__
    std::thread th; std::mutex mx; std::atomic<bool> stop{ false }; std::atomic<int> astate{ 0 };
#else
    EMSCRIPTEN_WEBSOCKET_T ws = 0;
#endif
} GL;

#ifndef __EMSCRIPTEN__
// TLS for wss:// (internet links), loaded from the system's OpenSSL at run time so the game has no new dependency
static struct {
    void* h = nullptr; bool tried = false;
    int (*init)(uint64_t, const void*); const void* (*method)(); void* (*ctxNew)(const void*); void* (*sslNew)(void*); int (*setFd)(void*, int);
    long (*ctrl)(void*, int, long, void*); int (*connect)(void*); int (*read)(void*, void*, int); int (*write)(void*, const void*, int); int (*pending)(const void*); void (*sslFree)(void*); void (*ctxFree)(void*);
} SSLX;
static bool sslLoad() {
    if (SSLX.tried) return SSLX.h != nullptr; SSLX.tried = true;
    const char* libs[] = { "libssl.so.3", "libssl.so.1.1", "libssl.so" };
    for (auto l : libs) if ((SSLX.h = dlopen(l, RTLD_NOW))) { printf("net: using %s for secure links\n", l); break; }
    if (!SSLX.h) { printf("net: no libssl found, secure (internet) links unavailable\n"); return false; }
    auto S = [](const char* n) { return dlsym(SSLX.h, n); };
    *(void**)&SSLX.init = S("OPENSSL_init_ssl"); *(void**)&SSLX.method = S("TLS_client_method"); *(void**)&SSLX.ctxNew = S("SSL_CTX_new"); *(void**)&SSLX.sslNew = S("SSL_new");
    *(void**)&SSLX.setFd = S("SSL_set_fd"); *(void**)&SSLX.ctrl = S("SSL_ctrl"); *(void**)&SSLX.connect = S("SSL_connect"); *(void**)&SSLX.read = S("SSL_read"); *(void**)&SSLX.write = S("SSL_write");
    *(void**)&SSLX.pending = S("SSL_pending"); *(void**)&SSLX.sslFree = S("SSL_free"); *(void**)&SSLX.ctxFree = S("SSL_CTX_free");
    if (!SSLX.method || !SSLX.ctxNew || !SSLX.sslNew || !SSLX.setFd || !SSLX.connect || !SSLX.read || !SSLX.write) { printf("net: libssl is missing functions\n"); dlclose(SSLX.h); SSLX.h = nullptr; return false; }
    if (SSLX.init) SSLX.init(0, nullptr);
    return true;
}
static void guestThread(std::string url) {
    auto fail = [&](const std::string& e) { { std::lock_guard<std::mutex> lk(GL.mx); GL.err = e; } GL.astate = 3; printf("net: link failed: %s\n", e.c_str()); };
    bool tls = url.rfind("wss://", 0) == 0; std::string rest = url.substr(tls ? 6 : 5);
    size_t sl = rest.find('/'); std::string hostport = rest.substr(0, sl), path = sl == std::string::npos ? "/" : rest.substr(sl);
    std::string host = hostport; int port = tls ? 443 : 80; size_t co = hostport.rfind(':'); if (co != std::string::npos) { host = hostport.substr(0, co); port = atoi(hostport.c_str() + co + 1); }
    addrinfo hints{}, *res = nullptr; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) { fail("CAN'T FIND " + host); return; }
    int fd = socket(res->ai_family, SOCK_STREAM, 0);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    int rc = connect(fd, res->ai_addr, res->ai_addrlen); freeaddrinfo(res);
    if (rc != 0 && errno != EINPROGRESS) { close(fd); fail("CAN'T CONNECT"); return; }
    pollfd pf{ fd, POLLOUT, 0 }; int waited = 0;
    while (!GL.stop && waited < 8000) { int r = poll(&pf, 1, 100); waited += 100; if (r > 0) break; }
    int soerr = 0; socklen_t sl2 = sizeof soerr; getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl2);
    if (GL.stop || waited >= 8000 || soerr) { close(fd); fail(soerr ? "NO GAME AT THAT ADDRESS" : "CONNECTION TIMED OUT"); return; }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
    timeval tv{ 8, 0 }; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    int one = 1; setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    void* ctx = nullptr; void* ssl = nullptr;
    if (tls) {
        if (!sslLoad()) { close(fd); fail("THIS SYSTEM HAS NO TLS LIBRARY"); return; }
        ctx = SSLX.ctxNew(SSLX.method()); ssl = SSLX.sslNew(ctx); SSLX.setFd(ssl, fd);
        if (SSLX.ctrl) SSLX.ctrl(ssl, 55 /*SSL_CTRL_SET_TLSEXT_HOSTNAME*/, 0, (void*)host.c_str());
        if (SSLX.connect(ssl) != 1) { SSLX.sslFree(ssl); SSLX.ctxFree(ctx); close(fd); fail("SECURE LINK FAILED"); return; }
    }
    auto wr = [&](const std::string& s) { size_t o = 0; while (o < s.size()) { int n = tls ? SSLX.write(ssl, s.data() + o, (int)(s.size() - o)) : (int)send(fd, s.data() + o, s.size() - o, MSG_NOSIGNAL); if (n <= 0) return false; o += n; } return true; };
    auto rd = [&](char* b, int n) { return tls ? SSLX.read(ssl, b, n) : (int)recv(fd, b, n, 0); };
    std::string keyRaw; for (int i = 0; i < 16; i++) keyRaw += (char)(R.next() & 255);
    std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + b64(keyRaw) + "\r\nSec-WebSocket-Version: 13\r\nUser-Agent: BeltCommand\r\n\r\n";
    std::string in; char buf[16384];
    if (!wr(req)) { fail("CONNECTION LOST"); goto done; }
    while (in.find("\r\n\r\n") == std::string::npos) { int n = rd(buf, sizeof buf); if (n <= 0) { fail("NO ANSWER FROM THE HOST"); goto done; } in.append(buf, n); if (in.size() > 16384) break; }
    if (in.find(" 101") == std::string::npos) { fail("THAT IS NOT A BELT COMMAND GAME"); goto done; }
    in.erase(0, in.find("\r\n\r\n") + 4);
    GL.astate = 2; printf("net: connected to %s\n", url.c_str());
    {
        tv = { 0, 30000 }; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        while (!GL.stop) {
            std::vector<std::string> out; { std::lock_guard<std::mutex> lk(GL.mx); out.swap(GL.outbox); }
            for (auto& m : out) if (!wr(wsFrame(m, true))) { fail("CONNECTION LOST"); goto done; }
            bool more = true;
            while (more) {
                pollfd p2{ fd, POLLIN, 0 }; bool ready = (tls && SSLX.pending && SSLX.pending(ssl) > 0) || poll(&p2, 1, out.empty() ? 15 : 0) > 0;
                if (!ready) break;
                int n = rd(buf, sizeof buf);
                if (n == 0) { fail("THE HOST CLOSED THE GAME"); goto done; }
                if (n < 0) { if (errno == EAGAIN || errno == EWOULDBLOCK) break; fail("CONNECTION LOST"); goto done; }
                in.append(buf, n); more = (size_t)n == sizeof buf;
            }
            for (;;) {
                int op; std::string data; int r = wsParse(in, op, data, 8u << 20);
                if (r < 0) { fail("BAD DATA FROM THE HOST"); goto done; } if (r == 0) break;
                if (op == 2) { std::lock_guard<std::mutex> lk(GL.mx); GL.inbox.push_back(std::move(data)); }
                else if (op == 8) { fail("THE HOST CLOSED THE GAME"); goto done; }
                else if (op == 9) { std::string f; f += (char)0x8A; f += (char)(0x80 | data.size()); f += std::string(4, '\0'); f += data; wr(f); }
            }
        }
    }
done:
    if (ssl) { SSLX.sslFree(ssl); SSLX.ctxFree(ctx); }
    close(fd);
    if (GL.astate != 3) GL.astate = 3;
}
static void linkOpen(const std::string& url) {
    if (GL.th.joinable()) { GL.stop = true; GL.th.join(); }
    GL.stop = false; GL.astate = 1; GL.state = 1; GL.err.clear(); GL.inbox.clear(); GL.outbox.clear(); GL.url = url;
    GL.th = std::thread(guestThread, url);
}
static void linkClose() { if (GL.th.joinable()) { GL.stop = true; GL.th.join(); } GL.state = 0; GL.astate = 0; GL.inbox.clear(); GL.outbox.clear(); }
static void linkSend(const std::string& m) { std::lock_guard<std::mutex> lk(GL.mx); if (GL.outbox.size() < 64) GL.outbox.push_back(m); }
static void linkPoll(std::vector<std::string>& msgs) { std::lock_guard<std::mutex> lk(GL.mx); msgs.swap(GL.inbox); GL.state = GL.astate; }
#else
static EM_BOOL wsOnOpen(int, const EmscriptenWebSocketOpenEvent*, void*) { GL.state = 2; printf("net: connected to %s\n", GL.url.c_str()); return EM_TRUE; }
static EM_BOOL wsOnMsg(int, const EmscriptenWebSocketMessageEvent* e, void*) { if (!e->isText) GL.inbox.emplace_back((const char*)e->data, e->numBytes); return EM_TRUE; }
static EM_BOOL wsOnClose(int, const EmscriptenWebSocketCloseEvent*, void*) { if (GL.state != 3) { GL.err = GL.state == 2 ? "THE HOST CLOSED THE GAME" : "NO GAME AT THAT ADDRESS"; GL.state = 3; } return EM_TRUE; }
static EM_BOOL wsOnErr(int, const EmscriptenWebSocketErrorEvent*, void*) { if (GL.state != 3) { GL.err = GL.state == 2 ? "CONNECTION LOST" : "CAN'T CONNECT"; GL.state = 3; } return EM_TRUE; }
static void linkClose() { if (GL.ws > 0) { emscripten_websocket_close(GL.ws, 1000, "bye"); emscripten_websocket_delete(GL.ws); } GL.ws = 0; GL.state = 0; GL.inbox.clear(); }
static void linkOpen(const std::string& url) {
    linkClose(); GL.url = url; GL.err.clear(); GL.state = 1;
    EmscriptenWebSocketCreateAttributes a; emscripten_websocket_init_create_attributes(&a); a.url = GL.url.c_str(); a.protocols = nullptr; a.createOnMainThread = EM_TRUE;
    GL.ws = emscripten_websocket_new(&a);
    if (GL.ws <= 0) { GL.state = 3; GL.err = "CAN'T CONNECT"; return; }
    emscripten_websocket_set_onopen_callback(GL.ws, nullptr, wsOnOpen); emscripten_websocket_set_onmessage_callback(GL.ws, nullptr, wsOnMsg);
    emscripten_websocket_set_onclose_callback(GL.ws, nullptr, wsOnClose); emscripten_websocket_set_onerror_callback(GL.ws, nullptr, wsOnErr);
}
static void linkSend(const std::string& m) { if (GL.ws > 0 && GL.state == 2) emscripten_websocket_send_binary(GL.ws, (void*)m.data(), (uint32_t)m.size()); }
static void linkPoll(std::vector<std::string>& msgs) { msgs.swap(GL.inbox); }
#endif

// ================================================================ guest game
static struct {
    bool on = false, welcomed = false, haveWorld = false, leaveAsk = false, mouseMode = true; int slot = -1, hostState = ST_TITLE, gameId = -1, leaveSel = 0, lastResult = 0;
    std::string target, label, note, hostName; unsigned hostDone = 0; float sendT = 0, connT = 0, quietT = 0; uint32_t edges = 0; int localDev = -1;
    Player spec; // the camera when watching without a seat
} GS;
static bool GUEST_ON() { return GS.on; }

// what the player typed (a code from the host's screen, a LAN address, or a link) -> the WebSocket address
static std::string joinUrl(std::string t, std::string& err) {
    err.clear();
    for (auto& ch : t) ch = (char)tolower((unsigned char)ch);
    while (!t.empty() && t.back() == '/') t.pop_back();
    size_t j = t.find("#join="); if (j != std::string::npos) t = t.substr(j + 6);
    size_t s = t.find("://"); bool secure = false;
    if (s != std::string::npos) { secure = t.compare(0, s, "https") == 0 || t.compare(0, s, "wss") == 0; t = t.substr(s + 3); }
    size_t sl = t.find('/'); if (sl != std::string::npos) t = t.substr(0, sl);
    if (t.empty()) { err = "TYPE THE CODE FROM THE HOST'S SCREEN"; return ""; }
    int dashes = 0, dots = 0; bool letters = true; for (char c : t) { if (c == '-') dashes++; else if (c == '.') dots++; else if (!isalpha((unsigned char)c)) letters = false; }
    if (dots == 0 && dashes >= 2 && letters) return "wss://" + t + ".trycloudflare.com/ws"; // a quick-tunnel code like alpha-bravo-charlie-delta
    if (t.find("trycloudflare.com") != std::string::npos || secure) return "wss://" + t + "/ws";
    if (t.find(':') == std::string::npos) t += ":" + std::to_string(NET_PORT);
#ifdef __EMSCRIPTEN__
    bool pageSecure = EM_ASM_INT({ return location.protocol == 'https:' ? 1 : 0; });
    if (pageSecure) { err = "OPEN HTTP://" + t + " IN THE BROWSER TO JOIN ON WI-FI"; for (auto& ch : err) ch = (char)toupper((unsigned char)ch); return ""; }
#endif
    return "ws://" + t + "/ws";
}
static void guestLeave(const std::string& why = "") {
    linkClose(); GS.on = false; GS.welcomed = false; GS.haveWorld = false; GS.leaveAsk = false;
    for (auto& p : G.pl) p = Player();
    G.st.clear(); G.en.clear(); G.fi.clear(); G.bl.clear(); G.ast.clear(); G.parts.clear(); G.beams.clear(); G.warns.clear(); G.pings.clear();
    G.paused = false; G.result = 0;
    G.state = why.empty() ? ST_TITLE : ST_NET; GS.note = why; G.menuSel = 0; G.stateT = 0;
    if (!why.empty()) printf("net: left the game: %s\n", why.c_str());
}
static void guestConnect(const std::string& target) {
    std::string err, url = joinUrl(target, err);
    GS.target = target;
    if (url.empty()) { G.state = ST_NET; GS.on = false; GS.note = err; return; }
    GS.on = true; GS.welcomed = false; GS.haveWorld = false; GS.slot = -1; GS.gameId = -1; GS.connT = 0; GS.note.clear(); GS.label = target; GS.hostName.clear();
    for (auto& p : G.pl) p = Player();
    G.state = ST_NET; G.stateT = 0;
    printf("net: joining %s\n", url.c_str());
    linkOpen(url);
}

static void guestEvents(RBuf& r, size_t end) {
    int myTeam = GS.slot >= 0 ? G.pl[GS.slot].team : -2;
    while (r.i < end && !r.bad) {
        int t = r.u8();
        switch (t) {
        case EV_EXPLODE: { V2 p = r.pos(); float sz = (float)r.u16(); Col c = r.col(); explode(p, sz, c); break; }
        case EV_SPARKS: { V2 p = r.pos(); int n = r.u8(); Col c = r.col(); float sp = (float)r.u16(); sparks(p, n, c, sp); break; }
        case EV_RING: { V2 p = r.pos(); float a = (float)r.u16(), b = (float)r.u16(); Col c = r.col(); float l = r.u8() / 100.f; ring(p, a, b, c, l); break; }
        case EV_SHAKE: { V2 p = r.pos(); float a = (float)r.u8(); shakeAt(p, a); break; }
        case EV_SFX: { int s = r.u8(); V2 p = r.pos(); float v = r.frac(); if (s < SND_N) sfxAt(s, p, v); break; }
        case EV_SOUND: { int s = r.u8(); float v = r.frac(); int tm = r.u8(); if (s < SND_N && (tm == 255 || tm == myTeam)) AU.play(s, v); break; }
        case EV_PSOUND: { int s = r.u8(); float v = r.frac(); int pl = r.u8(); if (s < SND_N && pl == GS.slot) AU.play(s, v); break; }
        case EV_BEAM: { V2 a = r.pos(), b = r.pos(); float l = r.u8() / 100.f, w = r.u8() / 10.f; Col c = r.col(); G.beams.push_back({ a, b, l, l, w, c }); break; }
        default: r.i = end; break;
        }
    }
    r.i = end;
}
static void guestSnapshot(const std::string& m) {
    RBuf r(m, 1);
    int flags = r.u8(); bool full = flags & 1, inGame = flags & 2;
    int scr = r.u8(); G.mode = std::min(r.u8(), MODE_N - 1); G.mission = std::min(r.u8(), NMISSIONS - 1); int msel = r.u8(); GS.hostDone = r.u32();
    G.paused = r.u8() != 0; int result = r.u8(); G.winner = (int8_t)r.u8(); float resT = r.f32(); G.resultSel = r.u8(); G.resultMsg = r.str();
    int gid = r.u16();
    for (int i = 0; i < 4; i++) {
        Player& p = G.pl[i]; int f = r.u8(); bool was = p.joined;
        p.joined = f & 1; p.cpu = f & 2; p.remote = f & 4; p.devLost = f & 8; p.idx = i; p.col = PCOL[i];
        if (!p.joined) { p.view = false; continue; }
        p.team = r.u8(); int sel = r.u8(); V2 cur = r.v2(); p.name = r.str(); std::string toast = r.str(); float toastT = r.u8() / 50.f;
        if (i != GS.slot || !was) { p.sel = std::min(sel, NBUILD - 1); p.cur = cur; }
        if (!toast.empty() && (toast != p.toast || toastT > p.toastT + 0.3f)) { p.toast = toast; p.toastT = toastT; }
        p.view = i == GS.slot; p.dev = i == GS.slot ? GS.localDev : (p.cpu ? -9 : -7);
    }
    if (r.bad) return;
    if (scr == ST_LOBBY || scr == ST_MISSIONS) { G.menuSel = msel; }
    if (!inGame) { GS.hostState = scr; if (scr != ST_GAME) G.result = 0; return; }
    // world
    bool newGame = gid != GS.gameId;
    if (newGame && !full) return; // wait for the full picture of the new game
    if (newGame) {
        GS.gameId = gid; G.parts.clear(); G.beams.clear(); G.result = 0; GS.lastResult = 0; GS.leaveAsk = false;
        G.M = G.mode == MODE_RAID ? M_RAID : G.mode == MODE_VERSUS ? M_VERSUS : G.mode == MODE_ENDLESS ? M_ENDLESS : MISSIONS[G.mission];
    }
    G.WW = r.f32(); G.WH = r.f32(); G.nTeams = std::min(4, std::max(1, r.u8())); G.time = r.f32(); G.waveN = r.u16(); G.waveT = r.f32(); G.alertT = r.f32();
    G.motherUid = (int)r.u32(); G.motherSpawned = r.u8() != 0; G.crystalOut = r.u8() != 0;
    for (int t = 0; t < 4; t++) if (t >= G.nTeams) G.teams[t].used = false;
    for (int t = 0; t < G.nTeams; t++) {
        Team& T = G.teams[t]; int f = r.u8(); T.used = f & 1; T.alive = f & 2; T.crystal = f & 4; T.hq = r.i16(); T.col = PCOL[std::min(r.u8(), 3)];
        T.minerals = r.f32(); T.energy = r.f32(); T.ecap = r.f32(); T.eprod = r.f32(); T.drainRate = r.f32(); T.mined = r.f32();
        T.kills = r.u16(); T.built = r.u16(); T.ftrLost = r.u16(); T.found = r.u8(); T.order = r.i16();
    }
    for (int t = 0; t < G.nTeams; t++) {
        Team& T = G.teams[t];
        if (r.u8()) { int n = r.u16(); T.links.resize(n); for (auto& l : T.links) { l.first = r.u16(); l.second = r.u16(); } }
        int nf = r.u16(); T.linkFlow.resize(nf); for (auto& fl : T.linkFlow) { float f = std::min(r.frac(), 0.99f); fl = 14 * f / (1 - f); }
    }
    int na = r.u16(); bool astFull = r.u8() != 0;
    if (astFull) { G.ast.assign(na, Asteroid()); for (auto& a : G.ast) { readAstFull(r, a); readAstDyn(r, a); } }
    else { if ((int)G.ast.size() != na) G.ast.resize(na); int n = r.u16(); for (int k = 0; k < n && !r.bad; k++) { int i = r.u16(); Asteroid tmp; readAstDyn(r, i < na ? G.ast[i] : tmp); } }
    int ns = r.u16(); if ((int)G.st.size() != ns) G.st.resize(ns);
    int nrec = r.u16(); for (int k = 0; k < nrec && !r.bad; k++) { int i = r.u16(); Struct tmp; Struct& s = i < ns ? G.st[i] : tmp; float spin = s.spin; readStruct(r, s); s.spin = spin; }
    int ne = r.u16(); G.en.resize(ne);
    for (auto& e : G.en) { int t = r.u8(); e = Enemy(); e.t = std::min(t & 0x7f, E_N - 1); e.uid = (t & 0x80) ? G.motherUid : -1; e.p = r.pos(); e.v = r.pos(); e.ang = r.ang(); e.maxhp = ED[e.t].hp; e.hp = r.frac() * e.maxhp; e.flash = r.u8() / 1000.f; }
    int nf = r.u16(); G.fi.resize(nf);
    for (auto& f : G.fi) { f = Fighter(); f.team = r.u8() & 3; f.home = r.u16(); f.p = r.pos(); f.v = r.pos(); f.ang = r.ang(); f.maxhp = 100; f.hp = r.frac() * 100; f.flash = r.u8() / 1000.f; }
    int nb = r.u16(); G.bl.resize(nb);
    for (auto& b : G.bl) { int f = r.u8(); b = Bullet(); b.missile = f & 1; b.team = (f >> 1) - 1; b.p = r.pos(); b.v = r.pos(); b.life = 1; }
    int nw = r.u8(); G.warns.resize(nw); for (auto& w : G.warns) { w.p = r.pos(); w.t = r.u8() / 10.f; w.mother = r.u8() != 0; w.budget = 0; }
    int np = r.u8(); G.pings.resize(np); for (auto& p : G.pings) { p.p = r.pos(); p.t = r.u8() / 50.f; p.player = std::min(r.u8(), 3); }
    uint32_t evLen = r.u32(); size_t evEnd = r.i + evLen; if (evEnd > m.size()) return;
    GS.haveWorld = true; GS.hostState = scr;
    for (int t = 0; t < G.nTeams; t++) G.teams[t].dirty = false;
    if (newGame && GS.slot >= 0 && G.pl[GS.slot].joined) { // start the camera at my command
        Player& me = G.pl[GS.slot]; int hq = G.teams[me.team].hq; V2 c = hq >= 0 && hq < ns ? G.st[hq].p : V2(G.WW / 2, G.WH / 2);
        me.cam = c; me.cur = c + V2(40, 40); me.zoom = 0.85f; me.sel = 1; me.toastT = 0;
    }
    if (newGame) { GS.spec.cam = V2(G.WW / 2, G.WH / 2); GS.spec.zoom = 0.5f; }
    guestEvents(r, evEnd);
    // results: the sound depends on whose side you were on
    if (result && !GS.lastResult) {
        bool won = rivals() ? (GS.slot >= 0 && G.pl[GS.slot].joined && G.winner == G.pl[GS.slot].team) : result == 1;
        AU.play(won ? SND_WIN : SND_LOSE, 0.9f);
    }
    GS.lastResult = result; G.result = result; if (result) G.resultT = std::max(G.resultT, resT); else G.resultT = 0;
}
static void guestRecv() {
    if (!GS.on) return;
    std::vector<std::string> msgs; linkPoll(msgs);
    for (auto& m : msgs) {
        if (m.empty()) continue;
        if (m[0] == MSG_WELCOME) {
            RBuf r(m, 1); int proto = r.u8(); int slot = (int8_t)r.u8(); std::string note = r.str(); GS.hostName = r.str();
            if (proto != NET_PROTO || slot == -1 && note.find("VERSION") != std::string::npos) { guestLeave("VERSION MISMATCH - THE HOST AND THIS GAME NEED THE SAME UPDATE"); return; }
            GS.welcomed = true; GS.slot = slot; GS.note = note; GS.gameId = -1;
            printf("net: welcome from %s, %s\n", GS.hostName.c_str(), slot >= 0 ? ("seat P" + std::to_string(slot + 1)).c_str() : note.c_str());
        } else if (m[0] == MSG_SNAP && GS.welcomed) { GS.quietT = 0; guestSnapshot(m); }
    }
    if (GL.state == 2 && !GS.welcomed && GS.connT >= 0) { // say hello once
        WBuf w; w.u8(MSG_JOIN); w.u8(NET_PROTO); w.str(myName()); w.str(G.opt.token); linkSend(w.s); GS.connT = -1;
    }
    if (GL.state == 3) guestLeave(GL.err.empty() ? "CONNECTION LOST" : GL.err);
}

// ---- local control of my seat: the same feel as playing on the host ----
static void guestControl(float dt) {
    Player& p = G.pl[GS.slot];
    p.toastT = std::max(0.f, p.toastT - dt); p.shake *= std::pow(0.02f, dt); if (p.shake < 0.1f) p.shake = 0;
    Dev* k = kbd();
    V2 mv; float tl = 0, tr = 0, wheel = 0; bool held[NB] = {}, edge[NB] = {}; int padDev = -1;
    for (auto& d : G.devs) {
        if (d.remote) continue;
        mv += d.mv; tl = std::max(tl, d.tl); tr = std::max(tr, d.tr); wheel += d.wheel;
        for (int b = 0; b < NB; b++) { if (d.b[b]) held[b] = true; if (pr(d, b)) edge[b] = true; }
        if (d.id != -1 && (len(d.mv) > 0.1f || std::any_of(d.pend, d.pend + NB, [](bool x) { return x; }))) padDev = d.id;
    }
    if (len(mv) > 1) mv = norm(mv);
    if (k && (k->mouseMoved || k->clickPend)) GS.mouseMode = true;
    if (padDev >= 0) { GS.mouseMode = false; GS.localDev = padDev; }
    if (GS.mouseMode) GS.localDev = -1;
    p.dev = GS.localDev;
    float z = (tr - tl) * dt * 1.3f - wheel * 0.1f; p.zoom = clampf(p.zoom * (1 - z), 0.4f, 1.7f);
    float vs = viewScale(p); int barSlot = -1;
    if (GS.mouseMode && k) {
        V2 off = p.cur - p.cam;
        p.cam += k->mv * (900 / p.zoom * dt); p.cam.x = clampf(p.cam.x, -150, G.WW + 150); p.cam.y = clampf(p.cam.y, -150, G.WH + 150);
        V2 ms = k->mouse;
        if (ms.x >= p.vp.x && ms.x < p.vp.x + p.vp.w && ms.y >= p.vp.y && ms.y < p.vp.y + p.vp.h) { View v; v.vp = p.vp; v.cam = p.cam; v.s = vs; p.cur = v.s2w(ms); barSlot = barSlotAt(p.vp, ms); }
        else p.cur = p.cam + off;
    } else {
        float m = len(mv); float spd = (160 + 1150 * m * m) / p.zoom; p.cur += mv * (spd * dt);
        p.cur.x = clampf(p.cur.x, -100, G.WW + 100); p.cur.y = clampf(p.cur.y, -100, G.WH + 100);
        float hx = p.vp.w * 0.5f / vs, hy = p.vp.h * 0.5f / vs, bx = hx * 0.55f, by = hy * 0.5f;
        if (p.cur.x > p.cam.x + bx) p.cam.x = p.cur.x - bx; if (p.cur.x < p.cam.x - bx) p.cam.x = p.cur.x + bx;
        if (p.cur.y > p.cam.y + by) p.cam.y = p.cur.y - by; if (p.cur.y < p.cam.y - by) p.cam.y = p.cur.y + by;
    }
    p.hover = hoverAt(p.team, p.cur, rivals()); if (p.hover >= 0 && !seen(p.team, G.st[p.hover].team)) p.hover = -1;
    p.bigMap = held[B_BACK];
    if (edge[B_LB]) { p.sel = (p.sel + NBUILD - 1) % NBUILD; AU.play(SND_UI, 0.5f); }
    if (edge[B_RB]) { p.sel = (p.sel + 1) % NBUILD; AU.play(SND_UI, 0.5f); }
    if (k && k->digit >= 0) { if (k->digit < NBUILD && p.sel != k->digit) { p.sel = k->digit; AU.play(SND_UI, 0.5f); } }
    if (barSlot >= 0 && k && k->clickPend) { if (p.sel != barSlot) { p.sel = barSlot; AU.play(SND_UI, 0.5f); } edge[B_A] = false; } // clicking the build bar picks, never builds
    bool own = p.hover >= 0 && G.st[p.hover].team == p.team;
    if (held[B_Y] && own && G.st[p.hover].t != S_HQ) p.yHold += dt; else p.yHold = 0; // the host does the real recycle after the same hold
    for (int b : { B_A, B_B, B_X }) if (edge[b]) GS.edges |= 1u << b;
    // BC_GUESTBOT=1 (test): build something near my command every second (solar, miner, laser in turn)
    static float botT = 1; if (getenv("BC_GUESTBOT") && (botT -= dt) <= 0) {
        botT = 1; static int k = 0; const int picks[3] = { 1, 3, 4 }; p.sel = picks[k++ % 3];
        int hq = G.teams[p.team].hq; if (hq >= 0 && hq < (int)G.st.size()) { p.cur = G.st[hq].p + fromAng(R.range(0, 6.28f)) * R.range(70, 140); p.cam = p.cur; GS.edges |= 1u << B_A; }
    }
    // send
    GS.sendT -= dt;
    if (GS.edges || GS.sendT <= 0) {
        GS.sendT = 1.0f / 30;
        uint32_t h = 0; for (int b = 0; b < NB; b++) if (held[b] && (b == B_Y || b == B_BACK)) h |= 1u << b;
        WBuf w; w.u8(MSG_INPUT); w.u32(GS.edges); w.u32(h); w.u8((int)(clampf(mv.x, -1, 1) * 127) & 255); w.u8((int)(clampf(mv.y, -1, 1) * 127) & 255);
        w.frac(tl); w.frac(tr); w.v2(p.cur); w.u8(p.sel); linkSend(w.s); GS.edges = 0;
    }
}
// local effects between snapshots: things keep moving, particles and beams fade, engines leave trails
static void guestFx(float dt) {
    G.time += dt; if (G.result) G.resultT += dt;
    updateFx(dt);
    for (auto& e : G.en) { e.p += e.v * dt; e.flash = std::max(0.f, e.flash - dt);
        if (R.f() < dt * 20) { const EDef& D = ED[e.t]; V2 tail = e.p - fromAng(e.ang) * D.r; addPart(P_DOT, tail, -fromAng(e.ang) * 40 + V2(R.range(-10, 10), R.range(-10, 10)), 0.3f, Col(255, 140, 60), D.r * 0.25f); } }
    for (auto& f : G.fi) { f.p += f.v * dt; f.flash = std::max(0.f, f.flash - dt);
        if (R.f() < dt * 14) addPart(P_DOT, f.p - fromAng(f.ang) * 7, -fromAng(f.ang) * 30, 0.25f, mixc(G.teams[f.team].col, C_WHITE, 0.3f), 2.2f); }
    for (auto& b : G.bl) { b.p += b.v * dt; if (b.missile && R.f() < dt * 40) addPart(P_DOT, b.p, b.v * -0.1f, 0.35f, Col(255, 200, 120), 2.5f); }
    for (auto& s : G.st) {
        if (s.dead) continue;
        s.flash = std::max(0.f, s.flash - dt); s.beamT = std::max(0.f, s.beamT - dt); s.cd = std::max(0.f, s.cd - dt);
        s.spin += dt * (s.mining ? 7 : s.t == S_RESEARCH && s.powered && !G.teams[s.team].crystal ? 4 : 1);
        if (s.t == S_MINER && s.mining && s.ast >= 0 && s.ast < (int)G.ast.size() && R.f() < dt * 4) {
            const Asteroid& a = G.ast[s.ast]; V2 sp = a.p + fromAng(R.range(0, 6.28f)) * a.r * 0.8f; addPart(P_DOT, sp, (s.p - sp) * 1.4f, 0.6f, s.cryBeam ? C_CRY : C_MIN, 3);
        }
    }
    for (auto& w : G.warns) w.t = std::max(0.f, w.t - dt);
    G.alertT = std::max(0.f, G.alertT - dt);
}
// one guest frame (input already read): network, local control, effects
static void guestUpdate(float dt) {
    GS.quietT += dt; if (GS.connT >= 0) GS.connT += dt;
    guestRecv(); if (!GS.on) { usePresses(); return; }
    if (GL.state == 1 && GS.connT > 12) { guestLeave("CONNECTION TIMED OUT"); usePresses(); return; }
    if (GS.welcomed && GS.quietT > 12) { guestLeave("THE HOST STOPPED ANSWERING"); usePresses(); return; }
    bool anyB = false, anyA = false, anyStart = false; for (auto& d : G.devs) if (!d.remote) { anyB |= pr(d, B_B); anyA |= pr(d, B_A); anyStart |= pr(d, B_START); }
    if (!GS.welcomed) { G.state = ST_NET; if (anyB) guestLeave(); usePresses(); return; }
    int hs = GS.hostState;
    if (hs == ST_GAME && GS.haveWorld) {
        G.state = ST_GAME;
        if (anyStart) { GS.leaveAsk = !GS.leaveAsk; GS.leaveSel = 1; AU.play(SND_UI, 0.6f); }
        else if (GS.leaveAsk) {
            int ny = 0; for (auto& d : G.devs) if (!d.remote && d.navY) ny = d.navY;
            if (ny) { GS.leaveSel ^= 1; AU.play(SND_UI, 0.6f); }
            bool c; menuMouse(GS.leaveSel, c);
            if (anyB) GS.leaveAsk = false;
            else if (anyA || c) { if (GS.leaveSel == 0) { guestLeave(); usePresses(); return; } GS.leaveAsk = false; }
        }
        if (GS.slot >= 0 && G.pl[GS.slot].joined && !GS.leaveAsk && !G.paused && !G.result) {
            Player& me = G.pl[GS.slot]; me.view = true;
            if (me.vp.w != G.W || me.vp.h != G.H) layoutViews();
            if (!G.teams[me.team].alive && rivals()) { /* command lost: keep watching */ }
            guestControl(dt);
        } else if (GS.slot < 0) { // watching: move the camera freely
            V2 mv; for (auto& d : G.devs) if (!d.remote) mv += d.mv; if (len(mv) > 1) mv = norm(mv);
            GS.spec.cam += mv * (900 / std::max(0.3f, GS.spec.zoom) * dt); GS.spec.cam.x = clampf(GS.spec.cam.x, 0, G.WW); GS.spec.cam.y = clampf(GS.spec.cam.y, 0, G.WH);
        }
        guestFx(dt);
    } else {
        G.state = hs == ST_LOBBY || hs == ST_MISSIONS ? hs : ST_NET;
        if (anyB) { guestLeave(); usePresses(); return; }
    }
    usePresses();
}

// ================================================================ drawing for online play
static std::map<std::string, QRCode>* QRC = nullptr;
static void drawQR(Canvas& cv, const std::string& text, float x, float y, float size) {
    if (!QRC) QRC = new std::map<std::string, QRCode>();
    auto it = QRC->find(text); if (it == QRC->end()) it = QRC->emplace(text, qrEncode(text)).first;
    const QRCode& q = it->second; if (!q.size) return;
    float m = size / (q.size + 2);
    cv.rect(x, y, size, size, Col(255, 255, 255, 255));
    for (int yy = 0; yy < q.size; yy++) for (int xx = 0; xx < q.size; xx++) if (q.at(xx, yy)) cv.rect(x + (xx + 1) * m, y + (yy + 1) * m, m + 0.4f, m + 0.4f, Col(0, 0, 0, 255));
}
// the host's lobby panel: how friends join
static void drawHostPanel(Canvas& cv, float u) {
#ifndef __EMSCRIPTEN__
    if (!netHosting()) return;
    float y = 800 * u, qs = 170 * u, x = G.W * 0.5f - 814 * u;
    drawQR(cv, lanJoinUrl(), x, y, qs);
    FONT.draw(cv, "SAME WI-FI: SCAN, OR OPEN", V2(x + qs + 20 * u, y + 20 * u), 15 * u, withA(C_WHITE, 0.85f));
    std::string lan = "HTTP://" + NH.lanIp + ":" + std::to_string(NH.port);
    FONT.draw(cv, lan.c_str(), V2(x + qs + 20 * u, y + 52 * u), 20 * u, PCOL[0]);
    FONT.draw(cv, "IN A BROWSER", V2(x + qs + 20 * u, y + 88 * u), 15 * u, withA(C_WHITE, 0.85f));
    char g[48]; snprintf(g, sizeof g, "ONLINE PLAYERS: %d", onlineGuests()); FONT.draw(cv, g, V2(x + qs + 20 * u, y + 130 * u), 14 * u, withA(C_DIM, 0.95f));
    float rx = G.W * 0.5f + 814 * u - qs;
    if (NH.tstate == 2) {
        std::string link = std::string(WEB_SITE) + "/#join=" + NH.tcode;
        drawQR(cv, link, rx, y, qs);
        FONT.draw(cv, "INTERNET: SCAN, OR GO TO", V2(rx - 20 * u, y + 10 * u), 15 * u, withA(C_WHITE, 0.85f), 2);
        FONT.draw(cv, "BELT-COMMAND.VERCEL.APP", V2(rx - 20 * u, y + 40 * u), 18 * u, PCOL[0], 2);
        FONT.draw(cv, "JOIN GAME - CODE:", V2(rx - 20 * u, y + 76 * u), 15 * u, withA(C_WHITE, 0.85f), 2);
        std::string code = NH.tcode; for (auto& ch : code) ch = (char)toupper((unsigned char)ch);
        float cs = std::min(22 * u, 560 * u / std::max(1.f, (float)code.size()));
        FONT.draw(cv, code.c_str(), V2(rx - 20 * u, y + 106 * u), cs, C_MIN, 2);
        FONT.draw(cv, "BACK / TAB: CLOSE INTERNET LINK", V2(rx - 20 * u, y + 146 * u), 12 * u, withA(C_DIM, 0.95f), 2);
    } else {
        const char* l1 = NH.tstate == 1 ? "OPENING INTERNET LINK..." : NH.tstate == 3 ? NH.tmsg.c_str() : "PLAY WITH FRIENDS OVER THE INTERNET?";
        FONT.draw(cv, l1, V2(rx + qs, y + 40 * u), 17 * u, NH.tstate == 3 ? C_BAD : withA(C_WHITE, 0.85f), 2);
        if (NH.tstate != 1) FONT.draw(cv, "PRESS BACK / TAB TO OPEN A LINK", V2(rx + qs, y + 80 * u), 17 * u, PCOL[0], 2);
    }
#else
    (void)cv; (void)u;
#endif
}
// guest screens: connecting, waiting, errors
static void renderNetScreen(Canvas& cv) {
    float u = G.H / 1080.f, cx = G.W * 0.5f;
    drawTitleBackdrop(cv, u);
    FONT.draw(cv, "JOIN GAME", V2(cx, 120 * u), 50 * u, PCOL[0], 1, 0, 1.1f);
    std::string l1, l2; Col c1 = C_WHITE;
    if (!GS.on) { l1 = GS.note.empty() ? "NOT CONNECTED" : GS.note; c1 = C_BAD; l2 = "B BACK"; }
    else if (!GS.welcomed) { l1 = "CONNECTING..."; l2 = GS.label; }
    else { l1 = (GS.hostName.empty() ? std::string("THE HOST") : GS.hostName) + " IS IN THE MENUS"; l2 = "YOU'LL SEE THE LOBBY WHEN THEY OPEN IT   -   B LEAVE"; }
    for (auto& ch : l2) ch = (char)toupper((unsigned char)ch);
    float s1 = std::min(30 * u, G.W * 0.9f / std::max(1.f, (float)l1.size()));
    FONT.draw(cv, l1.c_str(), V2(cx, G.H * 0.42f), s1, c1, 1, 0, 0.8f);
    float s2 = std::min(18 * u, G.W * 0.9f / std::max(1.f, (float)l2.size()));
    FONT.draw(cv, l2.c_str(), V2(cx, G.H * 0.42f + 70 * u), s2, withA(C_WHITE, 0.7f), 1);
    if (GS.on && !GS.welcomed) { float a = G.stateT * 4; for (int i = 0; i < 8; i++) cv.glowDisc(V2(cx, G.H * 0.62f) + fromAng(a + i * PI / 4) * 30 * u, (3 + i * 0.6f) * u, withA(PCOL[0], 0.3f + i * 0.08f)); }
    cv.flush(G.ren);
}
// guest overlays in a game: leave menu, watching banner
static void renderGuestOverlay(Canvas& cv) {
    float u = G.H / 1080.f, cx = G.W * 0.5f;
    if (GS.slot < 0) {
        std::string t = GS.note.empty() ? "WATCHING" : GS.note;
        cv.rect(0, 0, (float)G.W, 60 * u, Col(0, 0, 0, 150));
        FONT.draw(cv, t.c_str(), V2(cx, 18 * u), 22 * u, C_MIN, 1);
        FONT.draw(cv, "STICK / WASD MOVE   START LEAVE", V2(cx, G.H - 40 * u), 14 * u, withA(C_DIM, 0.95f), 1);
    }
    if (GS.leaveAsk) {
        cv.rect(0, 0, (float)G.W, (float)G.H, Col(0, 0, 0, 160));
        FONT.draw(cv, "LEAVE THE GAME?", V2(cx, G.H * 0.32f), 48 * u, PCOL[0], 1, 0, 1.1f);
        const char* items[2] = { "LEAVE", "KEEP PLAYING" };
        menuList(cv, items, 2, GS.leaveSel, cx, G.H * 0.48f, 30 * u, 60 * u);
        FONT.draw(cv, "A CONFIRM   B BACK", V2(cx, G.H * 0.66f), 16 * u, withA(C_DIM, 0.95f), 1);
    }
    cv.flush(G.ren);
}
static void renderSpectator(Canvas& cv) {
    GS.spec.vp = { 0, 0, G.W, G.H };
    View v; v.vp = GS.spec.vp; v.cam = GS.spec.cam; v.s = G.H / 900.f * GS.spec.zoom;
    drawStars(cv, v); cv.flush(G.ren);
    drawWorld(cv, v, nullptr); cv.flush(G.ren);
    float u = G.H / 1080.f, mw = 360 * u;
    drawMinimap(cv, G.W - mw - 16 * u, 76 * u, mw, nullptr, u); cv.flush(G.ren);
}

// ================================================================ LAN game list (native join screen)
struct LanGame { std::string ip, name; int port, players, screen; float seen; };
static std::vector<LanGame> LANS;
#ifndef __EMSCRIPTEN__
static int DISC = -1;
static void lanListen(float dt) {
    if (DISC < 0) {
        DISC = socket(AF_INET, SOCK_DGRAM, 0); if (DISC < 0) return;
        int one = 1; setsockopt(DISC, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
#ifdef SO_REUSEPORT
        setsockopt(DISC, SOL_SOCKET, SO_REUSEPORT, &one, sizeof one);
#endif
        sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(NET_DISC_PORT);
        if (bind(DISC, (sockaddr*)&a, sizeof a) < 0) { printf("net: can't listen for LAN games (%s)\n", strerror(errno)); close(DISC); DISC = -2; return; }
        fcntl(DISC, F_SETFL, fcntl(DISC, F_GETFL) | O_NONBLOCK);
    }
    if (DISC < 0) return;
    for (auto& g : LANS) g.seen += dt;
    char buf[256]; sockaddr_in from{}; socklen_t fl = sizeof from;
    for (;;) {
        ssize_t n = recvfrom(DISC, buf, sizeof buf - 1, 0, (sockaddr*)&from, &fl); if (n <= 0) break; buf[n] = 0;
        int proto, port, players, scr; char name[80] = "";
        if (sscanf(buf, "BELTCMD %d %d %d %d %79[^\n]", &proto, &port, &players, &scr, name) < 4 || proto != NET_PROTO) continue;
        char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &from.sin_addr, ip, sizeof ip);
        if (netHosting() && (std::string(ip) == NH.lanIp || std::string(ip) == "127.0.0.1") && port == NH.port) continue; // that's me
        bool found = false; for (auto& g : LANS) if (g.ip == ip && g.port == port) { g.seen = 0; g.players = players; g.screen = scr; g.name = name; found = true; }
        if (!found) LANS.push_back({ ip, name, port, players, scr, 0 });
    }
    LANS.erase(std::remove_if(LANS.begin(), LANS.end(), [](const LanGame& g) { return g.seen > 4; }), LANS.end());
}
#else
static void lanListen(float) {}
#endif
