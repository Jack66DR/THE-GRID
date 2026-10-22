#include "PluginEditor.h"
#include "BinaryData.h"

using namespace thegrid::ui;

juce::ValueTree GridEditor::clipboard;

//==============================================================================
juce::Font thegrid::ui::mono (float height)
{
    static juce::Typeface::Ptr stm = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf,
                                                                             BinaryData::ShareTechMonoRegular_ttfSize);
    return juce::Font (juce::FontOptions (stm).withHeight (height * kFontScale));
}

float thegrid::ui::textWidth (const juce::Font& f, const juce::String& s)
{
    return juce::GlyphArrangement::getStringWidth (f, s);
}

juce::String thegrid::ui::noteName (int midi)
{
    if (midi < 0) return "--";
    return juce::String (thegrid::kNoteName[midi % 12]) + juce::String (midi / 12 - 2);
}

void thegrid::ui::glowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Justification j,
                             const juce::Font& f, juce::Colour c, float alpha)
{
    g.setFont (f);
    static const float offs[8][2] = { { -1.6f, 0.0f }, { 1.6f, 0.0f }, { 0.0f, -1.6f }, { 0.0f, 1.6f },
                                      { -1.1f, -1.1f }, { 1.1f, 1.1f }, { -1.1f, 1.1f }, { 1.1f, -1.1f } };
    g.setColour (c.withAlpha (0.10f * alpha));
    for (auto& o : offs)
        g.drawText (text, r.translated (o[0], o[1]), j, false);
    g.setColour (c.withAlpha (alpha));
    g.drawText (text, r, j, false);
}

void thegrid::ui::glowStroke (juce::Graphics& g, const juce::Path& p, float width, juce::Colour c, float alpha)
{
    const auto js = juce::PathStrokeType::curved;
    g.setColour (c.withAlpha (0.07f * alpha));
    g.strokePath (p, juce::PathStrokeType (width * 5.0f, js, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (0.16f * alpha));
    g.strokePath (p, juce::PathStrokeType (width * 2.6f, js, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (alpha));
    g.strokePath (p, juce::PathStrokeType (width, js, juce::PathStrokeType::rounded));
}

void thegrid::ui::glowBox (juce::Graphics& g, juce::Rectangle<float> r, float width, juce::Colour c, float alpha)
{
    juce::Path p;
    p.addRectangle (r);
    glowStroke (g, p, width, c, alpha);
}

void thegrid::ui::litBlock (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float alpha)
{
    g.setColour (c.withAlpha (0.08f * alpha));
    g.fillRect (r.expanded (5.0f));
    g.setColour (c.withAlpha (0.18f * alpha));
    g.fillRect (r.expanded (2.0f));
    g.setColour (c.withAlpha (alpha));
    g.fillRect (r);
}

namespace
{
    double nowSecs() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

    juce::Colour buttonColour (juce::Button& b)
    {
        if (b.getProperties().contains ("colour"))
            return juce::Colour::fromString (b.getProperties()["colour"].toString());
        return col::ice;
    }
}

//==============================================================================
LookAndFeel::LookAndFeel (const Skin& s) : skin (s)
{
    setColour (juce::ResizableWindow::backgroundColourId, col::tube);
    setColour (juce::TooltipWindow::textColourId, col::cream);
    setDefaultSansSerifTypeface (mono (14.0f).getTypefacePtr());
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.5f);
    const auto c = buttonColour (b);
    if (b.getToggleState() || down)
        litBlock (g, r, c);
    else
    {
        if (over)
        {
            g.setColour (c.withAlpha (0.12f));
            g.fillRect (r);
        }
        glowBox (g, r, 1.3f, c, 0.85f);
    }
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const bool on = b.getToggleState() || down;
    juce::String text = b.getButtonText();
    if (b.getToggleState() && (bool) b.getProperties().getWithDefault ("lamp", false))
        text = (skin.blink() ? juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xa0 ")) : "  ") + text;
    const auto f = mono (14.0f).withExtraKerningFactor (0.1f);
    if (on)
    {
        g.setColour (col::tube);
        g.setFont (f);
        g.drawText (text, b.getLocalBounds(), juce::Justification::centred);
    }
    else
        glowText (g, text, b.getLocalBounds().toFloat(), juce::Justification::centred, f, buttonColour (b));
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const int w = juce::roundToInt (textWidth (mono (13.0f), tip)) + 24, h = 28;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 8, w, h)
        .constrainedWithin (parent);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    const auto r = juce::Rectangle<float> ((float) w, (float) h);
    g.fillAll (col::tube);
    glowBox (g, r.reduced (1.5f), 1.2f, col::cream, 0.8f);
    glowText (g, text, r.reduced (10.0f, 0.0f), juce::Justification::centredLeft, mono (13.0f), col::cream);
}

//==============================================================================
Field::Field (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& l, juce::Colour c)
    : label (l), colour (c)
{
    param = s.getParameter (paramId);
    setTitle (l);
    if (param != nullptr)
    {
        discrete = dynamic_cast<juce::AudioParameterChoice*> (param) != nullptr || dynamic_cast<juce::AudioParameterBool*> (param) != nullptr;
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { value = v; repaint(); });
        attachment->sendInitialUpdate();
    }
    setTooltip (discrete ? "CLICK FOR THE NEXT, RIGHT-CLICK FOR THE ONE BEFORE" : "DRAG UP OR DOWN, OR SCROLL. DOUBLE-CLICK TO RESET");
}

void Field::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (2.5f);
    const float a = dimmed ? 0.35f : 1.0f;
    if (isMouseOver() && ! dimmed)
    {
        g.setColour (colour.withAlpha (0.10f));
        g.fillRect (r);
    }
    glowBox (g, r, 1.2f, colour, 0.75f * a);
    glowText (g, label, r.reduced (10.0f, 0.0f), juce::Justification::centredLeft,
              mono (12.0f).withExtraKerningFactor (0.12f), colour, 0.7f * a);
    const auto txt = param != nullptr ? param->getCurrentValueAsText().toUpperCase() : juce::String();
    glowText (g, txt, r.reduced (10.0f, 0.0f), juce::Justification::centredRight,
              mono (15.0f).withExtraKerningFactor (0.06f), colour, a);
}

