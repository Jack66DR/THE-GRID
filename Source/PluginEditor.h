#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace thegrid::ui
{
    // Ice-blue phosphor for the machine and the steps, coral for the locks, cream for anything you have to read.
    namespace col
    {
        const juce::Colour tube  { 0xff04070a };
        const juce::Colour ice   { 0xff7fdcff };
        const juce::Colour coral { 0xffff7a5c };
        const juce::Colour cream { 0xfff0ede6 };
    }

    inline constexpr float kFontScale = 1.25f;
    juce::Font mono (float height);
    float textWidth (const juce::Font&, const juce::String&);
    juce::String noteName (int midi);   // Logic naming: 60 is C3

    struct Skin
    {
        double t = 0.0;
        bool blink() const { return std::fmod (t, 1.0) < 0.55; }
    };

    void glowText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Justification,
                   const juce::Font&, juce::Colour, float alpha = 1.0f);
    void glowStroke (juce::Graphics&, const juce::Path&, float width, juce::Colour, float alpha = 1.0f);
    void glowBox (juce::Graphics&, juce::Rectangle<float>, float width, juce::Colour, float alpha = 1.0f);
    void litBlock (juce::Graphics&, juce::Rectangle<float>, juce::Colour, float alpha = 1.0f);

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        explicit LookAndFeel (const Skin&);
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
        void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
        juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;

        const Skin& skin;
    };

    // A terminal field: [ LABEL      VALUE ].
    // Choices and switches: click for the next, right-click for the one before. Numbers: drag up or down, or scroll.
    // Double-click for the default.
    class Field : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        Field (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label, juce::Colour);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        void setDimmed (bool d) { if (d != dimmed) { dimmed = d; repaint(); } }

    private:
        void nudge (int steps);
        juce::RangedAudioParameter* param = nullptr;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        juce::String label;
        juce::Colour colour;
        float value = 0.0f, dragStartValue = 0.0f;
        bool discrete = true, dragging = false, dimmed = false;
    };

    // The pattern's length, which lives with the pattern rather than as a parameter.
    class LengthField : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit LengthField (GridProcessor&);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        void tick();

    private:
        void setLen (int);
        GridProcessor& proc;
        int shown = -1, dragStart = 16;
    };

    class Display : public juce::Component
    {
    public:
        Display (GridProcessor& p, const Skin& s) : proc (p), skin (s) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        void tick();

    private:
        GridProcessor& proc;
        const Skin& skin;
        int bar = 1, beat = 1, slot = 0, want = 0, note = -1, trans = 0, stepNow = -1, len = 16;
        uint32_t notes = 0;
        float flash = 0.0f;
        bool playing = false, lastBlink = false;
    };

    class PatternBar : public juce::Component
    {
    public:
        PatternBar (GridProcessor& p, const Skin& s) : proc (p), skin (s) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void tick();

    private:
        juce::Rectangle<float> key (int i) const;
        GridProcessor& proc;
        const Skin& skin;
        int playing = -1, editing = -1;
        bool lastBlink = false;
    };

    enum class Page { step, note, vel, gate, chance, ratchet, tie };

    // The 8 x 16 pads: 128 steps, a row of sixteen per line.
    class PadGrid : public juce::Component, public juce::TooltipClient
    {
    public:
        PadGrid (GridProcessor&, const Skin&);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        juce::String getTooltip() override;
        void tick();

        Page page = Page::step;

        // lane numbers (click to mute) down the left, the pads, then each lane's LENGTH down the right
        static constexpr float kPad = 58.0f, kGap = 6.0f, kGroup = 6.0f, kLabelW = 48.0f, kHeadH = 24.0f, kLaneGap = 14.0f, kLaneW = 64.0f;
        static constexpr int kW = 1162, kH = 530;
        static juce::Rectangle<float> cell (int s)
        {
            const int c = s % thegrid::kCols, r = s / thegrid::kCols;
            return { kLabelW + (float) c * (kPad + kGap) + (float) (c / 4) * kGroup, kHeadH + (float) r * (kPad + kGap), kPad, kPad };
        }
        static juce::Rectangle<float> laneLabel (int l) { const auto r = cell (l * thegrid::kCols); return { 0.0f, r.getY(), kLabelW - 8.0f, r.getHeight() }; }
        static juce::Rectangle<float> laneLength (int l)
        {
            const auto r = cell (l * thegrid::kCols + thegrid::kCols - 1);
            return { r.getRight() + kLaneGap, r.getY(), kLaneW, r.getHeight() };
        }

    private:
        int cellAt (juce::Point<float>) const;
        thegrid::Lock lock() const;
        void setLock (int s, int value);
        void nudge (int s, int units);
        void paintCell (int s);

        GridProcessor& proc;
        const Skin& skin;
        void setLaneLength (int lane, int n);
        int laneAt (juce::Point<float>, bool lengthStrip) const;

        std::array<int, thegrid::kLanes> heads { -1, -1, -1, -1, -1, -1, -1, -1 };
        int count = -1, hover = -1, dragCell = -1, dragStartValue = 0, paintValue = 0, laneDrag = -1, laneDragStart = 16;
        juce::Point<float> mousePos;
        float dragStartY = 0.0f;
        bool painting = false, paintOn = true;
        uint32_t lastVersion = 0, lastNotes = 0, lastSteps = 0;
        float flash = 0.0f;
        thegrid::Params params;
    };

    class Title : public juce::Component
    {
    public:
        explicit Title (const Skin& s) : skin (s) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        void tick();

    private:
        const Skin& skin;
        bool lastBlink = false;
    };

    class Glass : public juce::Component
    {
    public:
        explicit Glass (const Skin&);
        void paint (juce::Graphics&) override;
        void resized() override;
        void boot (double now) { bootStart = now; }
        bool booting() const { return skin.t - bootStart < kBootSecs; }
        static constexpr double kBootSecs = 3.1;

    private:
        const Skin& skin;
        juce::Image lines;
        double bootStart = -100.0;
    };
}

class GridEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GridEditor (GridProcessor&);
    ~GridEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void pump (double dt = 1.0 / 30.0) { advance (dt); }
    void boot() { glass.boot (skin.t); }
    void setPage (thegrid::ui::Page);

    static constexpr int kBaseW = 1240, kBaseH = 920;

private:
    class Face : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    void timerCallback() override { advance (1.0 / 30.0); }
    void advance (double dt);
    void savePattern();
    void loadPattern();

    GridProcessor& proc;
    thegrid::ui::Skin skin;
    thegrid::ui::LookAndFeel lnf { skin };
    Face face;
    thegrid::ui::Title title { skin };
    thegrid::ui::Display display { proc, skin };
    thegrid::ui::PatternBar patterns { proc, skin };
    thegrid::ui::PadGrid grid { proc, skin };
    thegrid::ui::LengthField length { proc };
    thegrid::ui::Glass glass { skin };
    std::vector<std::unique_ptr<thegrid::ui::Field>> fields;
    thegrid::ui::Field* skipField = nullptr;
    std::array<juce::TextButton, 7> pages;
    juce::TextButton copyButton { "COPY" }, pasteButton { "PASTE" }, clearButton { "CLEAR" }, saveButton { "SAVE" }, loadButton { "LOAD" };
    juce::TextButton runButton { "RUN" }, transposeButton { "TRANSPOSE" }, thruButton { "THRU" };
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };
    double clearArmedUntil = 0.0;
    bool lastBlink = false, wasBooting = false;

    static juce::ValueTree clipboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridEditor)
};
