# THE GRID

**manners! DSP** step sequencer. An Audio Unit MIDI effect (plus VST3), macOS universal (Apple Silicon and Intel).

One track per instance. 128 steps on an 8 x 16 grid of pads, read like a page: row 1 is steps 1 to 16, row 2 is 17 to 32,
and so on down. Every step carries its own locks: on or off, NOTE, VELOCITY, GATE, CHANCE, RATCHET and TIE.
Notes live in a key and scale, so changing KEY or SCALE takes the whole pattern with it.

It follows Logic: tempo, time signature, cycle and wherever you drop the playhead.

## Build

GitHub Actions builds it on every push to `main` (Actions tab, "Build THE GRID"). It also runs the offline engine test and Apple's `auval` check.
Download the `THE-GRID-macOS` artifact from the finished run. Inside are `THE-GRID-AU.zip` and `THE-GRID-VST3.zip`.

## Install

```sh
sudo unzip -o THE-GRID-AU.zip -d /Library/Audio/Plug-Ins/Components/
sudo xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/Components/THE GRID.component"
```

In Logic it goes in a Software Instrument track's **MIDI FX** slot (above the instrument slot): MIDI FX > Audio Units > manners! DSP > THE GRID.
It plays whatever instrument is on that track. Press play. One GRID per track; give each its own LENGTH and they drift against each other.

## The pads

| | |
|---|---|
| **STEP** page | Click a pad on or off. Drag to paint. |
| **NOTE** page | Drag up or down through the scale. |
| **VEL** page | Drag for velocity, 1 to 127. |
| **GATE** page | Drag for note length, 5 to 100 % of the step. |
| **CHANCE** page | Drag for the chance the step plays, in fives. |
| **RATCHET** page | Drag for 1 to 8 hits inside the step. |
| **TIE** page | Click to tie a step into the next. Same note: one long note. Different note: legato. Empty next step: it holds through it. |
| Any value page | Clicking an off step turns it on. Double-click resets that lock. Scroll to nudge. Shift-drag paints the first pad's value across the others. |
| Cmd-click | Step on or off, on any page. |
| Alt-click | The pattern ends here (sets LENGTH). |

Locks that aren't plain show on every page: a coral dot for CHANCE under 100, a small x for RATCHET, a coral edge for TIE.
The coral bracket marks where the pattern ends. The lit pad is the playhead; the lit column and row numbers say where it is.

## Patterns

| Control | What it does |
|---|---|
| 1 to 8 | Eight patterns. Click to edit; while playing, a new one takes over on the next bar line. Automatable as PATTERN. |
| LENGTH | 1 to 128 steps, per pattern. Drag or scroll; double-click for 16. |
| COPY, PASTE, CLEAR, SAVE, LOAD | Per pattern. CLEAR wants two clicks. Files are `.grid`, in Music > THE GRID Patterns by default. |

## Key

| Control | What it does |
|---|---|
| KEY, SCALE, ROOT | Every note is quantised to SCALE in KEY, from the ROOT octave up four octaves. Fifteen scales: the seven modes, harmonic and melodic minor, Phrygian dominant, the pentatonics, blues, whole tone and chromatic. Note names use Logic's convention (middle C is C3). |

## Play

| Control | What it does |
|---|---|
| RATE | One step is 1/1 to 1/32, with triplets and dotted values. |
| DIR | FWD, BWD, PEND (bounce), RAND (anywhere), BROWN (a random walk: mostly forward, sometimes back, now and then a stumble or a leap). |
| SKIP | FWD, BWD and PEND sometimes jump a step. 0 to 100 %. |
| SWING | 50 % is straight; up to 75 % pushes every second step late. |
| RESET | NEVER, or start again from step 1 every 1, 2, 4 or 8 bars. |
| SEED | FREE: RAND, BROWN, SKIP and CHANCE roll new dice every pass. LOCKED: the same every time the song passes the same point, so a bounce matches what you heard. |

## Output

| Control | What it does |
|---|---|
| CHANNEL | MIDI channel out. |
| THRU | Lit: notes you play also pass to the instrument. Everything that isn't a note always passes. |
| TRANSPOSE | Lit: notes you play shift the pattern, relative to C3. |
| RUN | Lit: THE GRID runs whenever Logic plays. |

## Local tools

Configure with `-DTHEGRID_BUILD_TESTS=ON` to also build:

- `THEGRID_Test`: the offline engine checks (CI runs this).
- `THEGRID_Tool midi out.mid [starter|brown|long]`: 8 bars at 110 BPM through the full plugin, as a MIDI file you can drag into Logic.
- `THEGRID_Tool snapshot out.png [step|note|vel|gate|chance|ratchet|tie|boot]`: renders the editor.

Inspired by the way grid sequencers like the Squarp Hapax work; THE GRID is its own design.
The terminal font is Share Tech Mono, under the SIL Open Font Licence (`Resources/ShareTechMono-OFL.txt`).