void Field::nudge (int steps)
{
    if (param == nullptr || attachment == nullptr) return;
    const auto range = param->getNormalisableRange();
    const float span = range.end - range.start;
    const float inc = discrete || range.interval > 0.0f ? std::max (1.0f, range.interval) : span / 100.0f;
    float v = value + inc * (float) steps;
    if (discrete && dynamic_cast<juce::AudioParameterChoice*> (param) != nullptr)
    {
        const int n = juce::roundToInt (span) + 1;
        v = (float) ((juce::roundToInt (value) + steps % n + n) % n);   // choices wrap round
    }
    attachment->setValueAsCompleteGesture (juce::jlimit (range.start, range.end, v));
}

void Field::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    dragStartValue = value;
    if (discrete)
    {
        if (auto* b = dynamic_cast<juce::AudioParameterBool*> (param))
            attachment->setValueAsCompleteGesture (b->get() ? 0.0f : 1.0f);
        else
            nudge (e.mods.isPopupMenu() ? -1 : 1);
    }
}

void Field::mouseDrag (const juce::MouseEvent& e)
{
    if (discrete || param == nullptr) return;
    if (! dragging) { attachment->beginGesture(); dragging = true; }
    const auto range = param->getNormalisableRange();
    const float span = range.end - range.start;
    const float perPx = range.interval >= 1.0f ? 1.0f / 12.0f : span / 200.0f;   // whole numbers: one per 12 px
    float v = dragStartValue + (float) -e.getDistanceFromDragStartY() * perPx;
    if (range.interval >= 1.0f) v = std::round (v);
    attachment->setValueAsPartOfGesture (juce::jlimit (range.start, range.end, v));
}

void Field::mouseUp (const juce::MouseEvent&)
{
    if (dragging) attachment->endGesture();
    dragging = false;
}

void Field::mouseDoubleClick (const juce::MouseEvent&)
{
    if (param != nullptr && ! discrete)
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue()));
}

void Field::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (! discrete && std::abs (w.deltaY) > 0.0f)
        nudge (w.deltaY > 0.0f ? 1 : -1);
}


//==============================================================================
LengthField::LengthField (GridProcessor& p) : proc (p)
{
    setTooltip ("PATTERN LENGTH: DRAG UP OR DOWN, OR SCROLL. DOUBLE-CLICK FOR 16. ALT-CLICK A PAD SETS IT TOO");
}

void LengthField::tick()
{
    const int l = proc.pattern.length (proc.editPattern());
    if (l != shown) { shown = l; repaint(); }
}

void LengthField::setLen (int n)
{
    const int p = proc.editPattern();
    n = juce::jlimit (1, thegrid::kSteps, n);
    if (n != proc.pattern.length (p))
    {
        proc.pattern.setLength (p, n);
        proc.patternEdited();
    }
    tick();
}

void LengthField::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (2.5f);
    if (isMouseOver())
    {
        g.setColour (col::coral.withAlpha (0.10f));
        g.fillRect (r);
    }
    glowBox (g, r, 1.2f, col::coral, 0.75f);
    glowText (g, "LENGTH", r.reduced (10.0f, 0.0f), juce::Justification::centredLeft,
              mono (12.0f).withExtraKerningFactor (0.12f), col::coral, 0.7f);
    const int l = proc.pattern.length (proc.editPattern());
    const juce::String txt = juce::String (l) + " STEPS" + (l % 16 == 0 ? "  (" + juce::String (l / 16) + (l == 16 ? " ROW)" : " ROWS)") : juce::String());
    glowText (g, txt, r.reduced (10.0f, 0.0f), juce::Justification::centredRight,
              mono (15.0f).withExtraKerningFactor (0.06f), col::coral);
}

void LengthField::mouseDown (const juce::MouseEvent&) { dragStart = proc.pattern.length (proc.editPattern()); }
void LengthField::mouseDrag (const juce::MouseEvent& e) { setLen (dragStart + juce::roundToInt ((float) -e.getDistanceFromDragStartY() / 6.0f)); }
void LengthField::mouseDoubleClick (const juce::MouseEvent&) { setLen (16); }
void LengthField::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (std::abs (w.deltaY) > 0.0f)
        setLen (proc.pattern.length (proc.editPattern()) + (w.deltaY > 0.0f ? 1 : -1));
}

//==============================================================================
void Display::tick()
{
    auto& e = proc.engine;
    const int nb = e.uiBar.load(), nbeat = e.uiBeat.load(), ns = e.uiSlot.load(), nw = proc.editPattern(), nn = e.uiNote.load(),
              nt = e.uiTranspose.load(), nst = e.uiStep.load(), nl = proc.pattern.length (e.uiSlot.load());
    const uint32_t nc = e.uiNotes.load();
    const bool np = e.uiPlaying.load();
    if (nc != notes) flash = 1.0f;
    const bool b = skin.blink();
    const bool changed = nb != bar || nbeat != beat || ns != slot || nw != want || np != playing || nn != note || nt != trans
                         || nst != stepNow || nl != len || nc != notes || flash > 0.01f || (b != lastBlink && np && nw != ns);
    bar = nb; beat = nbeat; slot = ns; want = nw; playing = np; note = nn; notes = nc; trans = nt; stepNow = nst; len = nl;
    lastBlink = b;
    flash *= 0.7f;
    if (changed) repaint();
}

