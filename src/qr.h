// Minimal QR encoder: byte mode, ECC level L, single-block versions 1-5 (up to 106 bytes), mask chosen by penalty.
// From Light Cycle Arena (src/qr.cpp, written for that project), made header-only for Belt Command's single-file build.
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <climits>
#include <algorithm>

struct QRCode { int size = 0; std::vector<unsigned char> m; bool at(int x, int y) const { return m[y * size + x] != 0; } };
namespace qrimpl {


static const int DATA_CW[6] = {0, 19, 34, 55, 80, 108}, EC_CW[6] = {0, 7, 10, 15, 20, 26};
static uint8_t gmul(uint8_t a, uint8_t b) { int r = 0; for (int i = 7; i >= 0; i--) { r = (r << 1) ^ ((r >> 7) * 0x11D); r ^= ((b >> i) & 1) * a; } return (uint8_t)r; }
static std::vector<uint8_t> rsDivisor(int deg) {
    std::vector<uint8_t> r(deg, 0); r[deg - 1] = 1; uint8_t root = 1;
    for (int i = 0; i < deg; i++) { for (int j = 0; j < deg; j++) { r[j] = gmul(r[j], root); if (j + 1 < deg) r[j] ^= r[j + 1]; } root = gmul(root, 2); }
    return r;
}
static std::vector<uint8_t> rsRemainder(const std::vector<uint8_t>& data, const std::vector<uint8_t>& div) {
    std::vector<uint8_t> r(div.size(), 0);
    for (uint8_t b : data) { uint8_t f = b ^ r[0]; r.erase(r.begin()); r.push_back(0); for (size_t i = 0; i < r.size(); i++) r[i] ^= gmul(div[i], f); }
    return r;
}
struct Grid {
    int n; std::vector<uint8_t> mod, fn;
    explicit Grid(int n_) : n(n_), mod(n_ * n_, 0), fn(n_ * n_, 0) {}
    void set(int x, int y, bool dark, bool func = true) { mod[y * n + x] = dark; if (func) fn[y * n + x] = 1; }
    bool get(int x, int y) const { return mod[y * n + x]; }
};
static void finder(Grid& g, int cx, int cy) {
    for (int dy = -4; dy <= 4; dy++) for (int dx = -4; dx <= 4; dx++) {
        int x = cx + dx, y = cy + dy; if (x < 0 || y < 0 || x >= g.n || y >= g.n) continue;
        int d = std::max(std::abs(dx), std::abs(dy)); g.set(x, y, d != 2 && d != 4);
    }
}
static void formatBits(Grid& g, int mask) {
    int data = (1 << 3) | mask, rem = data;   // ECC level L = 01
    for (int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    int bits = ((data << 10) | rem) ^ 0x5412; auto b = [&](int i) { return ((bits >> i) & 1) != 0; };
    for (int i = 0; i <= 5; i++) g.set(8, i, b(i));
    g.set(8, 7, b(6)); g.set(8, 8, b(7)); g.set(7, 8, b(8));
    for (int i = 9; i < 15; i++) g.set(14 - i, 8, b(i));
    for (int i = 0; i < 8; i++) g.set(g.n - 1 - i, 8, b(i));
    for (int i = 8; i < 15; i++) g.set(8, g.n - 15 + i, b(i));
    g.set(8, g.n - 8, true);   // the dark module
}
static bool maskBit(int m, int x, int y) {
    switch (m) {
        case 0: return (x + y) % 2 == 0; case 1: return y % 2 == 0; case 2: return x % 3 == 0; case 3: return (x + y) % 3 == 0;
        case 4: return (x / 3 + y / 2) % 2 == 0; case 5: return x * y % 2 + x * y % 3 == 0; case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
        default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
    }
}
static long penalty(const Grid& g) {
    long p = 0; int n = g.n;
    for (int pass = 0; pass < 2; pass++)   // runs of 5+ in rows / columns, and finder-like 1:1:3:1:1 patterns
        for (int a = 0; a < n; a++) {
            int run = 0; bool col = false; std::vector<int> seq;
            for (int b = 0; b < n; b++) {
                bool v = pass ? g.get(a, b) : g.get(b, a);
                if (b && v == col) run++; else { if (b) seq.push_back(run); run = 1; col = v; }
                if (run == 5) p += 3; else if (run > 5) p++;
            }
            seq.push_back(run);
            for (int b = 0; b + 6 < n; b++) {
                bool pat = true; const int P[7] = {1, 0, 1, 1, 1, 0, 1};
                for (int k = 0; k < 7; k++) if ((pass ? g.get(a, b + k) : g.get(b + k, a)) != (P[k] != 0)) { pat = false; break; }
                if (pat) {
                    bool before = true, after = true;
                    for (int k = 1; k <= 4; k++) { int q = b - k; if (q >= 0 && (pass ? g.get(a, q) : g.get(q, a))) before = false; q = b + 6 + k; if (q < n && (pass ? g.get(a, q) : g.get(q, a))) after = false; }
                    if (before || after) p += 40;
                }
            }
        }
    for (int y = 0; y + 1 < n; y++) for (int x = 0; x + 1 < n; x++) { bool c = g.get(x, y); if (g.get(x + 1, y) == c && g.get(x, y + 1) == c && g.get(x + 1, y + 1) == c) p += 3; }
    long dark = 0; for (uint8_t v : g.mod) dark += v; long total = (long)n * n;
    long k = (std::labs(dark * 20 - total * 10) + total - 1) / total - 1; if (k > 0) p += k * 10;
    return p;
}


} // namespace qrimpl
static QRCode qrEncode(const std::string& text) {
    using namespace qrimpl;
    QRCode out; int ver = 0;
    for (int v = 1; v <= 5; v++) if ((int)text.size() + 2 <= DATA_CW[v]) { ver = v; break; }   // 4-bit mode + 8-bit count + data, rounded up
    if (!ver) return out;
    // data codewords
    std::vector<int> bits; auto put = [&](int val, int len) { for (int i = len - 1; i >= 0; i--) bits.push_back((val >> i) & 1); };
    put(4, 4); put((int)text.size(), 8); for (unsigned char c : text) put(c, 8);
    int cap = DATA_CW[ver] * 8; put(0, std::min(4, cap - (int)bits.size())); while (bits.size() % 8) bits.push_back(0);
    for (int pad = 0xEC; (int)bits.size() < cap; pad ^= 0xEC ^ 0x11) put(pad, 8);
    std::vector<uint8_t> data(DATA_CW[ver]); for (size_t i = 0; i < bits.size(); i++) data[i >> 3] |= bits[i] << (7 - (i & 7));
    std::vector<uint8_t> ec = rsRemainder(data, rsDivisor(EC_CW[ver])), all = data; all.insert(all.end(), ec.begin(), ec.end());
    // function patterns
    int n = 17 + 4 * ver; Grid g(n);
    for (int i = 0; i < n; i++) { g.set(6, i, i % 2 == 0); g.set(i, 6, i % 2 == 0); }
    finder(g, 3, 3); finder(g, n - 4, 3); finder(g, 3, n - 4);
    if (ver >= 2) { int c = n - 7; for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) g.set(c + dx, c + dy, std::max(std::abs(dx), std::abs(dy)) != 1); }
    formatBits(g, 0);   // reserve the format areas
    // data, zig-zag from the bottom right
    size_t i = 0;
    for (int right = n - 1; right >= 1; right -= 2) {
        if (right == 6) right = 5;
        for (int vert = 0; vert < n; vert++) for (int j = 0; j < 2; j++) {
            int x = right - j; bool up = ((right + 1) & 2) == 0; int y = up ? n - 1 - vert : vert;
            if (!g.fn[y * n + x] && i < all.size() * 8) { g.mod[y * n + x] = (all[i >> 3] >> (7 - (i & 7))) & 1; i++; }
        }
    }
    // pick the mask with the lowest penalty
    int best = 0; long bestP = LONG_MAX;
    for (int m = 0; m < 8; m++) {
        Grid t = g; for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) if (!t.fn[y * n + x] && maskBit(m, x, y)) t.mod[y * n + x] ^= 1;
        formatBits(t, m); long p = penalty(t); if (p < bestP) { bestP = p; best = m; }
    }
    for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) if (!g.fn[y * n + x] && maskBit(best, x, y)) g.mod[y * n + x] ^= 1;
    formatBits(g, best);
    out.size = n; out.m.assign(g.mod.begin(), g.mod.end());
    return out;
}
