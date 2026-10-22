// Offline checks for THE GRID engine. Build with -DTHEGRID_BUILD_TESTS=ON.

#include "Engine.h"

#include <cstdio>
#include <functional>
#include <memory>
#include <vector>

using namespace thegrid;

static constexpr double sr = 48000.0;
static int failures = 0;

static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok) ++failures;
}

struct Out
{
    std::vector<Engine::Log::Ev> ev;
    std::vector<NoteOut> raw;   // with absolute offsets
    std::vector<Engine::Log::Ev> ons() const { std::vector<Engine::Log::Ev> v; for (auto& e : ev) if (e.on) v.push_back (e); return v; }
    std::vector<int> steps() const { std::vector<int> v; for (auto& e : ev) if (e.on) v.push_back (e.step); return v; }
    std::vector<int> notes() const { std::vector<int> v; for (auto& e : ev) if (e.on) v.push_back (e.note); return v; }
    std::vector<int64_t> rawOns() const { std::vector<int64_t> v; for (auto& e : raw) if (e.on) v.push_back (e.offset); return v; }
};

struct Run
{
    double seconds = 2.0, bpm = 120.0, startPpq = 0.0;
    int block = 256;
    uint64_t salt = 1;
    std::function<void (Params&)> setup = [] (Params&) {};
    std::function<void (Pattern&)> pattern = [] (Pattern&) {};
    std::function<void (Params&, double)> during = nullptr;
    std::vector<NoteIn> in;   // offsets are absolute samples
    double loopStart = 0.0, loopEnd = 0.0;
    double stopAt = -1.0;     // seconds
};

// sixteen steps on, each a different note, everything else plain
static void sixteen (Pattern& p)
{
    for (int s = 0; s < 16; ++s) { p.set (lkOn, 0, s, 1); p.set (lkNote, 0, s, s); }
}

static Out run (const Run& r)
{
    auto e = std::make_unique<Engine>();
    e->prepare (sr);
    e->setInstanceSalt (r.salt);
    auto log = std::make_unique<Engine::Log>();
    e->log = log.get();
    Params p;
    p.scale = 0;   // chromatic, so note numbers are easy to check
    r.setup (p);
    auto pat = std::make_unique<Pattern>();
    for (int i = 0; i < kPatterns; ++i) pat->clear (i);
    r.pattern (*pat);
    Out o;
    Clock c;
    c.bpm = r.bpm;
    std::vector<NoteIn> blockIn;
    std::array<NoteOut, 512> outs;
    const int64_t total = (int64_t) (r.seconds * sr);
    for (int64_t s = 0; s < total; s += r.block)
    {
        const int n = (int) std::min<int64_t> (r.block, total - s);
        if (r.during) r.during (p, (double) s / sr);
        c.playing = r.stopAt < 0.0 || (double) s / sr < r.stopAt;
        double ppq = r.startPpq + (double) s / sr * r.bpm / 60.0;
        c.looping = r.loopEnd > r.loopStart;
        c.loopStart = r.loopStart;
        c.loopEnd = r.loopEnd;
        if (c.looping && ppq >= r.loopEnd)
            ppq = r.loopStart + std::fmod (ppq - r.loopStart, r.loopEnd - r.loopStart);
        c.ppq = ppq;
        blockIn.clear();
        for (auto ni : r.in)
            if (ni.offset >= s && ni.offset < s + n) { ni.offset -= (int) s; blockIn.push_back (ni); }
        const int m = e->process (n, p, c, *pat, blockIn.data(), (int) blockIn.size(), outs.data(), (int) outs.size());
        for (int i = 0; i < m; ++i) { auto x = outs[(size_t) i]; x.offset += (int) s; o.raw.push_back (x); }
    }
    o.ev.assign (log->ev.begin(), log->ev.begin() + log->count);
    return o;
}

// in time order, every note-off matches a sounding note, no note struck twice without letting go, and nothing left hanging
static bool tidy (const Out& o, bool allowHanging = false)
{
    std::array<bool, 128> on {};
    int64_t last = -1;
    for (auto& e : o.raw)
    {
        if (e.offset < last) return false;
        last = e.offset;
        if (e.on) { if (on[(size_t) e.note]) return false; on[(size_t) e.note] = true; }
        else { if (! on[(size_t) e.note]) return false; on[(size_t) e.note] = false; }
    }
    if (allowHanging) return true;
    for (bool b : on) if (b) return false;
    return true;
}

