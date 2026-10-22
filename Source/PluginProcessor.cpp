#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace thegrid;

juce::AudioProcessorValueTreeState::ParameterLayout GridProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto names = [] (const char* const* arr, int n) { StringArray s; for (int i = 0; i < n; ++i) s.add (arr[i]); return s; };
    auto pct = AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return String (v) + " %"; });

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::key, 1 }, "Key", names (kNoteName, 12), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::scale, 1 }, "Scale", names (kScaleName, kNumScales), kDefaultScale));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::octave, 1 }, "Root", 0, 6, 2, AudioParameterIntAttributes()
                .withStringFromValueFunction ([] (int v, int) { return "C" + String (v); })));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::rate, 1 }, "Rate", names (kRateName, kNumRates), 4));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::dir, 1 }, "Direction", names (kDirName, kNumDirs), dirFwd));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::skip, 1 }, "Skip", 0, 100, 0, pct));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::swing, 1 }, "Swing", 50, 75, 50, pct));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::reset, 1 }, "Reset", names (kResetName, kNumResets), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::seed, 1 }, "Seed", names (kSeedName, 2), 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::transpose, 1 }, "Transpose", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::thru, 1 }, "Thru", true));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::channel, 1 }, "Channel", 1, 16, 1));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::run, 1 }, "Run", true));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::pattern, 1 }, "Pattern", 1, kPatterns, 1));
    return l;
}

GridProcessor::GridProcessor()
    : AudioProcessor (BusesProperties()),   // a MIDI effect: no audio in or out
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pKey = apvts.getRawParameterValue (ParamIDs::key);
    pScale = apvts.getRawParameterValue (ParamIDs::scale);
    pOctave = apvts.getRawParameterValue (ParamIDs::octave);
    pRate = apvts.getRawParameterValue (ParamIDs::rate);
    pDir = apvts.getRawParameterValue (ParamIDs::dir);
    pSkip = apvts.getRawParameterValue (ParamIDs::skip);
    pSwing = apvts.getRawParameterValue (ParamIDs::swing);
    pReset = apvts.getRawParameterValue (ParamIDs::reset);
    pSeed = apvts.getRawParameterValue (ParamIDs::seed);
    pTranspose = apvts.getRawParameterValue (ParamIDs::transpose);
    pThru = apvts.getRawParameterValue (ParamIDs::thru);
    pChannel = apvts.getRawParameterValue (ParamIDs::channel);
    pRun = apvts.getRawParameterValue (ParamIDs::run);
    pPattern = apvts.getRawParameterValue (ParamIDs::pattern);
    notesIn.reserve (256);
    out.ensureSize (8192);
    engine.setInstanceSalt ((uint64_t) juce::Random::getSystemRandom().nextInt64());
}

void GridProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

Params GridProcessor::currentParams() const
{
    auto i = [] (const std::atomic<float>* a) { return juce::roundToInt (a->load()); };
    Params p;
    p.key = i (pKey);
    p.scale = i (pScale);
    p.octave = i (pOctave);
    p.rate = i (pRate);
    p.dir = i (pDir);
    p.skip = i (pSkip);
    p.swing = i (pSwing);
    p.reset = i (pReset);
    p.seed = i (pSeed);
    p.transpose = pTranspose->load() > 0.5f;
    p.thru = pThru->load() > 0.5f;
    p.run = pRun->load() > 0.5f;
    p.slot = juce::jlimit (0, kPatterns - 1, i (pPattern) - 1);
    return p;
}

void GridProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n <= 0)
        return;

    const auto p = currentParams();

    Clock clock;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) clock.bpm = *bpm;
            const auto ppq = pos->getPpqPosition();
            clock.playing = pos->getIsPlaying() && ppq.hasValue();
            if (ppq.hasValue()) clock.ppq = *ppq;
            if (auto ts = pos->getTimeSignature())
                if (ts->numerator > 0 && ts->denominator > 0)
                    clock.barPpq = 4.0 * (double) ts->numerator / (double) ts->denominator;
            if (pos->getIsLooping())
                if (auto lp = pos->getLoopPoints())
                {
                    clock.looping = true;
                    clock.loopStart = lp->ppqStart;
                    clock.loopEnd = lp->ppqEnd;
                }
        }

    // what comes in: notes can transpose (and pass through on THRU), everything else passes straight through
    out.clear();
    notesIn.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int off = juce::jlimit (0, n - 1, meta.samplePosition);
        if (m.isNoteOn())
        {
            if (notesIn.size() < notesIn.capacity())
                notesIn.push_back ({ off, m.getNoteNumber(), (int) m.getVelocity(), true });
            if (p.thru) out.addEvent (m, off);
        }
        else if (m.isNoteOff())
        {
            if (p.thru) out.addEvent (m, off);
        }
        else
            out.addEvent (m, off);
    }

    const int ch = juce::jlimit (1, 16, juce::roundToInt (pChannel->load()));
    const int made = engine.process (n, p, clock, pattern, notesIn.data(), (int) notesIn.size(),
                                     notesOut.data(), (int) notesOut.size());
    for (int i = 0; i < made; ++i)
    {
        const auto& e = notesOut[(size_t) i];
        out.addEvent (e.on ? juce::MidiMessage::noteOn (ch, e.note, (juce::uint8) e.velocity)
                           : juce::MidiMessage::noteOff (ch, e.note), juce::jlimit (0, n - 1, e.offset));
    }
    midi.swapWith (out);
}

//==============================================================================
int GridProcessor::editPattern() const
{
    return juce::jlimit (0, kPatterns - 1, juce::roundToInt (pPattern->load()) - 1);
}

void GridProcessor::setEditPattern (int s)
{
    if (auto* prm = apvts.getParameter (ParamIDs::pattern))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (juce::jlimit (0, kPatterns - 1, s) + 1)));
        prm->endChangeGesture();
    }
}

void GridProcessor::patternEdited()
{
    ++pattern.version;
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

juce::ValueTree GridProcessor::patternToTree (int s) const
{
    // one property per lock, 128 values each, comma separated
    juce::ValueTree t ("GRIDPATTERN");
    t.setProperty ("format", 1, nullptr);
    t.setProperty ("length", pattern.length (s), nullptr);
    for (int l = 0; l < kNumLocks; ++l)
    {
        juce::String v;
        v.preallocateBytes (kSteps * 4);
        for (int st = 0; st < kSteps; ++st)
        {
            if (st > 0) v << ",";
            v << pattern.get ((Lock) l, s, st);
        }
        t.setProperty (juce::String (kLockName[l]).toLowerCase(), v, nullptr);
    }
    return t;
}

bool GridProcessor::patternFromTree (int s, const juce::ValueTree& t)
{
    if (! t.hasType ("GRIDPATTERN"))
        return false;
    for (int l = 0; l < kNumLocks; ++l)
    {
        const auto key = juce::String (kLockName[l]).toLowerCase();
        const auto vals = juce::StringArray::fromTokens (t.getProperty (key).toString(), ",", {});
        for (int st = 0; st < kSteps; ++st)
            pattern.set ((Lock) l, s, st, st < vals.size() ? vals[st].getIntValue() : kLockDef[l]);
    }
    pattern.setLength (s, (int) t.getProperty ("length", 16));
    patternEdited();
    return true;
}

void GridProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree bank ("BANK");
    for (int s = 0; s < kPatterns; ++s)
        bank.appendChild (patternToTree (s), nullptr);
    state.appendChild (bank, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GridProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto state = juce::ValueTree::fromXml (*xml);
    auto bank = state.getChildWithName ("BANK");
    if (bank.isValid())
    {
        for (int s = 0; s < kPatterns && s < bank.getNumChildren(); ++s)
            patternFromTree (s, bank.getChild (s));
        state.removeChild (bank, nullptr);
    }
    apvts.replaceState (state);
}

juce::AudioProcessorEditor* GridProcessor::createEditor()
{
    return new GridEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GridProcessor();
}