void Display::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (3.0f);
    glowBox (g, r, 1.3f, col::ice, 0.8f);
    auto in = r.reduced (16.0f, 6.0f);
    const auto small = mono (12.0f).withExtraKerningFactor (0.15f);

    auto pt = in.removeFromLeft (104.0f);
    glowText (g, "PATTERN", pt.removeFromTop (16.0f), juce::Justification::centredLeft, small, col::ice, 0.7f);
    glowText (g, juce::String (slot + 1), pt, juce::Justification::centredLeft, mono (34.0f), col::ice);
    if (playing && want != slot && skin.blink())
        glowText (g, ">" + juce::String (want + 1), pt.withTrimmedLeft (30.0f), juce::Justification::centredLeft, mono (16.0f), col::cream);

    auto st = in.removeFromLeft (150.0f);
    glowText (g, "STEP", st.removeFromTop (16.0f), juce::Justification::centredLeft, small, col::coral, 0.7f);
    glowText (g, playing && stepNow >= 0 ? juce::String (stepNow + 1).paddedLeft ('0', 3) : juce::String ("---"), st,
              juce::Justification::centredLeft, mono (34.0f), col::coral);
    glowText (g, "/" + juce::String (len), st.withTrimmedLeft (78.0f).withTrimmedTop (10.0f), juce::Justification::centredLeft,
              mono (15.0f), col::coral, 0.7f);

    auto nt = in.removeFromLeft (124.0f);
    glowText (g, "NOTE", nt.removeFromTop (16.0f), juce::Justification::centredLeft, small, col::cream, 0.7f);
    glowText (g, noteName (note), nt, juce::Justification::centredLeft, mono (34.0f), col::cream, 0.75f + 0.25f * flash);

    auto top = in.removeFromTop (in.getHeight() * 0.5f);
    glowText (g, "BAR", top.removeFromLeft (58.0f), juce::Justification::centredLeft, small, col::ice, 0.7f);
    glowText (g, playing ? juce::String (bar) + "." + juce::String (beat) : juce::String ("--.-"), top,
              juce::Justification::centredLeft, mono (17.0f), col::ice);
    glowText (g, "TRANS", in.removeFromLeft (58.0f), juce::Justification::centredLeft, small, col::ice, 0.7f);
    glowText (g, (trans > 0 ? "+" : "") + juce::String (trans), in, juce::Justification::centredLeft, mono (17.0f), col::ice);
}

//==============================================================================
juce::Rectangle<float> PatternBar::key (int i) const
{
    const float w = ((float) getWidth() - 4.0f * (thegrid::kPatterns - 1)) / thegrid::kPatterns;
    return juce::Rectangle<float> ((float) i * (w + 4.0f), 0.0f, w, (float) getHeight()).reduced (1.5f);
}

void PatternBar::tick()
{
    const int p = proc.engine.uiPlaying.load() ? proc.engine.uiSlot.load() : -1;
    const int e = proc.editPattern();
    const bool b = skin.blink();
    if (p != playing || e != editing || (b != lastBlink && p >= 0))
    {
        playing = p; editing = e;
        repaint();
    }
    lastBlink = b;
}

void PatternBar::paint (juce::Graphics& g)
{
    for (int i = 0; i < thegrid::kPatterns; ++i)
    {
        const auto r = key (i);
        const auto c = col::ice;
        const auto f = mono (16.0f);
        if (i == editing)
        {
            litBlock (g, r, c);
            g.setColour (col::tube);
            g.setFont (f);
            g.drawText (juce::String (i + 1), r, juce::Justification::centred, false);
        }
        else
        {
            glowBox (g, r, 1.2f, c, 0.7f);
            glowText (g, juce::String (i + 1), r, juce::Justification::centred, f, c, 0.85f);
        }
        if (i == playing && skin.blink())
        {
            g.setColour (i == editing ? col::tube : col::coral);
            g.fillRect (r.getRight() - 9.0f, r.getY() + 4.0f, 5.0f, 5.0f);
        }
    }
}

void PatternBar::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < thegrid::kPatterns; ++i)
        if (key (i).expanded (2.0f).contains (e.position))
        {
            proc.setEditPattern (i);
            tick();
            return;
        }
}

//==============================================================================
PadGrid::PadGrid (GridProcessor& p, const Skin& s) : proc (p), skin (s)
{
    params = proc.currentParams();
    setTitle ("Pads");
}

thegrid::Lock PadGrid::lock() const
{
    switch (page)
    {
        case Page::note:    return thegrid::lkNote;
        case Page::vel:     return thegrid::lkVel;
        case Page::gate:    return thegrid::lkGate;
        case Page::chance:  return thegrid::lkChance;
        case Page::ratchet: return thegrid::lkRatchet;
        case Page::tie:     return thegrid::lkTie;
        case Page::step:    return thegrid::lkOn;
    }
    return thegrid::lkOn;
}

void PadGrid::tick()
{
    bool dirty = false;
    const auto np = proc.currentParams();
    if (np.scale != params.scale || np.key != params.key || np.octave != params.octave || np.slot != params.slot)
        dirty = true;
    params = np;
    const uint32_t v = proc.pattern.version.load();
    if (v != lastVersion) { lastVersion = v; dirty = true; }
    const int st = proc.engine.uiPlaying.load() ? proc.engine.uiStep.load() : -1;
    if (st != stepNow) { stepNow = st; dirty = true; }
    const uint32_t n = proc.engine.uiNotes.load();
    if (n != lastNotes) { lastNotes = n; flash = 1.0f; dirty = true; }
    else if (flash > 0.0f) { flash = flash < 0.04f ? 0.0f : flash * 0.72f; dirty = true; }
    if (dirty) repaint();
}

