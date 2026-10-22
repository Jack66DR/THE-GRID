// THEGRID_Tool
//   midi <out.mid> [starter|brown|busy]             runs 8 bars at 110 BPM through the full plugin, writes what comes out as a MIDI file
//   snapshot <out.png> [step|note|vel|gate|chance|ratchet|tie|boot]   renders the editor to a PNG for a layout check

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

struct Transport : juce::AudioPlayHead
{
    double bpm = 110.0, sampleRate = 48000.0;
    int64_t sample = 0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setIsPlaying (true);
        p.setTimeInSamples (sample);
        p.setPpqPosition ((double) sample / sampleRate * bpm / 60.0);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

static void set (GridProcessor& proc, const juce::String& id, float v)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

// a busy pattern with every kind of lock on it, chords, and lanes of different lengths, for looking at
static void showcase (thegrid::Pattern& pat)
{
    using namespace thegrid;
    pat.starter (0);
    // lane 2: a counter-line, 12 long
    const int line[] = { 17, 19, 22, 24, 22, 19 };
    for (int s = 0; s < 12; s += 2)
    {
        const int c = Pattern::cell (2, s);
        pat.set (lkOn, 0, c, 1);
        pat.set (lkNote, 0, c, line[s / 2]);
        pat.set (lkVel, 0, c, 70 + s * 4);
        pat.set (lkGate, 0, c, 30 + s * 5);
    }
    pat.setLength (0, 2, 12);
    // lane 6: a fifth under the bass, every beat, with a maybe and a ratchet
    for (int s = 0; s < 16; s += 4) pat.set (lkOn, 0, Pattern::cell (6, s), 1);
    pat.set (lkChance, 0, Pattern::cell (6, 8), 40);
    pat.set (lkRatchet, 0, Pattern::cell (6, 12), 2);
    // lane 3: five long, muted
    for (int s = 0; s < 5; ++s) pat.set (lkOn, 0, Pattern::cell (3, s), s % 2 == 0 ? 1 : 0);
    pat.setLength (0, 3, 5);
    pat.setMute (0, 3, true);
    ++pat.version;
}

static int midi (const juce::File& out, const juce::String& look)
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;
    GridProcessor proc;
    Transport tr;
    proc.setPlayHead (&tr);
    proc.setPlayConfigDetails (0, 0, sr, block);
    if (look == "brown") { set (proc, ParamIDs::dir, (float) thegrid::dirBrown); set (proc, ParamIDs::seed, 1.0f); }
    if (look == "busy") showcase (proc.pattern);
    proc.prepareToPlay (sr, block);

    juce::MidiMessageSequence seq;
    juce::AudioBuffer<float> buf (0, block);
    juce::MidiBuffer mb;
    const int total = (int) (8 * 4 * 60.0 / tr.bpm * sr);
    int ons = 0;
    for (int s = 0; s < total; s += block)
    {
        tr.sample = s;
        mb.clear();
        buf.setSize (0, block);
        proc.processBlock (buf, mb);
        for (const auto meta : mb)
        {
            auto m = meta.getMessage();
            const double ticks = (double) (s + meta.samplePosition) / sr * tr.bpm / 60.0 * 960.0;
            m.setTimeStamp (ticks);
            seq.addEvent (m);
            ons += m.isNoteOn() ? 1 : 0;
        }
    }
    seq.updateMatchedPairs();
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (960);
    juce::MidiMessageSequence tempo;
    tempo.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / tr.bpm)));
    mf.addTrack (tempo);
    mf.addTrack (seq);
    out.deleteFile();
    juce::FileOutputStream os (out);
    mf.writeTo (os);
    std::printf ("%s: 8 bars, %d notes, wrote %s\n", look.toRawUTF8(), ons, out.getFullPathName().toRawUTF8());
    return 0;
}

static int snapshot (const juce::File& out, const juce::String& look)
{
    juce::ScopedJuceInitialiser_GUI gui;
    GridProcessor proc;
    Transport tr;
    proc.setPlayHead (&tr);
    proc.setPlayConfigDetails (0, 0, 48000.0, 512);
    showcase (proc.pattern);
    proc.prepareToPlay (48000.0, 512);

    proc.booted = true;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* editor = dynamic_cast<GridEditor*> (ed.get());
    const juce::StringArray pages { "step", "note", "vel", "gate", "chance", "ratchet", "tie" };
    if (editor != nullptr && pages.contains (look))
        editor->setPage ((thegrid::ui::Page) pages.indexOf (look));
    ed->setSize (GridEditor::kBaseW, GridEditor::kBaseH);

    juce::AudioBuffer<float> buf (0, 512);
    juce::MidiBuffer mb;
    const int blocks = (int) (6.3 * 60.0 / tr.bpm * 48000.0) / 512;
    for (int b = 0; b < blocks; ++b)
    {
        tr.sample = (int64_t) b * 512;
        mb.clear();
        proc.processBlock (buf, mb);
        if (editor != nullptr && look == "boot" && b == blocks - 50) editor->boot();
        if (editor != nullptr && b % 3 == 0) editor->pump();
    }
    if (editor != nullptr) editor->pump();

    auto img = ed->createComponentSnapshot (ed->getLocalBounds());
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    ed.reset();
    return 0;
}

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::printf ("usage: THEGRID_Tool midi <out.mid> [starter|brown|busy]\n"
                     "       THEGRID_Tool snapshot <out.png> [step|note|vel|gate|chance|ratchet|tie|boot]\n");
        return 2;
    }
    const juce::String mode (argv[1]);
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[2])));
    if (mode == "midi")
        return midi (out, argc > 3 ? juce::String (argv[3]) : juce::String ("starter"));
    if (mode == "snapshot")
        return snapshot (out, argc > 3 ? juce::String (argv[3]) : juce::String ("step"));
    return 2;
}
