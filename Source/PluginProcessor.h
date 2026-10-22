#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

namespace ParamIDs
{
    inline constexpr const char* key       = "key";
    inline constexpr const char* scale     = "scale";
    inline constexpr const char* octave    = "octave";
    inline constexpr const char* rate      = "rate";
    inline constexpr const char* dir       = "dir";
    inline constexpr const char* skip      = "skip";
    inline constexpr const char* swing     = "swing";
    inline constexpr const char* reset     = "reset";
    inline constexpr const char* seed      = "seed";
    inline constexpr const char* transpose = "transpose";
    inline constexpr const char* thru      = "thru";
    inline constexpr const char* channel   = "channel";
    inline constexpr const char* run       = "run";
    inline constexpr const char* pattern   = "pattern";
}

class GridProcessor : public juce::AudioProcessor
{
public:
    GridProcessor();
    ~GridProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "THE GRID"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "THE GRID"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    thegrid::Engine engine;     // the editor reads its telemetry
    thegrid::Pattern pattern;   // the editor edits it

    // The pattern the editor shows and edits: the PATTERN parameter.
    int editPattern() const;
    void setEditPattern (int);
    void patternEdited();

    thegrid::Params currentParams() const;

    juce::ValueTree patternToTree (int p) const;
    bool patternFromTree (int p, const juce::ValueTree&);
    static constexpr const char* kFileExtension = ".grid";

    bool booted = false;

private:
    std::atomic<float>* pKey = nullptr;
    std::atomic<float>* pScale = nullptr;
    std::atomic<float>* pOctave = nullptr;
    std::atomic<float>* pRate = nullptr;
    std::atomic<float>* pDir = nullptr;
    std::atomic<float>* pSkip = nullptr;
    std::atomic<float>* pSwing = nullptr;
    std::atomic<float>* pReset = nullptr;
    std::atomic<float>* pSeed = nullptr;
    std::atomic<float>* pTranspose = nullptr;
    std::atomic<float>* pThru = nullptr;
    std::atomic<float>* pChannel = nullptr;
    std::atomic<float>* pRun = nullptr;
    std::atomic<float>* pPattern = nullptr;

    std::vector<thegrid::NoteIn> notesIn;
    std::array<thegrid::NoteOut, 1024> notesOut {};
    juce::MidiBuffer out;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridProcessor)
};