void PadGrid::paint (juce::Graphics& g)
{
    using namespace thegrid;
    const int s = proc.editPattern();
    const int L = proc.pattern.length (s);
    const bool showHead = stepNow >= 0 && proc.engine.uiSlot.load() == s;
    const int root = 12 * (std::clamp (params.octave, 0, 6) + 2) + std::clamp (params.key, 0, 11);

    // column numbers along the top, row starts down the side; the playhead's column and row lit
    const auto hf = mono (11.0f).withExtraKerningFactor (0.08f);
    for (int c = 0; c < kCols; ++c)
    {
        const auto r = cell (c);
        const auto top = juce::Rectangle<float> (r.getX(), 0.0f, r.getWidth(), kHeadH - 4.0f);
        if (showHead && stepNow % kCols == c)
        {
            litBlock (g, top.reduced (14.0f, 3.0f), col::ice, 0.9f);
            g.setColour (col::tube);
            g.setFont (hf);
            g.drawText (juce::String (c + 1), top, juce::Justification::centred, false);
        }
        else
            glowText (g, juce::String (c + 1), top, juce::Justification::centred, hf, col::ice, c % 4 == 0 ? 0.75f : 0.4f);
    }
    for (int row = 0; row < kRows; ++row)
    {
        const auto r = cell (row * kCols);
        const auto side = juce::Rectangle<float> (0.0f, r.getY(), kLabelW - 8.0f, r.getHeight());
        const bool lit = showHead && stepNow / kCols == row;
        if (lit)
        {
            litBlock (g, side.reduced (2.0f, 16.0f), col::ice, 0.9f);
            g.setColour (col::tube);
            g.setFont (hf);
            g.drawText (juce::String (row * kCols + 1), side, juce::Justification::centred, false);
        }
        else
            glowText (g, juce::String (row * kCols + 1), side, juce::Justification::centred, hf, col::ice, row * kCols < L ? 0.6f : 0.2f);
    }

    const auto valueFont = mono (12.0f);
    for (int st = 0; st < kSteps; ++st)
    {
        const auto r = cell (st);
        const bool inLen = st < L;
        const bool on = proc.pattern.on (s, st);
        const bool here = showHead && st == stepNow;
        const bool over = st == hover;

        if (! inLen)
        {
            glowBox (g, r, 1.0f, col::ice, over ? 0.35f : 0.12f);
            if (on)
            {
                g.setColour (col::ice.withAlpha (0.25f));
                g.fillRect (r.withSizeKeepingCentre (6.0f, 6.0f));
            }
            continue;
        }

        // the pad
        if (on)
        {
            g.setColour (col::ice.withAlpha (over ? 0.24f : 0.15f));
            g.fillRect (r);
            glowBox (g, r, 1.4f, col::ice, 0.95f);
        }
        else
        {
            if (over)
            {
                g.setColour (col::ice.withAlpha (0.08f));
                g.fillRect (r);
            }
            glowBox (g, r, 1.1f, col::ice, over ? 0.75f : 0.38f);
        }
        if (here)
        {
            g.setColour (col::cream.withAlpha (0.06f + 0.26f * flash));
            g.fillRect (r);
            glowBox (g, r.expanded (1.5f), 2.0f, col::cream, 1.0f);
        }

        if (! on)
            continue;

        const int note = root + quantise (proc.pattern.get (lkNote, s, st), params.scale);
        const int vel = proc.pattern.get (lkVel, s, st), gate = proc.pattern.get (lkGate, s, st);
        const int chance = proc.pattern.get (lkChance, s, st), ratchet = proc.pattern.get (lkRatchet, s, st);
        const bool tie = proc.pattern.get (lkTie, s, st) != 0;
        const auto inner = r.reduced (7.0f);

        switch (page)
        {
            case Page::step:
                glowText (g, noteName (note), r, juce::Justification::centred, valueFont, col::cream, 0.85f);
                break;
            case Page::note:
                glowText (g, noteName (note), r, juce::Justification::centred, mono (14.0f), col::cream);
                break;
            case Page::vel:
            {
                const auto band = inner.withTrimmedTop (18.0f);
                const float h = std::max (2.0f, band.getHeight() * (float) vel / 127.0f);
                litBlock (g, band.withTop (band.getBottom() - h).withSizeKeepingCentre (14.0f, h), col::coral, 0.75f);
                glowText (g, juce::String (vel), inner.withHeight (16.0f), juce::Justification::centred, mono (11.0f), col::cream);
                break;
            }
            case Page::gate:
            {
                const auto bar = inner.withTrimmedTop (inner.getHeight() * 0.62f);
                litBlock (g, bar.withWidth (std::max (2.0f, bar.getWidth() * (float) gate / 100.0f)), col::coral, 0.75f);
                glowText (g, juce::String (gate) + "%", inner.withTrimmedBottom (inner.getHeight() * 0.4f),
                          juce::Justification::centred, mono (11.0f), col::cream);
                break;
            }
            case Page::chance:
            {
                const auto bar = inner.withTrimmedTop (inner.getHeight() * 0.62f);
                glowBox (g, bar, 1.0f, col::coral, 0.4f);
                if (chance > 0)
                    litBlock (g, bar.withWidth (bar.getWidth() * (float) chance / 100.0f), col::coral, 0.75f);
                glowText (g, juce::String (chance) + "%", inner.withTrimmedBottom (inner.getHeight() * 0.4f),
                          juce::Justification::centred, mono (11.0f), col::cream);
                break;
            }
            case Page::ratchet:
            {
                const auto band = inner.withTrimmedTop (inner.getHeight() * 0.5f);
                const float w = band.getWidth() / (float) ratchet;
                for (int i = 0; i < ratchet; ++i)
                    litBlock (g, juce::Rectangle<float> (band.getX() + (float) i * w + 1.5f, band.getY(), std::max (2.0f, w - 3.0f), band.getHeight()),
                              col::coral, 0.75f);
                glowText (g, "x" + juce::String (ratchet), inner.withTrimmedBottom (inner.getHeight() * 0.5f),
                          juce::Justification::centred, mono (12.0f), col::cream);
                break;
            }
            case Page::tie:
                glowText (g, noteName (note), r.withTrimmedBottom (18.0f), juce::Justification::centred, valueFont, col::cream, 0.75f);
                if (tie)
                {
                    juce::Path a;
                    const float y = r.getBottom() - 15.0f;
                    a.startNewSubPath (r.getX() + 10.0f, y);
                    a.lineTo (r.getRight() + kGap + 4.0f, y);
                    a.startNewSubPath (r.getRight() + kGap - 2.0f, y - 5.0f);
                    a.lineTo (r.getRight() + kGap + 4.0f, y);
                    a.lineTo (r.getRight() + kGap - 2.0f, y + 5.0f);
                    glowStroke (g, a, 1.8f, col::coral, 1.0f);
                }
                break;
        }

        // the locks that differ from plain, shown on every page so nothing hides
        if (page != Page::chance && chance < 100)
        {
            g.setColour (col::coral);
            g.fillEllipse (r.getX() + 5.0f, r.getY() + 5.0f, 6.0f, 6.0f);
        }
        if (page != Page::ratchet && ratchet > 1)
            glowText (g, "x" + juce::String (ratchet), { r.getRight() - 26.0f, r.getY() + 2.0f, 23.0f, 13.0f },
                      juce::Justification::centredRight, mono (9.0f), col::coral);
        if (page != Page::tie && tie)
            litBlock (g, { r.getRight() - 3.0f, r.getY() + 14.0f, 3.0f, r.getHeight() - 28.0f }, col::coral, 0.9f);
    }

    // where the pattern ends
    {
        const auto r = cell (L - 1);
        juce::Path b;
        const float x = r.getRight() + kGap * 0.5f;
        b.startNewSubPath (x - 5.0f, r.getY() - 2.0f);
        b.lineTo (x, r.getY() - 2.0f);
        b.lineTo (x, r.getBottom() + 2.0f);
        b.lineTo (x - 5.0f, r.getBottom() + 2.0f);
        glowStroke (g, b, 1.8f, col::coral, 1.0f);
    }
}

