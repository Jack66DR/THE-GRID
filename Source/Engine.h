// THE GRID engine: a Hapax-style step sequencer. Header only, no JUCE, so the offline test can drive it directly.
//
// One track per instance. A pattern is up to 128 steps, laid out on the 8 x 16 grid as eight rows of sixteen:
// row 1 is steps 1 to 16, row 2 is 17 to 32, and so on. LENGTH says where the pattern ends.
//
// Every step carries its own locks: ON, NOTE (in the key and scale), VELOCITY, GATE, CHANCE, RATCHET and TIE.
//
// Timing comes from where the song is, so it follows Logic's tempo, cycle and relocation. Directions FWD, BWD, PEND
// and RAND are worked out straight from the song position. BROWN (a random walk) and SKIP remember where they were.
// SEED LOCKED makes every random thing (RAND, BROWN, SKIP, CHANCE) the same every time the song passes the same point;
// FREE rolls new dice on every pass.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace thegrid
{
    constexpr int kRows = 8;
    constexpr int kCols = 16;
    constexpr int kSteps = kRows * kCols;   // 128
    constexpr int kPatterns = 8;
    constexpr int kMaxNote = 48;            // four octaves above the root
    constexpr int kMaxRatchet = 8;

    constexpr int kNumRates = 12;
    inline const char* const kRateName[kNumRates] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                                                      "1/4T", "1/8T", "1/16T", "1/4.", "1/8.", "1/16." };
    inline constexpr double kRatePpq[kNumRates] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                                    2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.5, 0.75, 0.375 };

    enum Dir { dirFwd, dirBwd, dirPend, dirRand, dirBrown };
    constexpr int kNumDirs = 5;
    inline const char* const kDirName[kNumDirs] = { "FWD", "BWD", "PEND", "RAND", "BROWN" };

    constexpr int kNumResets = 5;
    inline const char* const kResetName[kNumResets] = { "NEVER", "1 BAR", "2 BARS", "4 BARS", "8 BARS" };
    inline constexpr int kResetBars[kNumResets] = { 0, 1, 2, 4, 8 };

    inline const char* const kSeedName[2] = { "FREE", "LOCKED" };

    // Scales as twelve-bit masks, bit 0 the root.
    constexpr int kNumScales = 15;
    inline const char* const kScaleName[kNumScales] = { "CHROMATIC", "MAJOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN",
                                                        "MINOR", "LOCRIAN", "HARM MINOR", "MEL MINOR", "PHRYG DOM",
                                                        "MAJ PENT", "MIN PENT", "BLUES", "WHOLE TONE" };
    constexpr int kDefaultScale = 6;   // MINOR
    inline uint16_t scaleMask (int scale)
    {
        auto m = [] (std::initializer_list<int> d) { uint16_t r = 0; for (int x : d) r = (uint16_t) (r | (1u << x)); return r; };
        switch (std::clamp (scale, 0, kNumScales - 1))
        {
            case 0:  return 0x0fff;
            case 1:  return m ({ 0, 2, 4, 5, 7, 9, 11 });
            case 2:  return m ({ 0, 2, 3, 5, 7, 9, 10 });
            case 3:  return m ({ 0, 1, 3, 5, 7, 8, 10 });
            case 4:  return m ({ 0, 2, 4, 6, 7, 9, 11 });
            case 5:  return m ({ 0, 2, 4, 5, 7, 9, 10 });
            case 6:  return m ({ 0, 2, 3, 5, 7, 8, 10 });
            case 7:  return m ({ 0, 1, 3, 5, 6, 8, 10 });
            case 8:  return m ({ 0, 2, 3, 5, 7, 8, 11 });
            case 9:  return m ({ 0, 2, 3, 5, 7, 9, 11 });
            case 10: return m ({ 0, 1, 4, 5, 7, 8, 10 });
            case 11: return m ({ 0, 2, 4, 7, 9 });
            case 12: return m ({ 0, 3, 5, 7, 10 });
            case 13: return m ({ 0, 3, 5, 6, 7, 10 });
            default: return m ({ 0, 2, 4, 6, 8, 10 });
        }
    }

    // The nearest note in the scale, ties going down. Semitones above the root.
    inline int quantise (int semis, int scale)
    {
        const uint16_t mask = scaleMask (scale);
        auto in = [mask] (int s) { return ((mask >> (((s % 12) + 12) % 12)) & 1) != 0; };
        for (int d = 0; d < 12; ++d)
        {
            if (in (semis - d)) return semis - d;
            if (in (semis + d)) return semis + d;
        }
        return semis;
    }

    // The next scale note up or down from a quantised one: how a dragged pad moves through the scale.
    inline int scaleStep (int semis, int scale, int delta)
    {
        int s = quantise (std::clamp (semis, 0, kMaxNote), scale);
        const int dir = delta > 0 ? 1 : -1;
        for (int n = 0; n < std::abs (delta); ++n)
        {
            int t = s + dir;
            while (t >= 0 && t <= kMaxNote && quantise (t, scale) != t) t += dir;
            if (t < 0 || t > kMaxNote) break;
            s = t;
        }
        return s;
    }

    inline const char* const kNoteName[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // The locks every step carries.
    enum Lock { lkOn, lkNote, lkVel, lkGate, lkChance, lkRatchet, lkTie };
    constexpr int kNumLocks = 7;
    inline constexpr int kLockMin[kNumLocks] = { 0, 0,        1,   5,   0,   1,           0 };
    inline constexpr int kLockMax[kNumLocks] = { 1, kMaxNote, 127, 100, 100, kMaxRatchet, 1 };
    inline constexpr int kLockDef[kNumLocks] = { 0, 12,       100, 50,  100, 1,           0 };
    inline const char* const kLockName[kNumLocks] = { "ON", "NOTE", "VEL", "GATE", "CHANCE", "RATCHET", "TIE" };

    // All eight patterns. The editor writes, the audio thread reads; every value is atomic.
    struct Pattern
    {
        std::array<std::array<std::atomic<uint8_t>, kPatterns * kSteps>, kNumLocks> v {};
        std::array<std::atomic<uint8_t>, kPatterns> len {};
        std::atomic<uint32_t> version { 0 };

        Pattern()
        {
            for (int p = 0; p < kPatterns; ++p) clear (p);
            starter (0);
        }

        static size_t at (int p, int s) { return (size_t) (std::clamp (p, 0, kPatterns - 1) * kSteps + std::clamp (s, 0, kSteps - 1)); }
        int get (Lock l, int p, int s) const { return v[(size_t) l][at (p, s)].load (std::memory_order_relaxed); }
        void set (Lock l, int p, int s, int value)
        {
            v[(size_t) l][at (p, s)].store ((uint8_t) std::clamp (value, kLockMin[l], kLockMax[l]), std::memory_order_relaxed);
        }
        bool on (int p, int s) const { return get (lkOn, p, s) != 0; }
        int length (int p) const { return std::clamp ((int) len[(size_t) std::clamp (p, 0, kPatterns - 1)].load (std::memory_order_relaxed), 1, kSteps); }
        void setLength (int p, int n) { len[(size_t) std::clamp (p, 0, kPatterns - 1)].store ((uint8_t) std::clamp (n, 1, kSteps), std::memory_order_relaxed); }

        void clearStep (int p, int s)
        {
            for (int l = 0; l < kNumLocks; ++l) set ((Lock) l, p, s, kLockDef[l]);
        }

        // An empty pattern, sixteen steps long.
        void clear (int p)
        {
            for (int s = 0; s < kSteps; ++s) clearStep (p, s);
            setLength (p, 16);
            ++version;
        }

        // Something to hear on the first press of play: a little minor riff with a tie, a ratchet and a maybe.
        void starter (int p)
        {
            clear (p);
            struct S { int step, note, vel, gate, chance, ratchet, tie; };
            static constexpr S riff[] = {
                { 0, 12, 110, 60, 100, 1, 0 }, { 3, 19, 90, 40, 100, 1, 0 }, { 6, 15, 100, 50, 100, 1, 1 },
                { 7, 15, 100, 50, 100, 1, 0 }, { 10, 22, 80, 30, 50, 1, 0 }, { 12, 12, 100, 50, 100, 3, 0 },
                { 14, 24, 95, 40, 100, 1, 0 } };
            for (auto& r : riff)
            {
                set (lkOn, p, r.step, 1);
                set (lkNote, p, r.step, r.note);
                set (lkVel, p, r.step, r.vel);
                set (lkGate, p, r.step, r.gate);
                set (lkChance, p, r.step, r.chance);
                set (lkRatchet, p, r.step, r.ratchet);
                set (lkTie, p, r.step, r.tie);
            }
            ++version;
        }

        void copyPattern (int from, int to)
        {
            if (from == to) return;
            for (int l = 0; l < kNumLocks; ++l)
                for (int s = 0; s < kSteps; ++s)
                    set ((Lock) l, to, s, get ((Lock) l, from, s));
            setLength (to, length (from));
            ++version;
        }
    };

    struct Params
    {
        int key = 0;          // 0 = C
        int scale = kDefaultScale;
        int octave = 2;       // root octave, Logic naming: 2 is C2 (MIDI 48)
        int rate = 4;         // 1/16
        int dir = dirFwd;
        int skip = 0;         // percent: FWD, BWD and PEND sometimes jump a step
        int swing = 50;       // percent, 50 is straight
        int reset = 0;
        int seed = 0;         // 0 FREE, 1 LOCKED
        bool transpose = false;
        bool thru = true;
        bool run = true;
        int slot = 0;
    };

    struct Clock
    {
        double bpm = 120.0;
        double ppq = 0.0;
        double barPpq = 4.0;
        bool playing = false;
        bool looping = false;
        double loopStart = 0.0, loopEnd = 0.0;
    };

    struct NoteIn { int offset = 0; int note = 60; int velocity = 100; bool on = true; };
    struct NoteOut { int offset = 0; int note = 60; int velocity = 0; bool on = false; };

    inline uint64_t mix (uint64_t h)
    {
        h ^= h >> 31; h *= 0x94D049BB133111EBull; h ^= h >> 29; h *= 0xBF58476D1CE4E5B9ull; h ^= h >> 32;
        return h;
    }
    inline uint64_t hash3 (int64_t a, uint64_t b, uint64_t c)
    {
        return mix ((uint64_t) a * 0x9E3779B97F4A7C15ull ^ mix (b + 0x632BE59BD9B4E019ull) ^ (c * 0xD6E8FEB86659FD93ull));
    }

    class Engine
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            lastId = INT64_MIN;
            expectedPpq = -1.0e9;
            now = 0;
            wasRunning = false;
            qn = 0;
            sounding.fill (0);
            heldTie = -1;
            walkValid = false;
        }

        // A per-instance number mixed into FREE randomness, so two GRIDs don't roll the same dice.
        void setInstanceSalt (uint64_t s) { instanceSalt = s; }

        // For the offline test: every note, with the sample it landed on since prepare().
        struct Log
        {
            struct Ev { int64_t sample; int note, velocity, step; bool on; };
            std::array<Ev, 16384> ev {};
            int count = 0;
        };
        Log* log = nullptr;

        // Works out a block. Notes in are sorted by offset. Returns how many notes it wrote to out (at most maxOut).
        int process (int n, const Params& p, const Clock& clock, const Pattern& pat,
                     const NoteIn* in, int numIn, NoteOut* out, int maxOut)
        {
            numOut = 0;
            blockLen = n;
            outBuf = out;
            outMax = maxOut;

            const bool running = p.run && clock.playing && clock.bpm > 1.0;
            const double pps = clock.bpm / 60.0 / sr;
            const double barPpq = std::max (0.25, clock.barPpq);

            // the transport stopped: let go of everything
            if (wasRunning && ! running)
            {
                allOff (now);
                expectedPpq = -1.0e9;
                lastId = INT64_MIN;
                walkValid = false;
                uiStep = -1;
            }
            if (running && ! wasRunning)
                ++pass;
            wasRunning = running;
            if (! running)
                activeSlot = std::clamp (p.slot, 0, kPatterns - 1);

            numTicks = 0;
            int switchAt = -1;
            if (running)
            {
                const double p0 = clock.ppq, p1 = clock.ppq + pps * n;
                const bool continuous = std::abs (p0 - expectedPpq) < 1.0e-3;
                const bool wraps = clock.looping && clock.loopEnd > clock.loopStart + 0.01
                                   && p0 < clock.loopEnd - 1.0e-9 && p1 > clock.loopEnd;
                if (! continuous && expectedPpq > -1.0e8)
                    ++pass;                  // a relocation: FREE gets new dice
                if (wraps)
                {
                    const int base = std::clamp ((int) std::llround ((clock.loopEnd - p0) / pps), 0, n);
                    scan (p, barPpq, p0, base, 0, continuous, pps, -1);
                    const int before = numTicks;
                    if (base < n)
                        scan (p, barPpq, clock.loopStart, n - base, base, false, pps, 1);
                    wrapPending = numTicks == before;   // no tick after the wrap yet: the next one starts the new pass
                }
                else
                    scan (p, barPpq, p0, n, 0, continuous, pps, -1);
                expectedPpq = wraps ? clock.loopStart + (p1 - clock.loopEnd) : p1;

                // a new pattern takes over on the next bar line
                const double bar0 = std::floor (p0 / barPpq), bar1 = std::floor ((p1 - 1.0e-9) / barPpq);
                switchAt = (bar1 > bar0 || std::abs (p0 - bar0 * barPpq) < 1.0e-9)
                               ? std::clamp ((int) std::llround ((bar1 * barPpq - p0) / pps), 0, n - 1) : -1;
                if (wraps) switchAt = std::clamp ((int) std::llround ((clock.loopEnd - p0) / pps), 0, n - 1);

                uiBar = (int) std::floor (p0 / barPpq) + 1;
                uiBeat = (int) std::floor (std::fmod (std::max (0.0, p0), barPpq)) + 1;
            }

            // ---- play the ticks in order
            int ii = 0;
            for (int t = 0; t < numTicks; ++t)
            {
                const auto& tk = ticks[(size_t) t];
                while (ii < numIn && in[ii].offset <= tk.offset)
                {
                    if (in[ii].on && p.transpose) transpose = in[ii].note - 60;
                    ++ii;
                }
                if (tk.newPass || (t == 0 && passPending)) { ++pass; passPending = false; }
                if (switchAt >= 0 && tk.offset >= switchAt)
                {
                    activeSlot = std::clamp (p.slot, 0, kPatterns - 1);
                    switchAt = -1;
                }
                const int64_t T = now + tk.offset;
                flushBefore (T);
                flushOffsAt (T);
                step (T, tk.count, tk.interval, p, pat);
            }
            while (ii < numIn)
            {
                if (in[ii].on && p.transpose) transpose = in[ii].note - 60;
                ++ii;
            }
            if (switchAt >= 0)
                activeSlot = std::clamp (p.slot, 0, kPatterns - 1);
            if (wrapPending) { passPending = true; wrapPending = false; }
            flushBefore (now + n);

            uiPlaying = clock.playing;
            uiBpm = (float) clock.bpm;
            uiSlot = activeSlot;
            uiTranspose = transpose;
            now += n;
            return numOut;
        }

        // the editor's view
        std::atomic<int> uiStep { -1 }, uiNote { -1 }, uiSlot { 0 }, uiBar { 1 }, uiBeat { 1 }, uiTranspose { 0 };
        std::atomic<uint32_t> uiNotes { 0 }, uiSteps { 0 };   // bumped on every note, and every step the playhead lands on
        std::atomic<bool> uiPlaying { false };
        std::atomic<float> uiBpm { 120.0f };

    private:
        struct Tick { int offset; int64_t count; double interval; bool newPass; };
        struct Ev { int64_t t; int note, vel; bool on; uint32_t id; };   // an off only ends the note with its own id

        static int64_t floorDiv (int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
        static int64_t mod (int64_t a, int64_t b) { return a - floorDiv (a, b) * b; }

        uint64_t salt (const Params& p) const
        {
            return p.seed == 1 ? 0x5EEDull : mix (pass * 0x2545F4914F6CDD1Dull + instanceSalt + 1);
        }

        // Finds the ticks in the 'len' samples starting at 'from' (quarter notes), as offsets from base.
        // A tick belongs to the sample it rounds to, so every block size agrees on where it lands.
        void scan (const Params& p, double barPpq, double from, int len, int base, bool cont, double pps, int firstIsNewPass)
        {
            if (! cont) lastId = INT64_MIN;
            const double D = kRatePpq[std::clamp (p.rate, 0, kNumRates - 1)];
            const double swingOff = (std::clamp (p.swing, 50, 75) - 50) / 50.0 * D;   // how late the off-beats land
            const int rb = kResetBars[std::clamp (p.reset, 0, kNumResets - 1)];
            const int64_t period = rb > 0 ? std::max<int64_t> (1, (int64_t) std::llround (rb * barPpq / D)) : 0;
            auto at = [&] (int64_t k) { return (double) k * D + ((k & 1) != 0 ? swingOff : 0.0); };
            const double to = from + pps * len;
            bool first = firstIsNewPass > 0;
            for (int64_t k = (int64_t) std::floor (from / D) - 2; k <= (int64_t) std::floor (to / D) + 1; ++k)
            {
                const double tk = at (k);
                const int64_t rel = std::llround ((tk - from) / pps);
                if (rel < 0 || rel >= len || k <= lastId)
                    continue;
                lastId = k;
                if (numTicks < (int) ticks.size())
                    ticks[(size_t) numTicks++] = { std::clamp (base + (int) rel, 0, blockLen - 1),
                                                   period > 0 ? mod (k, period) : k,
                                                   (at (k + 1) - tk) / pps, first };
                first = false;
            }
        }

        // ---- where the playhead lands for a count
        static int closedForm (int64_t c, int L, int dir, uint64_t s)
        {
            switch (dir)
            {
                case dirBwd:  return L - 1 - (int) mod (c, L);
                case dirPend:
                {
                    if (L == 1) return 0;
                    const int t = (int) mod (c, 2 * L - 2);
                    return t < L ? t : 2 * L - 2 - t;
                }
                case dirRand: return (int) (hash3 (c, s, 0xA11) % (uint64_t) L);
                default:      return (int) mod (c, L);
            }
        }

        void walkStart (int L, int dir)
        {
            walkPos = dir == dirBwd ? L - 1 : 0;
            walkSign = 1;
        }

        void walkAdvance (int64_t c, int L, const Params& p, uint64_t s)
        {
            const uint64_t h = hash3 (c, s, 0xB0);
            walkPos = (int) mod (walkPos, L);
            if (p.dir == dirBrown)
            {
                // mostly forward, sometimes back, now and then a stumble or a leap
                const int r = (int) (h % 100);
                const int d = r < 30 ? -1 : r < 40 ? 0 : r < 90 ? 1 : 2;
                walkPos = (int) mod (walkPos + d, L);
                return;
            }
            const int moves = ((int) ((h >> 16) % 100) < std::clamp (p.skip, 0, 100)) ? 2 : 1;
            for (int m = 0; m < moves; ++m)
            {
                if (p.dir == dirBwd) walkPos = (int) mod (walkPos - 1, L);
                else if (p.dir == dirPend)
                {
                    if (L == 1) { walkPos = 0; continue; }
                    if (walkPos + walkSign < 0 || walkPos + walkSign > L - 1) walkSign = -walkSign;
                    walkPos += walkSign;
                }
                else walkPos = (int) mod (walkPos + 1, L);
            }
        }

        int position (int64_t c, int L, const Params& p)
        {
            const uint64_t s = salt (p);
            const bool walks = p.dir == dirBrown || (p.skip > 0 && p.dir != dirRand);
            if (! walks || c < 0)
            {
                walkValid = false;
                return closedForm (c, L, p.dir, s);
            }
            if (walkValid && c == walkCount)
                return (int) mod (walkPos, L);
            if (walkValid && c == walkCount + 1)
                walkAdvance (c, L, p, s);
            else if (c == 0)
                walkStart (L, p.dir);
            else if (p.seed == 1 && c <= (1 << 20))
            {
                // LOCKED: replay the walk from the start so the same bar always walks the same way
                walkStart (L, p.dir);
                for (int64_t k = 1; k <= c; ++k)
                    walkAdvance (k, L, p, s);
            }
            else if (! walkValid)
                walkStart (L, p.dir);
            else
                walkAdvance (c, L, p, s);   // FREE after a jump or a cycle: carry on from where it was
            walkCount = c;
            walkValid = true;
            return (int) mod (walkPos, L);
        }

        // ---- one step
        void step (int64_t T, int64_t c, double interval, const Params& p, const Pattern& pat)
        {
            const int s = activeSlot;
            const int L = pat.length (s);
            const int pos = position (c, L, p);
            uiStep = pos;
            uiSteps.fetch_add (1, std::memory_order_release);

            bool plays = pat.on (s, pos);
            if (plays)
            {
                const int chance = pat.get (lkChance, s, pos);
                if (chance < 100)
                    plays = chance > 0 && (int) (hash3 (c, salt (p), (uint64_t) pos + 0xC4A) % 100) < chance;
            }

            if (! plays)
            {
                // a tied note holds over an empty step, then lets go at its end
                if (heldTie >= 0)
                {
                    queue (T + std::max<int64_t> (1, std::llround (interval)), heldTie, 0, false, heldId);
                    heldTie = -1;
                }
                return;
            }

            const int root = 12 * (std::clamp (p.octave, 0, 6) + 2) + std::clamp (p.key, 0, 11);   // Logic: C2 is MIDI 48
            const int note = std::clamp (root + quantise (pat.get (lkNote, s, pos), p.scale) + transpose, 0, 127);
            const int vel = pat.get (lkVel, s, pos);
            const double gate = pat.get (lkGate, s, pos) / 100.0;
            const int ratchet = pat.get (lkRatchet, s, pos);
            const bool tie = pat.get (lkTie, s, pos) != 0;
            const double sub = interval / ratchet;
            const int64_t minLen = std::max<int64_t> (1, (int64_t) std::llround (0.004 * sr));

            for (int i = 0; i < ratchet; ++i)
            {
                const int64_t ti = T + (int64_t) std::llround (i * sub);
                const bool last = i == ratchet - 1;
                uint32_t id = ++nextId == 0 ? ++nextId : nextId;
                if (i == 0 && heldTie >= 0)
                {
                    if (heldTie == note && ratchet == 1)
                        id = heldId;   // the tie carries on into this step: no new note, this step's gate and tie take over
                    else
                    {
                        // legato: the new note starts, the held one lets go a sample later
                        emitNow (ti, note, vel, true, id);
                        if (heldTie != note) queue (ti + 1, heldTie, 0, false, heldId);
                    }
                    heldTie = -1;
                }
                else if (i == 0)
                    emitNow (ti, note, vel, true, id);
                else
                    queue (ti, note, vel, true, id);

                if (last && tie)
                {
                    heldTie = note;
                    heldId = id;
                }
                else
                    queue (ti + std::max (minLen, (int64_t) std::llround (sub * gate)), note, 0, false, id);
            }

            uiNote = note;
            uiNotes.fetch_add (1, std::memory_order_release);
            if (log != nullptr && log->count < (int) log->ev.size())
                log->ev[(size_t) log->count++] = { T, note, vel, pos, true };
        }

        // ---- note bookkeeping: a small queue of what's coming, and which notes are sounding
        void queue (int64_t t, int note, int vel, bool on, uint32_t id)
        {
            if (qn < (int) q.size())
                q[(size_t) qn++] = { t, note, vel, on, id };
        }

        // A note struck while it's still sounding is let go first. An off for a note that's since been struck
        // again (a different id) is ignored, so a short new note isn't cut by the old one's ending.
        void emitNow (int64_t t, int note, int vel, bool on, uint32_t id)
        {
            note = std::clamp (note, 0, 127);
            auto& sid = sounding[(size_t) note];
            if (on)
            {
                if (sid != 0) write (t, note, 0, false);
                write (t, note, vel, true);
                sid = id;
            }
            else if (sid != 0 && sid == id)
            {
                write (t, note, 0, false);
                sid = 0;
            }
        }

        // Plays what's queued before T, earliest first, offs before ons at the same sample.
        void flushBefore (int64_t T)
        {
            for (;;)
            {
                int best = -1;
                for (int i = 0; i < qn; ++i)
                {
                    const auto& e = q[(size_t) i];
                    if (e.t >= T) continue;
                    if (best < 0 || e.t < q[(size_t) best].t || (e.t == q[(size_t) best].t && ! e.on && q[(size_t) best].on))
                        best = i;
                }
                if (best < 0) return;
                const Ev e = q[(size_t) best];
                q[(size_t) best] = q[(size_t) --qn];
                emitNow (e.t, e.note, e.vel, e.on, e.id);
            }
        }

        void flushOffsAt (int64_t T)
        {
            for (int i = 0; i < qn;)
            {
                const Ev e = q[(size_t) i];
                if (! e.on && e.t == T)
                {
                    q[(size_t) i] = q[(size_t) --qn];
                    emitNow (e.t, e.note, 0, false, e.id);
                }
                else ++i;
            }
        }

        void allOff (int64_t t)
        {
            qn = 0;
            heldTie = -1;
            for (int nt = 0; nt < 128; ++nt)
                if (sounding[(size_t) nt] != 0)
                {
                    write (t, nt, 0, false);
                    sounding[(size_t) nt] = 0;
                }
        }

        void write (int64_t t, int note, int vel, bool on)
        {
            const int off = (int) std::clamp<int64_t> (t - now, 0, std::max (0, blockLen - 1));
            if (outBuf != nullptr && numOut < outMax)
                outBuf[numOut++] = { off, note, vel, on };
            if (! on && log != nullptr && log->count < (int) log->ev.size())
                log->ev[(size_t) log->count++] = { t, note, 0, -1, false };
        }

        double sr = 48000.0;
        int64_t lastId = INT64_MIN;
        std::array<Tick, 512> ticks {};
        int numTicks = 0;
        double expectedPpq = -1.0e9;
        int64_t now = 0;
        bool wasRunning = false;
        int activeSlot = 0, transpose = 0;
        uint64_t pass = 0, instanceSalt = 0;
        bool wrapPending = false, passPending = false;

        std::array<Ev, 1024> q {};
        int qn = 0;
        std::array<uint32_t, 128> sounding {};   // the id of the note sounding on each key, 0 for none
        int heldTie = -1;
        uint32_t heldId = 0, nextId = 0;

        bool walkValid = false;
        int64_t walkCount = 0;
        int walkPos = 0, walkSign = 1;

        NoteOut* outBuf = nullptr;
        int numOut = 0, outMax = 0, blockLen = 1;
    };
}
