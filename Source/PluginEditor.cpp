#include "PluginEditor.h"

//==============================================================================
void OpenerLNF::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                  float minPos, float maxPos, juce::Slider::SliderStyle style,
                                  juce::Slider& s)
{
    const bool vertical = style == juce::Slider::LinearVertical;
    const float lo = (float) s.getMinimum(), hi = (float) s.getMaximum();
    const float zero = (lo < 0.f && hi > 0.f) ? juce::jmap (0.f, lo, hi, minPos, maxPos) : minPos;
    const juce::Rectangle<float> area ((float) x, (float) y, (float) w, (float) h);
    const float thick = 4.f;

    juce::Rectangle<float> track, fill;
    juce::Rectangle<float> thumb;
    const float tw = vertical ? 46.f : 52.f, th = 20.f;

    if (vertical)
    {
        const float cx = area.getCentreX();
        track = { cx - thick / 2, juce::jmin (minPos, maxPos), thick, std::abs (maxPos - minPos) };
        fill  = { cx - thick / 2, juce::jmin (pos, zero), thick, std::abs (pos - zero) };
        thumb = { cx - tw / 2, pos - th / 2, tw, th };
    }
    else
    {
        const float cy = area.getCentreY();
        track = { juce::jmin (minPos, maxPos), cy - thick / 2, std::abs (maxPos - minPos), thick };
        fill  = { juce::jmin (pos, zero), cy - thick / 2, std::abs (pos - zero), thick };
        thumb = { pos - tw / 2, cy - th / 2, tw, th };
    }

    g.setColour (Theme::line);
    g.fillRoundedRectangle (track, 2.f);
    g.setColour (Theme::accent);
    g.fillRoundedRectangle (fill, 2.f);

    g.setColour (Theme::accent.withAlpha (0.25f));
    g.fillRoundedRectangle (thumb.expanded (2.f), th * 0.5f + 2.f);
    g.setColour (Theme::accent2);
    g.fillRoundedRectangle (thumb, th * 0.5f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    g.drawText (s.getTextFromValue (s.getValue()), thumb, juce::Justification::centred, false);
}

void OpenerLNF::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();

    if (b.getName() == "power")
    {
        auto c = r.getCentre();
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.28f;
        g.setColour (on ? juce::Colours::white : juce::Colours::white.withAlpha (hover ? 0.7f : 0.45f));
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rad, rad, 0.f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
        g.strokePath (arc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (c.x, c.y - rad * 1.15f, c.x, c.y - rad * 0.1f, 2.6f);
        return;
    }

    r = r.reduced (0.f, 2.f);
    g.setColour (on ? Theme::accent.withAlpha (0.18f) : (hover ? Theme::panel.brighter (0.08f) : Theme::panel));
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (on ? Theme::accent : Theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);
    g.setColour (on ? Theme::text : Theme::dim);
    g.setFont (juce::Font (juce::FontOptions (13.f, juce::Font::bold)));
    g.drawText (b.getButtonText(), r, juce::Justification::centred, false);
}

//==============================================================================
SpatialDisplay::SpatialDisplay (OpenerXAudioProcessor& p) : proc (p)
{
    for (int b = 0; b < nBands; ++b)
        freq[(size_t) b] = 40.f * std::pow (14000.f / 40.f, (float) b / (nBands - 1));
    level.fill (-100.f);
    startTimerHz (30);
}

void SpatialDisplay::timerCallback()
{
    const int pos = proc.fifoPos.load (std::memory_order_acquire);
    const int mask = OpenerXAudioProcessor::fifoSize - 1;
    dataL.fill (0.f); dataR.fill (0.f);
    for (int i = 0; i < fftSize; ++i)
    {
        const size_t idx = (size_t) ((pos - fftSize + i) & mask);
        dataL[(size_t) i] = proc.fifoL[idx];
        dataR[(size_t) i] = proc.fifoR[idx];
    }
    window.multiplyWithWindowingTable (dataL.data(), fftSize);
    window.multiplyWithWindowingTable (dataR.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform (dataL.data());
    fft.performFrequencyOnlyForwardTransform (dataR.data());

    const float sr = (float) proc.currentSampleRate.load();
    const float binHz = sr / (float) fftSize;
    for (int b = 0; b < nBands; ++b)
    {
        const float f0 = freq[(size_t) b] / 1.06f, f1 = freq[(size_t) b] * 1.06f;
        const int k0 = juce::jlimit (1, fftSize / 2 - 1, (int) (f0 / binHz));
        const int k1 = juce::jlimit (k0, fftSize / 2 - 1, (int) (f1 / binHz));
        float sl = 0.f, sr2 = 0.f;
        for (int k = k0; k <= k1; ++k)
        {
            sl  += dataL[(size_t) k] * dataL[(size_t) k];
            sr2 += dataR[(size_t) k] * dataR[(size_t) k];
        }
        const float a = std::sqrt (sl), c = std::sqrt (sr2);
        const float amp = std::sqrt (a * a + c * c) / (fftSize * 0.25f);
        const float db = juce::Decibels::gainToDecibels (amp, -100.f);
        const float newPan = (c - a) / (a + c + 1e-9f);

        level[(size_t) b] = juce::jmax (db, level[(size_t) b] - 1.2f);
        pan[(size_t) b] += 0.35f * (newPan - pan[(size_t) b]);
    }
    repaint();
}

void SpatialDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (r, 8.f);

    auto area = r.reduced (10.f);
    const float fMin = 30.f, fMax = 16000.f;
    auto yOf = [&] (float f)
    {
        return area.getBottom() - std::log (f / fMin) / std::log (fMax / fMin) * area.getHeight();
    };

    g.setFont (10.f);
    for (float f : { 100.f, 1000.f, 10000.f })
    {
        const float y = yOf (f);
        g.setColour (Theme::line);
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        g.setColour (Theme::dim);
        g.drawText (f >= 1000.f ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f),
                    (int) area.getX() + 2, (int) y - 12, 30, 12, juce::Justification::left);
    }
    g.setColour (Theme::line);
    g.drawVerticalLine ((int) area.getCentreX(), area.getY(), area.getBottom());

    for (int b = 0; b < nBands; ++b)
    {
        const float db = level[(size_t) b];
        if (db < -75.f) continue;
        const float t = juce::jlimit (0.f, 1.f, (db + 75.f) / 65.f);
        const float rad = 2.f + t * 15.f;
        const float x = area.getCentreX() + pan[(size_t) b] * area.getWidth() * 0.46f;
        const float y = yOf (freq[(size_t) b]);
        g.setColour (Theme::accent.withAlpha (0.12f + 0.55f * t));
        g.fillEllipse (x - rad, y - rad, rad * 2.f, rad * 2.f);
    }
}