int PadGrid::cellAt (juce::Point<float> p) const
{
    for (int s = 0; s < thegrid::kSteps; ++s)
        if (cell (s).expanded (kGap * 0.5f).contains (p))
            return s;
    return -1;
}

void PadGrid::setLock (int st, int value)
{
    const int s = proc.editPattern();
    const auto l = lock();
    if (proc.pattern.get (l, s, st) == value && (l == thegrid::lkOn || proc.pattern.on (s, st)))
        return;
    proc.pattern.set (l, s, st, value);
    if (l != thegrid::lkOn && (l != thegrid::lkTie || value != 0))
        proc.pattern.set (thegrid::lkOn, s, st, 1);   // giving a step a value switches it on
    proc.patternEdited();
    repaint();
}

void PadGrid::nudge (int st, int units)
{
    const int s = proc.editPattern();
    const auto l = lock();
    const int v = proc.pattern.get (l, s, st);
    int t = v;
    switch (l)
    {
        case thegrid::lkNote:   t = thegrid::scaleStep (thegrid::quantise (v, params.scale), params.scale, units); break;
        case thegrid::lkChance: t = (v / 5 + units) * 5; break;
        case thegrid::lkOn: case thegrid::lkVel: case thegrid::lkGate: case thegrid::lkRatchet: case thegrid::lkTie:
                                t = v + units; break;
    }
    setLock (st, juce::jlimit (thegrid::kLockMin[l], thegrid::kLockMax[l], t));
}

void PadGrid::paintCell (int st)
{
    const int s = proc.editPattern();
    if (page == Page::step)
    {
        if (proc.pattern.on (s, st) != paintOn) { proc.pattern.set (thegrid::lkOn, s, st, paintOn ? 1 : 0); proc.patternEdited(); repaint(); }
    }
    else if (page == Page::tie)
        setLock (st, paintOn ? 1 : 0);
    else
        setLock (st, paintValue);
}

void PadGrid::mouseDown (const juce::MouseEvent& e)
{
    dragCell = cellAt (e.position);
    painting = false;
    if (dragCell < 0) return;
    const int s = proc.editPattern();

    // ALT: the pattern ends here
    if (e.mods.isAltDown())
    {
        proc.pattern.setLength (s, dragCell + 1);
        proc.patternEdited();
        dragCell = -1;
        repaint();
        return;
    }
    // CMD (or CTRL): a step on or off, whatever page you're on
    if (e.mods.isCommandDown())
    {
        proc.pattern.set (thegrid::lkOn, s, dragCell, proc.pattern.on (s, dragCell) ? 0 : 1);
        proc.patternEdited();
        dragCell = -1;
        repaint();
        return;
    }

    if (page == Page::step || page == Page::tie)
    {
        // click to flip, drag to paint the same way
        const auto l = page == Page::step ? thegrid::lkOn : thegrid::lkTie;
        paintOn = ! (proc.pattern.get (l, s, dragCell) != 0 && proc.pattern.on (s, dragCell));
        painting = true;
        paintCell (dragCell);
        return;
    }

    const int v = proc.pattern.get (lock(), s, dragCell);
    if (e.mods.isShiftDown())
    {
        // SHIFT: paint this pad's value across the pads you drag over
        paintValue = v;
        painting = true;
        if (! proc.pattern.on (s, dragCell)) { proc.pattern.set (thegrid::lkOn, s, dragCell, 1); proc.patternEdited(); repaint(); }
        return;
    }
    dragStartValue = lock() == thegrid::lkNote ? thegrid::quantise (v, params.scale) : v;
    dragStartY = e.position.y;
    if (! proc.pattern.on (s, dragCell)) { proc.pattern.set (thegrid::lkOn, s, dragCell, 1); proc.patternEdited(); repaint(); }
}

void PadGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragCell < 0) return;
    if (painting)
    {
        const int st = cellAt (e.position);
        if (st >= 0) paintCell (st);
        return;
    }
    const auto l = lock();
    const float dy = dragStartY - e.position.y;
    int target = dragStartValue;
    switch (l)
    {
        case thegrid::lkNote:
        {
            const int steps = juce::roundToInt (dy / 8.0f);
            target = steps == 0 ? dragStartValue : thegrid::scaleStep (dragStartValue, params.scale, steps);
            break;
        }
        case thegrid::lkVel:     target = dragStartValue + juce::roundToInt (dy / 1.5f); break;
        case thegrid::lkGate:    target = dragStartValue + juce::roundToInt (dy / 2.0f); break;
        case thegrid::lkChance:  target = (dragStartValue + juce::roundToInt (dy / 1.6f)) / 5 * 5; break;
        case thegrid::lkRatchet: target = dragStartValue + juce::roundToInt (dy / 16.0f); break;
        case thegrid::lkOn: case thegrid::lkTie: break;
    }
    setLock (dragCell, juce::jlimit (thegrid::kLockMin[l], thegrid::kLockMax[l], target));
}

void PadGrid::mouseUp (const juce::MouseEvent&)
{
    dragCell = -1;
    painting = false;
}

void PadGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int st = cellAt (e.position);
    if (st < 0 || page == Page::step || page == Page::tie || e.mods.isAltDown() || e.mods.isCommandDown())
        return;
    setLock (st, thegrid::kLockDef[lock()]);
}

void PadGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int st = cellAt (e.position);
    if (st < 0 || page == Page::step || page == Page::tie || std::abs (w.deltaY) <= 0.0f)
        return;
    nudge (st, w.deltaY > 0.0f ? 1 : -1);
}

void PadGrid::mouseMove (const juce::MouseEvent& e)
{
    const int st = cellAt (e.position);
    if (st != hover) { hover = st; repaint(); }
}

void PadGrid::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

juce::String PadGrid::getTooltip()
{
    if (hover < 0) return {};
    const juce::String step = "STEP " + juce::String (hover + 1) + ": ";
    switch (page)
    {
        case Page::step:    return step + "CLICK ON OR OFF, DRAG TO PAINT";
        case Page::note:    return step + "DRAG UP OR DOWN THROUGH THE SCALE";
        case Page::vel:     return step + "DRAG FOR VELOCITY";
        case Page::gate:    return step + "DRAG FOR NOTE LENGTH (SHARE OF THE STEP)";
        case Page::chance:  return step + "DRAG FOR THE CHANCE IT PLAYS";
        case Page::ratchet: return step + "DRAG FOR HITS IN THE STEP (1 TO 8)";
        case Page::tie:     return step + "CLICK: TIE INTO THE NEXT STEP";
    }
    return {};
}

//==============================================================================
void Title::tick()
{
    const bool b = skin.blink();
    if (b != lastBlink) repaint();
    lastBlink = b;
}

void Title::paint (juce::Graphics& g)
{
    // THE small and coral, GRID big and blue
    const auto fs = mono (20.0f).withExtraKerningFactor (0.3f);
    const auto f = mono (48.0f).withExtraKerningFactor (0.14f);
    const juce::String a ("THE"), b ("GRID");
    const float wa = textWidth (fs, a), wb = textWidth (f, b);
    const auto h = (float) getHeight();
    glowText (g, a, { 4.0f, 0.0f, wa + 10.0f, h * 0.5f + 8.0f }, juce::Justification::centredLeft, fs, col::coral);
    glowText (g, b, { 4.0f + wa + 14.0f, 0.0f, wb + 10.0f, h }, juce::Justification::centredLeft, f, col::ice);
    // a little 2 x 4 of pads for a cursor
    const float x0 = 4.0f + wa + 14.0f + wb + 16.0f, y0 = h * 0.5f - 16.0f;
    for (int i = 0; i < 8; ++i)
    {
        const auto r = juce::Rectangle<float> (x0 + (float) (i % 4) * 9.0f, y0 + (float) (i / 4) * 17.0f, 7.0f, 14.0f);
        const bool lit = (i == 0 || i == 5 || i == 3) || (i == 6 && skin.blink());
        if (lit) litBlock (g, r, i == 6 ? col::coral : col::ice, 1.0f);
        else glowBox (g, r, 1.0f, col::ice, 0.4f);
    }
}

//==============================================================================
Glass::Glass (const Skin& s) : skin (s)
{
    setInterceptsMouseClicks (false, false);
}