static std::vector<int> seq (std::initializer_list<int> l) { return std::vector<int> (l); }

int main()
{
    constexpr int64_t six = 6000;   // a 1/16 at 120 BPM

    std::printf ("timing\n");
    {
        Run r;
        r.seconds = 1.0;
        r.pattern = sixteen;
        r.stopAt = 0.99;
        auto o = run (r);
        auto ons = o.ons();
        bool timed = ons.size() == 8;
        for (size_t i = 0; timed && i < ons.size(); ++i) timed = ons[i].sample == (int64_t) i * six;
        check (timed, "1/16 lands on the sample, 8 steps in a second at 120");
        check (o.steps() == seq ({ 0, 1, 2, 3, 4, 5, 6, 7 }), "steps play in order");
        check (o.notes()[0] == 48 && o.notes()[3] == 51, "note 0 is the root (C2 = MIDI 48), chromatic up from there");
        check (tidy (o), "every note gets its note-off, including on stop");

        Run a = r; a.block = 61;
        Run b = r; b.block = 1024;
        check (run (a).rawOns() == o.rawOns() && run (b).rawOns() == o.rawOns(), "block size doesn't move a note");
    }

    std::printf ("length and directions\n");
    {
        Run r;
        r.seconds = 1.0;
        r.pattern = [] (Pattern& p) { sixteen (p); p.setLength (0, 0, 4); };
        check (run (r).steps() == seq ({ 0, 1, 2, 3, 0, 1, 2, 3 }), "LENGTH 4 wraps after four");

        Run b = r; b.setup = [] (Params& p) { p.dir = dirBwd; };
        check (run (b).steps() == seq ({ 3, 2, 1, 0, 3, 2, 1, 0 }), "BWD");
        Run pd = r; pd.setup = [] (Params& p) { p.dir = dirPend; };
        check (run (pd).steps() == seq ({ 0, 1, 2, 3, 2, 1, 0, 1 }), "PEND bounces without repeating the ends");

        Run rd = r; rd.seconds = 4.0; rd.setup = [] (Params& p) { p.dir = dirRand; };
        auto l = run (rd).steps();
        bool inRange = l.size() == 32, varied = false;
        for (size_t i = 0; i < l.size(); ++i) { inRange = inRange && l[i] >= 0 && l[i] < 4; if (i > 0 && l[i] != l[i - 1]) varied = true; }
        check (inRange && varied, "RAND stays inside the length and moves around");

        Run sk; sk.seconds = 1.0; sk.pattern = sixteen; sk.setup = [] (Params& p) { p.skip = 100; };
        check (run (sk).steps() == seq ({ 0, 2, 4, 6, 8, 10, 12, 14 }), "SKIP 100 jumps every step");


        Run rs; rs.seconds = 4.0; rs.pattern = [] (Pattern& p) { sixteen (p); p.setLength (0, 0, 12); };
        rs.setup = [] (Params& p) { p.reset = 1; };
        auto rl = run (rs).steps();
        check (rl.size() == 32 && rl[12] == 0 && rl[15] == 3 && rl[16] == 0, "RESET 1 BAR starts a 12-step pattern again on the bar");
    }

    std::printf ("lanes\n");
    {
        Run ch; ch.seconds = 0.2;
        ch.pattern = [] (Pattern& p) { for (int l : { 0, 3, 7 }) p.set (lkOn, 0, Pattern::cell (l, 0), 1); };
        auto o = run (ch);
        auto ons = o.ons();
        check (ons.size() == 3 && ons[0].sample == 0 && ons[1].sample == 0 && ons[2].sample == 0, "three lanes on step 1 play a chord");
        check (ons.size() == 3 && ons[0].note == 72 && ons[2].note == 48, "a fresh lane's note: top row highest, bottom row the root");
        check (tidy (o), "a chord lets go cleanly");

        Run pm; pm.seconds = 2.0;
        pm.pattern = [] (Pattern& p)
        {
            for (int s = 0; s < 16; ++s) { p.set (lkOn, 0, Pattern::cell (0, s), 1); p.set (lkOn, 0, Pattern::cell (1, s), 1); }
            p.setLength (0, 0, 3);
            p.setLength (0, 1, 4);
        };
        std::vector<int> a, b;
        for (auto c : run (pm).steps()) (c < 16 ? a : b).push_back (c % 16);
        check (a.size() == 16 && b.size() == 16 && a[3] == 0 && a[5] == 2 && b[3] == 3 && b[4] == 0 && b[12] == 0,
               "lanes 3 and 4 long drift against each other");

        Run mu; mu.seconds = 0.5;
        mu.pattern = [] (Pattern& p) { p.set (lkOn, 0, Pattern::cell (0, 0), 1); p.set (lkOn, 0, Pattern::cell (1, 0), 1); p.setMute (0, 1, true); };
        auto mo = run (mu).steps();
        check (mo == seq ({ 0 }), "a muted lane stays quiet");

        Run tl; tl.seconds = 0.5;
        tl.pattern = [] (Pattern& p)
        {
            p.set (lkOn, 0, Pattern::cell (0, 0), 1); p.set (lkTie, 0, Pattern::cell (0, 0), 1);
            p.set (lkOn, 0, Pattern::cell (1, 1), 1); p.set (lkNote, 0, Pattern::cell (1, 1), 0);
        };
        auto to = run (tl);
        check (to.rawOns().size() == 2 && to.raw.size() >= 4 && to.raw[3].note == 72 && to.raw[3].offset == 2 * six,
               "a tie in one lane isn't broken by a note in another");
    }

    std::printf ("locks\n");
    {
        Run r;
        r.seconds = 0.5;
        r.pattern = [] (Pattern& p) { p.set (lkOn, 0, 0, 1); p.set (lkGate, 0, 0, 50); p.set (lkVel, 0, 0, 77); };
        auto o = run (r);
        check (o.raw.size() >= 2 && o.raw[0].on && o.raw[0].velocity == 77 && ! o.raw[1].on && o.raw[1].offset == 3000,
               "VELOCITY and GATE 50%: a 1/16 note lasts half a step");

        Run off; off.seconds = 0.5; off.pattern = [] (Pattern& p) { p.set (lkOn, 0, 2, 1); };
        check (run (off).steps() == seq ({ 2 }), "steps that are off stay quiet");

        Run c0; c0.seconds = 2.0; c0.pattern = [] (Pattern& p) { sixteen (p); for (int s = 0; s < 16; ++s) p.set (lkChance, 0, s, 0); };
        check (run (c0).ons().empty(), "CHANCE 0 never plays");

        Run c5; c5.seconds = 32.0; c5.pattern = [] (Pattern& p) { sixteen (p); for (int s = 0; s < 16; ++s) p.set (lkChance, 0, s, 50); };
        const auto n5 = run (c5).ons().size();
        check (n5 > 96 && n5 < 160, "CHANCE 50 plays about half of 256 steps");

        Run rt; rt.seconds = 0.5; rt.pattern = [] (Pattern& p) { p.set (lkOn, 0, 0, 1); p.set (lkRatchet, 0, 0, 3); };
        auto ro = run (rt);
        check (ro.rawOns() == std::vector<int64_t> ({ 0, 2000, 4000 }) && tidy (ro), "RATCHET 3 hits three times inside the step");

        Run sw; sw.seconds = 0.5; sw.pattern = sixteen; sw.setup = [] (Params& p) { p.swing = 66; };
        auto so = run (sw).ons();
        check (so.size() >= 3 && so[0].sample == 0 && so[1].sample == six + 1920 && so[2].sample == 2 * six, "SWING 66 pushes the off-beats late");
    }

    std::printf ("ties\n");
    {
        Run same; same.seconds = 0.5;
        same.pattern = [] (Pattern& p) { for (int s : { 0, 1 }) { p.set (lkOn, 0, s, 1); p.set (lkNote, 0, s, 5); } p.set (lkTie, 0, 0, 1); p.set (lkGate, 0, 1, 50); };
        auto o = run (same);
        check (o.rawOns().size() == 1 && o.raw.size() == 2 && o.raw[1].offset == six + 3000, "TIE into the same note: one long note, no retrigger");

        Run diff; diff.seconds = 0.5;
        diff.pattern = [] (Pattern& p) { for (int s : { 0, 1 }) p.set (lkOn, 0, s, 1); p.set (lkNote, 0, 1, 7); p.set (lkTie, 0, 0, 1); };
        auto d = run (diff);
        bool legato = d.raw.size() >= 3 && d.raw[1].on && d.raw[1].offset == six && ! d.raw[2].on && d.raw[2].offset == six + 1
                      && d.raw[2].note == 72;
        check (legato && tidy (d), "TIE into a different note: the new one starts before the old one lets go");

        Run gap; gap.seconds = 0.5;
        gap.pattern = [] (Pattern& p) { p.set (lkOn, 0, 0, 1); p.set (lkTie, 0, 0, 1); };
        auto g = run (gap);
        check (g.raw.size() >= 2 && ! g.raw[1].on && g.raw[1].offset == 2 * six, "TIE over an empty step holds to the end of it");
    }

    std::printf ("randomness\n");
    {
        auto pat = [] (Pattern& p) { sixteen (p); for (int s = 0; s < 16; ++s) p.set (lkChance, 0, s, 50); };
        Run a; a.seconds = 8.0; a.pattern = pat; a.setup = [] (Params& p) { p.dir = dirBrown; p.seed = 1; };
        Run b = a; b.block = 999; b.salt = 77;
        check (run (a).steps() == run (b).steps(), "SEED LOCKED: BROWN and CHANCE come out the same every time");

        Run f1 = a; f1.setup = [] (Params& p) { p.dir = dirBrown; p.seed = 0; };
        Run f2 = f1; f2.salt = 77;
        check (run (f1).steps() != run (f2).steps(), "SEED FREE: two GRIDs roll different dice");

        Run lp; lp.seconds = 8.0; lp.pattern = pat; lp.loopStart = 0.0; lp.loopEnd = 4.0;
        lp.setup = [] (Params& p) { p.seed = 1; };
        auto ll = run (lp).steps();
        bool same = ll.size() > 8;
        const size_t half = ll.size() / 2;
        for (size_t i = 0; same && i < half && i + half < ll.size(); ++i) same = ll[i] == ll[i + half];
        check (same, "LOCKED: every pass of a Logic cycle plays the same maybes");

        Run lf = lp; lf.setup = [] (Params& p) { p.seed = 0; };
        auto fl = run (lf).steps();
        bool differs = false;
        for (size_t i = 0; i < fl.size() / 2; ++i) differs = differs || fl[i] != fl[i + fl.size() / 2];
        check (differs || fl.size() != ll.size(), "FREE: each pass of the cycle rolls again");
    }

    std::printf ("cycle, stop, patterns, transpose\n");
    {
        Run lp; lp.seconds = 4.0; lp.pattern = sixteen; lp.loopStart = 0.0; lp.loopEnd = 2.0;
        auto o = run (lp);
        auto st = o.steps();
        bool loops = st.size() == 32;
        for (size_t i = 0; loops && i < st.size(); ++i) loops = st[i] == (int) (i % 8);
        check (loops && tidy (o, true), "a two-beat cycle plays steps 1 to 8 over and over");

        Run sp; sp.seconds = 1.0; sp.pattern = [] (Pattern& p) { p.set (lkOn, 0, 0, 1); p.set (lkTie, 0, 0, 1); p.setLength (0, 0, 1); };
        sp.stopAt = 0.3;
        check (tidy (run (sp)), "stopping lets go of a held tie");

        Run pt; pt.seconds = 4.0; pt.pattern = [] (Pattern& p) { sixteen (p); for (int s = 0; s < 16; ++s) { p.set (lkOn, 1, s, 1); p.set (lkNote, 1, s, 40); } };
        pt.during = [] (Params& p, double t) { if (t > 0.7) p.slot = 1; };
        auto n = run (pt).notes();
        bool ok = n.size() == 32;
        for (int i = 0; ok && i < 32; ++i) ok = i < 16 ? n[(size_t) i] < 70 : n[(size_t) i] == 88;
        check (ok, "a new pattern waits for the next bar line");

        Run tr; tr.seconds = 1.0; tr.pattern = sixteen; tr.setup = [] (Params& p) { p.transpose = true; };
        tr.in.push_back ({ 0, 67, 100, true });   // G3: up seven
        check (run (tr).notes()[0] == 55, "TRANSPOSE: G3 in moves it up a fifth");

        Run q; q.seconds = 0.5; q.pattern = [] (Pattern& p) { p.set (lkOn, 0, 0, 1); p.set (lkNote, 0, 0, 1); };
        q.setup = [] (Params& p) { p.scale = 1; p.key = 2; };   // D major
        check (run (q).notes()[0] == 50, "a note between scale notes falls to the one below (D major: D#2 falls to D2)");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