//==============================================================================
void OpenerXAudioProcessorEditor::setupSlider (juce::Slider& s, const juce::String& suffix,
                                               int decimals, double def, bool horizontal)
{
    s.setSliderStyle (horizontal ? juce::Slider::LinearHorizontal : juce::Slider::LinearVertical);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setTextValueSuffix (suffix);
    s.setNumDecimalPlacesToDisplay (decimals);
    s.setDoubleClickReturnValue (true, def);
    s.setLookAndFeel (&lnf);
    addAndMakeVisible (s);
}

OpenerXAudioProcessorEditor::OpenerXAudioProcessorEditor (OpenerXAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p)
{
    setSize (640, 420);

    setupSlider (amount,  "%",    0, 50.0);
    setupSlider (angle,   juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")), 0, 55.0);
    setupSlider (bass,    " dB",  1, 0.0);
    setupSlider (treble,  " dB",  1, 0.0);
    setupSlider (balance, "%",    0, 0.0, true);
    setupSlider (output,  " dB",  1, 0.0);

    auto& st = proc.apvts;
    aAmount  = std::make_unique<SA> (st, "amount", amount);
    aAngle   = std::make_unique<SA> (st, "angle", angle);
    aBass    = std::make_unique<SA> (st, "bass", bass);
    aTreble  = std::make_unique<SA> (st, "treble", treble);
    aBalance = std::make_unique<SA> (st, "balance", balance);
    aOutput  = std::make_unique<SA> (st, "output", output);

    auto setupBtn = [&] (juce::ToggleButton& b, const juce::String& txt, const juce::String& name)
    {
        b.setButtonText (txt);
        b.setName (name);
        b.setLookAndFeel (&lnf);
        addAndMakeVisible (b);
    };
    setupBtn (mono, "Mono", "mono");
    setupBtn (flip, "Flip L/R", "flip");
    setupBtn (polarity, "Polarity", "polarity");
    setupBtn (dim, "Dim", "dim");
    setupBtn (power, "", "power");

    aMono  = std::make_unique<BA> (st, "mono", mono);
    aFlip  = std::make_unique<BA> (st, "flip", flip);
    aPol   = std::make_unique<BA> (st, "polarity", polarity);
    aDim   = std::make_unique<BA> (st, "dim", dim);
    aPower = std::make_unique<BA> (st, "power", power);

    addAndMakeVisible (display);
}

OpenerXAudioProcessorEditor::~OpenerXAudioProcessorEditor()
{
    for (auto* s : { &amount, &angle, &bass, &treble, &balance, &output }) s->setLookAndFeel (nullptr);
    for (auto* b : { &mono, &flip, &polarity, &dim, &power }) b->setLookAndFeel (nullptr);
}

void OpenerXAudioProcessorEditor::resized()
{
    auto b = getLocalBounds();
    strip = b.removeFromRight (70);
    b.removeFromTop (6);
    b.removeFromBottom (6);

    auto left = b.removeFromLeft (170).reduced (8, 0);
    crossfeedBox = left.removeFromTop (left.getHeight() / 2).reduced (0, 4);
    eqBox = left.reduced (0, 4);

    auto place = [&] (juce::Rectangle<int> box, juce::Slider& a, juce::Slider& c)
    {
        auto inner = box.withTrimmedTop (28).withTrimmedBottom (24);
        a.setBounds (inner.removeFromLeft (inner.getWidth() / 2));
        c.setBounds (inner);
    };
    place (crossfeedBox, amount, angle);
    place (eqBox, bass, treble);

    auto right = b.removeFromRight (104).reduced (8, 0);
    for (auto* btn : { &mono, &flip, &polarity, &dim })
    {
        btn->setBounds (right.removeFromTop (34));
        right.removeFromTop (4);
    }
    right.removeFromTop (6);
    output.setBounds (right.withTrimmedBottom (24));

    auto centre = b.reduced (6, 0);
    balance.setBounds (centre.removeFromBottom (60).reduced (24, 10).withTrimmedBottom (14));
    display.setBounds (centre.withTrimmedBottom (4));

    power.setBounds (strip.withSizeKeepingCentre (50, 50).withY (getHeight() - 120));
}

void OpenerXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);

    auto header = [&] (juce::Rectangle<int> r, const juce::String& t)
    {
        g.setColour (Theme::text);
        g.setFont (juce::Font (juce::FontOptions (15.f, juce::Font::bold)));
        g.drawText (t, r.getX(), r.getY(), r.getWidth(), 22, juce::Justification::centredLeft);
        g.setColour (Theme::line);
        g.drawHorizontalLine (r.getY() - 2, (float) r.getX(), (float) r.getRight());
    };
    auto caption = [&] (juce::Slider& s, const juce::String& t)
    {
        g.setColour (Theme::dim);
        g.setFont (11.5f);
        g.drawText (t, s.getX() - 8, s.getBottom() + 4, s.getWidth() + 16, 16, juce::Justification::centred);
    };
    header (crossfeedBox, "CROSSFEED");
    header (eqBox, "EQUALIZER");
    caption (amount, "Amount"); caption (angle, "Angle");
    caption (bass, "Bass");     caption (treble, "Treble");
    caption (output, "Output");
    g.setColour (Theme::dim);
    g.setFont (11.5f);
    g.drawText ("L", balance.getX() - 22, balance.getY(), 18, balance.getHeight(), juce::Justification::centred);
    g.drawText ("R", balance.getRight() + 4, balance.getY(), 18, balance.getHeight(), juce::Justification::centred);
    g.drawText ("L/R Balance", balance.getBounds().translated (0, 22), juce::Justification::centred);

    // faixa azul da marca
    juce::ColourGradient grad (Theme::accent, (float) strip.getX(), 0.f,
                               Theme::accent2.darker (0.3f), (float) strip.getRight(), (float) getHeight(), false);
    g.setGradientFill (grad);
    g.fillRect (strip);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (22.f, juce::Font::bold)));
    g.drawText ("OPENER", strip.withHeight (28).withY (24), juce::Justification::centred);
    g.setFont (juce::Font (juce::FontOptions (15.f, juce::Font::bold)));
    g.drawText ("X", strip.withHeight (22).withY (50), juce::Justification::centred);
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.setFont (10.f);
    g.drawText ("crossfeed", strip.withHeight (14).withY (76), juce::Justification::centred);
}