void Glass::resized()
{
    lines = juce::Image (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
    juce::Graphics g (lines);
    g.setColour (juce::Colours::black.withAlpha (0.24f));
    for (int y = 0; y < getHeight(); y += 3)
        g.fillRect (0, y, getWidth(), 1);
    const float w = (float) getWidth(), h = (float) getHeight();
    juce::ColourGradient v (juce::Colours::transparentBlack, w * 0.5f, h * 0.5f,
                            juce::Colours::black.withAlpha (0.45f), 0.0f, 0.0f, true);
    v.addColour (0.66, juce::Colours::transparentBlack);
    g.setGradientFill (v);
    g.fillRect (getLocalBounds());
}

void Glass::paint (juce::Graphics& g)
{
    const double age = skin.t - bootStart;
    if (age >= kBootSecs)
    {
        g.drawImageAt (lines, 0, 0);
        return;
    }
    const float fade = age < 2.6 ? 1.0f : (float) (1.0 - (age - 2.6) / (kBootSecs - 2.6));
    g.setColour (col::tube.withAlpha (juce::jlimit (0.0f, 1.0f, fade)));
    g.fillRect (getLocalBounds());

    struct Line { const char* text; juce::Colour c; };
    const Line boot[] = {
        { "MANNERS! DSP  INTERFACE SYSTEM", col::cream },
        { "", col::cream },
        { "THE GRID  STEP SEQUENCER", col::ice },
        { "PADS ........................ 8 X 16", col::ice },
        { "LOCKS PER STEP .............. 7", col::ice },
        { "DICE ........................ LOADED", col::coral },
        { "SWING ....................... OPTIONAL", col::coral },
        { "", col::cream },
        { "> READY", col::cream } };
    const int totalChars = (int) (age * 100.0);
    const auto f = mono (19.0f).withExtraKerningFactor (0.08f);
    int used = 0;
    float y = 280.0f, curX = 300.0f, curY = 280.0f;
    juce::Colour curC = col::cream;
    for (auto& line : boot)
    {
        const juce::String s (line.text);
        const int show = juce::jlimit (0, s.length(), totalChars - used);
        used += s.length() + 4;
        if (show > 0)
            glowText (g, s.substring (0, show), { 300.0f, y, 760.0f, 30.0f }, juce::Justification::centredLeft, f, line.c, fade);
        curX = 300.0f + textWidth (f, s.substring (0, show)) + 4.0f;
        curY = y;
        curC = line.c;
        if (show < s.length() || totalChars - used < 0)
            break;
        y += 32.0f;
    }
    if (std::fmod (age, 0.4) < 0.22)
    {
        g.setColour (curC.withAlpha (fade));
        g.fillRect (curX, curY + 5.0f, 11.0f, 20.0f);
    }
    g.drawImageAt (lines, 0, 0);
}

//==============================================================================
namespace
{
    constexpr int kGridX = (GridEditor::kBaseW - PadGrid::kW) / 2;   // 72
    constexpr int kGridY = 152;
    constexpr int kMidY = kGridY + PadGrid::kH + 12;                 // the pattern row
    constexpr int kBoxY = kMidY + 60;                                 // the three boxes
}

void GridEditor::Face::paint (juce::Graphics& g)
{
    g.fillAll (col::tube);

    glowText (g, "MANNERS! DSP  //  STEP SEQUENCER", { 44.0f, 76.0f, 460.0f, 20.0f },
              juce::Justification::centredLeft, mono (12.0f).withExtraKerningFactor (0.22f), col::cream, 0.7f);

    auto box = [&g] (juce::Rectangle<float> r, const char* name, juce::Colour c)
    {
        const auto f = mono (13.0f).withExtraKerningFactor (0.3f);
        const juce::String label = juce::String ("[ ") + name + " ]";
        const float tw = textWidth (f, label) + 12.0f;
        const float tx = r.getX() + 18.0f;
        juce::Path p;
        p.startNewSubPath (tx, r.getY());
        p.lineTo (r.getX(), r.getY());
        p.lineTo (r.getX(), r.getBottom());
        p.lineTo (r.getRight(), r.getBottom());
        p.lineTo (r.getRight(), r.getY());
        p.lineTo (tx + tw, r.getY());
        glowStroke (g, p, 1.2f, c, 0.6f);
        glowText (g, label, { tx, r.getY() - 10.0f, tw, 20.0f }, juce::Justification::centred, f, c);
    };
    const float by = (float) kBoxY, bh = 142.0f;
    box ({ 26.0f, by, 300.0f, bh }, "KEY", col::ice);
    box ({ 342.0f, by, 556.0f, bh }, "PLAY", col::coral);
    box ({ 914.0f, by, 300.0f, bh }, "OUTPUT", col::ice);

    glowText (g, "SHIFT-DRAG: PAINT A VALUE   CMD-CLICK: STEP ON/OFF   ALT-CLICK: PATTERN ENDS HERE   DOUBLE-CLICK: RESET THE LOCK",
              { 20.0f, (float) kBaseH - 20.0f, (float) kBaseW - 40.0f, 16.0f },
              juce::Justification::centred, mono (11.0f).withExtraKerningFactor (0.06f), col::cream, 0.55f);
}

//==============================================================================
GridEditor::GridEditor (GridProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (face);
    face.setBounds (0, 0, kBaseW, kBaseH);

    face.addAndMakeVisible (title);
    title.setBounds (40, 8, 460, 66);
    face.addAndMakeVisible (display);
    display.setBounds (kBaseW - 26 - 604, 10, 604, 88);

    auto addField = [this] (const juce::String& id, const juce::String& label, juce::Colour c, juce::Rectangle<int> b)
    {
        auto f = std::make_unique<Field> (proc.apvts, id, label, c);
        face.addAndMakeVisible (*f);
        f->setBounds (b);
        auto* raw = f.get();
        fields.push_back (std::move (f));
        return raw;
    };

    // the pages: what a click on a pad edits
    const char* pageNames[7] = { "STEP", "NOTE", "VEL", "GATE", "CHANCE", "RATCHET", "TIE" };
    for (int i = 0; i < 7; ++i)
    {
        auto& b = pages[(size_t) i];
        b.setButtonText (pageNames[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1);
        b.getProperties().set ("colour", (i < 2 ? col::ice : col::coral).toString());
        b.setBounds (kGridX + (int) PadGrid::kLabelW + i * 100, 108, 96, 36);
        face.addAndMakeVisible (b);
        b.onClick = [this, i] { if (pages[(size_t) i].getToggleState()) setPage ((Page) i); };
    }
    pages[0].setToggleState (true, juce::dontSendNotification);

    face.addAndMakeVisible (length);
    length.setBounds (kGridX + PadGrid::kW - 312, 108, 300, 36);

    face.addAndMakeVisible (grid);
    grid.setBounds (kGridX, kGridY, PadGrid::kW, PadGrid::kH);

    // patterns and what to do with them
    face.addAndMakeVisible (patterns);
    patterns.setBounds (kGridX + (int) PadGrid::kLabelW, kMidY, 400, 34);
    int bx = kGridX + (int) PadGrid::kLabelW + 412;
    for (auto* b : { &copyButton, &pasteButton, &clearButton, &saveButton, &loadButton })
    {
        b->getProperties().set ("colour", (b == &clearButton ? col::coral : b == &saveButton || b == &loadButton ? col::cream : col::ice).toString());
        b->setBounds (bx, kMidY, 76, 34);
        bx += 80;
        face.addAndMakeVisible (*b);
    }
    copyButton.setTooltip ("COPY THIS PATTERN");
    pasteButton.setTooltip ("PASTE OVER THIS PATTERN");
    clearButton.setTooltip ("EMPTY THIS PATTERN (CLICK TWICE)");
    saveButton.setTooltip ("SAVE THIS PATTERN TO A FILE");
    loadButton.setTooltip ("LOAD A PATTERN FILE INTO THIS SLOT");
    copyButton.onClick = [this] { clipboard = proc.patternToTree (proc.editPattern()); };
    pasteButton.onClick = [this] { if (clipboard.isValid()) proc.patternFromTree (proc.editPattern(), clipboard); };
    clearButton.onClick = [this]
    {
        if (nowSecs() < clearArmedUntil)
        {
            proc.pattern.clear (proc.editPattern());
            proc.patternEdited();
            clearArmedUntil = 0.0;
            clearButton.setButtonText ("CLEAR");
        }
        else
        {
            clearArmedUntil = nowSecs() + 2.0;
            clearButton.setButtonText ("SURE?");
        }
    };
    saveButton.onClick = [this] { savePattern(); };
    loadButton.onClick = [this] { loadPattern(); };

    // KEY
    const int fy = kBoxY + 14;
    addField (ParamIDs::key, "KEY", col::ice, { 38, fy, 276, 38 });
    addField (ParamIDs::scale, "SCALE", col::ice, { 38, fy + 40, 276, 38 });
    addField (ParamIDs::octave, "ROOT", col::ice, { 38, fy + 80, 276, 38 });

    // PLAY
    addField (ParamIDs::rate, "RATE", col::coral, { 354, fy, 262, 38 });
    addField (ParamIDs::dir, "DIR", col::coral, { 354, fy + 40, 262, 38 });
    skipField = addField (ParamIDs::skip, "SKIP", col::coral, { 354, fy + 80, 262, 38 });
    addField (ParamIDs::swing, "SWING", col::coral, { 624, fy, 262, 38 });
    addField (ParamIDs::reset, "RESET", col::coral, { 624, fy + 40, 262, 38 });
    addField (ParamIDs::seed, "SEED", col::coral, { 624, fy + 80, 262, 38 });

    // OUTPUT
    addField (ParamIDs::channel, "CHANNEL", col::ice, { 926, fy, 276, 38 });
    auto toggle = [this] (juce::TextButton& b, const char* id, juce::Colour c, juce::Rectangle<int> r, const char* tip)
    {
        b.setClickingTogglesState (true);
        b.getProperties().set ("lamp", true);
        b.getProperties().set ("colour", c.toString());
        b.setTooltip (tip);
        b.setBounds (r);
        face.addAndMakeVisible (b);
        buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b));
    };
    toggle (thruButton, ParamIDs::thru, col::ice, { 926, fy + 40, 112, 38 }, "LIT: NOTES YOU PLAY ALSO PASS THROUGH TO THE INSTRUMENT");
    toggle (transposeButton, ParamIDs::transpose, col::ice, { 1040, fy + 40, 162, 38 }, "LIT: NOTES YOU PLAY TRANSPOSE THE PATTERN. C3 LEAVES IT WHERE IT IS");
    toggle (runButton, ParamIDs::run, col::ice, { 926, fy + 80, 276, 38 }, "LIT: THE GRID RUNS WHENEVER LOGIC PLAYS");

    face.addAndMakeVisible (glass);
    glass.setBounds (0, 0, kBaseW, kBaseH);
    if (! proc.booted)
    {
        proc.booted = true;
        glass.boot (skin.t);
    }

    setResizable (true, true);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);
    setResizeLimits (868, 644, 1736, 1288);
    setSize (1054, 782);   // 85 %, so it fits a laptop screen; drag the corner for more
    startTimerHz (30);
}

GridEditor::~GridEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GridEditor::setPage (Page pg)
{
    grid.page = pg;
    for (int i = 0; i < 7; ++i)
        pages[(size_t) i].setToggleState ((int) pg == i, juce::dontSendNotification);
    grid.repaint();
}

void GridEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::tube);
}

void GridEditor::resized()
{
    face.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kBaseW));
}

void GridEditor::advance (double dt)
{
    skin.t += dt;
    display.tick();
    patterns.tick();
    grid.tick();
    title.tick();
    length.tick();

    // SKIP only means something on FWD, BWD and PEND
    const int d = proc.currentParams().dir;
    if (skipField != nullptr)
        skipField->setDimmed (d == thegrid::dirRand || d == thegrid::dirBrown);

    const bool booting = glass.booting();
    if (booting || wasBooting)
        glass.repaint();
    wasBooting = booting;

    const bool b = skin.blink();
    if (b != lastBlink)
        for (auto* btn : { &runButton, &transposeButton, &thruButton })
            if (btn->getToggleState()) btn->repaint();
    lastBlink = b;

    if (clearArmedUntil > 0.0 && nowSecs() > clearArmedUntil)
    {
        clearArmedUntil = 0.0;
        clearButton.setButtonText ("CLEAR");
    }
}

static juce::File patternFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("THE GRID Patterns");
    dir.createDirectory();
    return dir;
}

void GridEditor::savePattern()
{
    const int s = proc.editPattern();
    chooser = std::make_unique<juce::FileChooser> ("Save pattern " + juce::String (s + 1),
                                                   patternFolder().getChildFile ("Pattern " + juce::String (s + 1) + GridProcessor::kFileExtension),
                                                   juce::String ("*") + GridProcessor::kFileExtension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, s] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              f = f.withFileExtension (GridProcessor::kFileExtension);
                              if (auto xml = proc.patternToTree (s).createXml())
                                  xml->writeTo (f);
                          });
}

void GridEditor::loadPattern()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a pattern into slot " + juce::String (proc.editPattern() + 1),
                                                   patternFolder(), juce::String ("*") + GridProcessor::kFileExtension);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (! f.existsAsFile()) return;
                              if (auto xml = juce::parseXML (f))
                                  proc.patternFromTree (proc.editPattern(), juce::ValueTree::fromXml (*xml));
                          });
}
