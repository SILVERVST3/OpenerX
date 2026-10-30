#include "PluginEditor.h"

static juce::String formatFreq (double v)
{
    return v >= 1000.0 ? juce::String (v / 1000.0, 2) + " kHz" : juce::String ((int) std::round (v)) + " Hz";
}

//==============================================================================
OpenerLNF::OpenerLNF()
{
    setColour (juce::ComboBox::backgroundColourId, Theme::panel);
    setColour (juce::ComboBox::outlineColourId, Theme::line);
    setColour (juce::ComboBox::textColourId, Theme::text);
    setColour (juce::ComboBox::arrowColourId, Theme::accent);
    setColour (juce::PopupMenu::backgroundColourId, Theme::panel);
    setColour (juce::PopupMenu::textColourId, Theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accent2);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::textColourOffId, Theme::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void OpenerLNF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? Theme::accent2 : (down ? Theme::accent2.withAlpha (0.5f)
                                             : (hover ? Theme::panel.brighter (0.1f) : Theme::panel)));
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (on ? Theme::accent : Theme::line);
    g.drawRoundedRectangle (r, 6.f, 1.f);
}

void OpenerLNF::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                  float minPos, float maxPos, juce::Slider::SliderStyle style,
                                  juce::Slider& s)
{
    const bool vertical = style == juce::Slider::LinearVertical;
    const float lo = (float) s.getMinimum(), hi = (float) s.getMaximum();
    const float zero = (lo < 0.f && hi > 0.f) ? juce::jmap (0.f, lo, hi, minPos, maxPos) : minPos;
    const juce::Rectangle<float> area ((float) x, (float) y, (float) w, (float) h);
    const float thick = 4.f;

    juce::Rectangle<float> track, fill, thumb;
    const float tw = vertical ? 54.f : 60.f, th = 20.f;

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

    // medidor de pico embutido (Output Gain): vermelho quando >= 0 dB
    if (vertical && s.getProperties().contains ("meter"))
    {
        const float peak = (float) (double) s.getProperties()["meter"];
        const float db = juce::Decibels::gainToDecibels (peak, -60.f);
        const float frac = juce::jlimit (0.f, 1.f, (db + 48.f) / 54.f);   // -48 dB ... +6 dB
        const float top = minPos - frac * (minPos - maxPos);
        const float cx = area.getCentreX() - 13.f;
        g.setColour (Theme::line);
        g.fillRoundedRectangle (cx - 2.5f, maxPos, 5.f, minPos - maxPos, 2.f);
        g.setColour (peak >= 1.f ? Theme::danger : Theme::accent.withAlpha (0.8f));
        g.fillRoundedRectangle (cx - 2.5f, top, 5.f, minPos - top, 2.f);
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
    if (b.getName() == "dots")
    {
        auto c = r.getCentre();
        g.setColour (juce::Colours::white.withAlpha (on ? 1.f : (hover ? 0.8f : 0.55f)));
        for (int i = -1; i <= 1; ++i) g.fillEllipse (c.x + i * 9.f - 2.5f, c.y - 2.5f, 5.f, 5.f);
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

    if (viewMode == 0)
    {
        dataL.fill (0.f); dataR.fill (0.f);
        for (int i = 0; i < fftSize; ++i)
        {
            const size_t idx = (size_t) ((pos - fftSize + i) & mask);
            dataL[(size_t) i] = proc.fifoL[idx];
            dataR[(size_t) i] = proc.fifoR[idx];
        }
        window.multiplyWithWindowingTable (dataL.data(), fftSize);
        window.multiplyWithWindowingTable (dataR.data(), fftSize);
        fft.performRealOnlyForwardTransform (dataL.data());   // bins intercalados (re, im)
        fft.performRealOnlyForwardTransform (dataR.data());

        const float binHz = (float) proc.currentSampleRate.load() / (float) fftSize;
        for (int b = 0; b < nBands; ++b)
        {
            const float f0 = freq[(size_t) b] / 1.06f, f1 = freq[(size_t) b] * 1.06f;
            const int k0 = juce::jlimit (1, fftSize / 2 - 1, (int) (f0 / binHz));
            const int k1 = juce::jlimit (k0, fftSize / 2 - 1, (int) (f1 / binHz));
            float sLL = 0.f, sRR = 0.f, sLR = 0.f;
            for (int k = k0; k <= k1; ++k)
            {
                const float lr = dataL[(size_t) (2 * k)], li = dataL[(size_t) (2 * k + 1)];
                const float rr = dataR[(size_t) (2 * k)], ri = dataR[(size_t) (2 * k + 1)];
                sLL += lr * lr + li * li;
                sRR += rr * rr + ri * ri;
                sLR += lr * rr + li * ri;
            }
            const float a = std::sqrt (sLL), c = std::sqrt (sRR);
            const float amp = std::sqrt (a * a + c * c) / (fftSize * 0.25f);
            const float db = juce::Decibels::gainToDecibels (amp, -100.f);
            const float newPan = (c - a) / (a + c + 1e-9f);
            const float corr = sLR / (a * c + 1e-12f);                  // correlação L/R da banda
            const float newWidth = juce::jlimit (0.f, 1.f, (1.f - corr) * 0.5f);

            level[(size_t) b] = juce::jmax (db, level[(size_t) b] - 1.2f);
            pan[(size_t) b]   += 0.35f * (newPan - pan[(size_t) b]);
            width[(size_t) b] += 0.25f * (newWidth - width[(size_t) b]);
        }
    }
    repaint();
}

void SpatialDisplay::paintSpectroscope (juce::Graphics& g, juce::Rectangle<float> area)
{
    const float fMin = 30.f, fMax = 16000.f;
    auto yOf = [&] (float f) { return area.getBottom() - std::log (f / fMin) / std::log (fMax / fMin) * area.getHeight(); };

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

    // contorno largo: largura estéreo de cada banda
    juce::Path outline;
    std::array<juce::Point<float>, nBands> rightPts;
    bool started = false;
    for (int b = 0; b < nBands; ++b)
    {
        const float t = juce::jlimit (0.f, 1.f, (level[(size_t) b] + 80.f) / 70.f);
        const float cx = area.getCentreX() + pan[(size_t) b] * area.getWidth() * 0.46f;
        const float hw = (3.f + width[(size_t) b] * area.getWidth() * 0.42f) * t;
        const float y = yOf (freq[(size_t) b]);
        const float xl = juce::jmax (area.getX(), cx - hw), xr = juce::jmin (area.getRight(), cx + hw);
        if (! started) { outline.startNewSubPath (xl, y); started = true; }
        else           outline.lineTo (xl, y);
        rightPts[(size_t) b] = { xr, y };
    }
    for (int b = nBands - 1; b >= 0; --b) outline.lineTo (rightPts[(size_t) b]);
    outline.closeSubPath();
    g.setColour (Theme::accent.withAlpha (0.08f));
    g.fillPath (outline);
    g.setColour (Theme::accent.withAlpha (0.35f));
    g.strokePath (outline, juce::PathStrokeType (1.f));

    // círculos: posição = balance + frequência; tamanho e brilho = nível da banda
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

void SpatialDisplay::paintGoniometer (juce::Graphics& g, juce::Rectangle<float> area, bool midSide)
{
    const auto c = area.getCentre();
    const float half = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    g.setColour (Theme::line);
    g.drawEllipse (c.x - half, c.y - half, half * 2.f, half * 2.f, 1.f);
    g.drawHorizontalLine ((int) c.y, c.x - half, c.x + half);
    g.drawVerticalLine ((int) c.x, c.y - half, c.y + half);
    g.setColour (Theme::dim);
    g.setFont (10.f);
    if (midSide)
    {
        g.drawText ("M", (int) c.x + 4, (int) (c.y - half), 14, 12, juce::Justification::left);
        g.drawText ("S", (int) (c.x + half) - 14, (int) c.y - 14, 14, 12, juce::Justification::right);
    }
    else
    {
        g.drawText ("L", (int) (c.x - half), (int) c.y - 14, 14, 12, juce::Justification::left);
        g.drawText ("R", (int) (c.x + half) - 14, (int) c.y - 14, 14, 12, juce::Justification::right);
    }

    const int pos = proc.fifoPos.load (std::memory_order_acquire);
    const int mask = OpenerXAudioProcessor::fifoSize - 1;
    g.setColour (Theme::accent.withAlpha (0.35f));
    for (int i = 0; i < 1024; ++i)
    {
        const size_t idx = (size_t) ((pos - 1024 + i) & mask);
        const float l = proc.fifoL[idx], r = proc.fifoR[idx];
        float x, y;
        if (midSide) { x = 0.5f * (l - r); y = 0.5f * (l + r); }
        else         { x = l;              y = r; }
        x = juce::jlimit (-1.f, 1.f, x) * half * 0.95f;
        y = juce::jlimit (-1.f, 1.f, y) * half * 0.95f;
        g.fillEllipse (c.x + x - 1.2f, c.y - y - 1.2f, 2.4f, 2.4f);
    }
}

void SpatialDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (r, 8.f);

    auto area = r.reduced (10.f);
    if (viewMode == 0)      paintSpectroscope (g, area);
    else                    paintGoniometer (g, area, viewMode == 2);

    g.setColour (Theme::dim);
    g.setFont (10.f);
    const char* names[] = { "Stereo Spectroscope", "Goniometer L/R", "Goniometer M/S" };
    g.drawText (names[viewMode], getLocalBounds().removeFromTop (18).reduced (10, 0), juce::Justification::centredRight);
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

void OpenerXAudioProcessorEditor::setupToggle (juce::ToggleButton& b, const juce::String& text, const juce::String& name)
{
    b.setButtonText (text);
    b.setName (name);
    b.setLookAndFeel (&lnf);
    addAndMakeVisible (b);
}

OpenerXAudioProcessorEditor::OpenerXAudioProcessorEditor (OpenerXAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p)
{
    setSize (700, 470);
    auto& st = proc.apvts;

    // --- sliders principais ---
    setupSlider (amount, "%", 0, 100.0);
    setupSlider (angle, juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")), 0, 60.0);
    setupSlider (bass, " dB", 1, 0.0);
    setupSlider (treble, " dB", 1, 0.0);
    setupSlider (balance, "%", 0, 0.0, true);
    setupSlider (output, " dB", 1, 0.0);
    setupSlider (compensation, "%", 0, 100.0, true);

    // --- sliders avançados (horizontais) ---
    setupSlider (bassFreq, "", 0, 250.0, true);   bassFreq.textFromValueFunction = formatFreq;
    setupSlider (trebFreq, "", 0, 2000.0, true);  trebFreq.textFromValueFunction = formatFreq;
    setupSlider (midGain,  " dB", 1, 0.0, true);
    setupSlider (sideGain, " dB", 1, 0.0, true);
    setupSlider (softStart, " ms", 0, 250.0, true);
    setupSlider (dimLevel, " dB", 1, -20.0, true);
    setupSlider (inGain,   " dB", 1, 0.0, true);
    setupSlider (postGain, " dB", 1, 0.0, true);

    aAmount  = std::make_unique<SA> (st, "amount", amount);
    aAngle   = std::make_unique<SA> (st, "angle", angle);
    aBass    = std::make_unique<SA> (st, "bass", bass);
    aTreble  = std::make_unique<SA> (st, "treble", treble);
    aBalance = std::make_unique<SA> (st, "balance", balance);
    aOutput  = std::make_unique<SA> (st, "output", output);
    aComp    = std::make_unique<SA> (st, "compensation", compensation);
    aBassF   = std::make_unique<SA> (st, "bassfreq", bassFreq);
    aTrebF   = std::make_unique<SA> (st, "treblefreq", trebFreq);
    aMid     = std::make_unique<SA> (st, "midgain", midGain);
    aSide    = std::make_unique<SA> (st, "sidegain", sideGain);
    aSoft    = std::make_unique<SA> (st, "softstart", softStart);
    aDimLvl  = std::make_unique<SA> (st, "dimlevel", dimLevel);
    aIn      = std::make_unique<SA> (st, "ingain", inGain);
    aPost    = std::make_unique<SA> (st, "postgain", postGain);

    // --- botões ---
    setupToggle (mono, "Mono", "mono");
    setupToggle (flip, "Flip L/R", "flip");
    setupToggle (polarity, "Polarity", "polarity");
    setupToggle (dim, "Dim", "dim");
    setupToggle (power, "", "power");
    setupToggle (dots, "", "dots");
    setupToggle (safeGain, "Safe Gain", "safegain");
    setupToggle (dither, "Dither", "dither");
    setupToggle (hq, "HQ Mode", "hq");
    aMono   = std::make_unique<BA> (st, "mono", mono);
    aFlip   = std::make_unique<BA> (st, "flip", flip);
    aPol    = std::make_unique<BA> (st, "polarity", polarity);
    aDim    = std::make_unique<BA> (st, "dim", dim);
    aPower  = std::make_unique<BA> (st, "power", power);
    aSafe   = std::make_unique<BA> (st, "safegain", safeGain);
    aDither = std::make_unique<BA> (st, "dither", dither);
    aHq     = std::make_unique<BA> (st, "hq", hq);
    dots.onClick = [this] { setAdvanced (dots.getToggleState()); };

    // --- Crossfeed Realism ---
    realismBox.setLookAndFeel (&lnf);
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (st.getParameter ("realism")))
        realismBox.addItemList (choice->choices, 1);
    addAndMakeVisible (realismBox);
    aRealism = std::make_unique<CA> (st, "realism", realismBox);

    addAndMakeVisible (display);

    // --- barra de presets: Undo/Redo, < nome >, A -> B ---
    for (auto* b : { &undoBtn, &redoBtn, &prevBtn, &nextBtn, &presetBtn, &aBtn, &bBtn, &copyBtn, &menuBtn })
    {
        b->setLookAndFeel (&lnf);
        addAndMakeVisible (b);
    }
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (+1); };
    presetBtn.onClick = [this]
    {
        if (juce::ModifierKeys::currentModifiers.isAltDown())      // Alt + clique: volta aos valores do preset
        {
            proc.setCurrentProgram (proc.getCurrentProgram());
            refreshPresetUi();
        }
        else
            showPresetMenu();
    };
    menuBtn.onClick = [this] { showPresetMenu(); };
    aBtn.onClick = [this] { proc.selectSlot (0); refreshPresetUi(); };
    bBtn.onClick = [this] { proc.selectSlot (1); refreshPresetUi(); };
    copyBtn.onClick = [this] { proc.copyToOtherSlot(); };

    setAdvanced (false);
    refreshPresetUi();
    startTimerHz (30);
}

OpenerXAudioProcessorEditor::~OpenerXAudioProcessorEditor()
{
    stopTimer();
    for (auto* s : { &amount, &angle, &bass, &treble, &balance, &output, &compensation,
                     &bassFreq, &trebFreq, &midGain, &sideGain, &softStart, &dimLevel, &inGain, &postGain })
        s->setLookAndFeel (nullptr);
    for (auto* b : { &mono, &flip, &polarity, &dim, &power, &dots, &safeGain, &dither, &hq })
        b->setLookAndFeel (nullptr);
    realismBox.setLookAndFeel (nullptr);
    for (auto* b : { &undoBtn, &redoBtn, &prevBtn, &nextBtn, &presetBtn, &aBtn, &bBtn, &copyBtn, &menuBtn })
        b->setLookAndFeel (nullptr);
}

void OpenerXAudioProcessorEditor::timerCallback()
{
    output.getProperties().set ("meter", (double) proc.outPeak.load());
    output.repaint();
    undoBtn.setEnabled (proc.undoManager.canUndo());
    redoBtn.setEnabled (proc.undoManager.canRedo());
}

void OpenerXAudioProcessorEditor::refreshPresetUi()
{
    presetBtn.setButtonText (proc.getProgramName (proc.getCurrentProgram()));
    aBtn.setToggleState (proc.abCurrent == 0, juce::dontSendNotification);
    bBtn.setToggleState (proc.abCurrent == 1, juce::dontSendNotification);
}

void OpenerXAudioProcessorEditor::setAdvanced (bool on)
{
    advanced = on;
    display.setVisible (! on);
    balance.setVisible (! on);
    for (auto* c : std::initializer_list<juce::Component*> { &bassFreq, &trebFreq, &midGain, &sideGain, &softStart,
                                                             &dimLevel, &inGain, &postGain, &safeGain, &dither, &hq, &realismBox })
        c->setVisible (on);
    repaint();
}

void OpenerXAudioProcessorEditor::stepPreset (int delta)
{
    const int n = proc.getNumPrograms();
    proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
    refreshPresetUi();
}

juce::File OpenerXAudioProcessorEditor::presetFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("OpenerX Presets");
    dir.createDirectory();
    return dir;
}

void OpenerXAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);

    juce::StringArray cats;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        cats.addIfNotAlreadyThere (OpenerXAudioProcessor::getPresetCategory (i));
    for (auto& cat : cats)
    {
        juce::PopupMenu sub;
        sub.setLookAndFeel (&lnf);
        for (int i = 0; i < proc.getNumPrograms(); ++i)
            if (OpenerXAudioProcessor::getPresetCategory (i) == cat)
                sub.addItem (1000 + i, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        m.addSubMenu (cat, sub);
    }
    m.addSeparator();
    m.addItem (1, "Save preset...");
    m.addItem (2, "Load preset...");
    m.addItem (3, "Reset All to Defaults");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn), [this] (int r)
    {
        if (r >= 1000)
        {
            proc.setCurrentProgram (r - 1000);
            refreshPresetUi();
        }
        else if (r == 1)
        {
            chooser = std::make_unique<juce::FileChooser> ("Save preset", presetFolder(), "*.opxpreset");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                      | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File()) return;
                f = f.withFileExtension ("opxpreset");
                if (auto xml = proc.apvts.copyState().createXml())
                    xml->writeTo (f);
                presetBtn.setButtonText ("User: " + f.getFileNameWithoutExtension());
            });
        }
        else if (r == 2)
        {
            chooser = std::make_unique<juce::FileChooser> ("Load preset", presetFolder(), "*.opxpreset");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (! f.existsAsFile()) return;
                if (auto xml = juce::XmlDocument::parse (f))
                    if (xml->hasTagName (proc.apvts.state.getType()))
                    {
                        proc.apvts.replaceState (juce::ValueTree::fromXml (*xml));
                        presetBtn.setButtonText ("User: " + f.getFileNameWithoutExtension());
                    }
            });
        }
        else if (r == 3)
        {
            proc.resetAllToDefaults();
            presetBtn.setButtonText ("Default");
        }
    });
}

//==============================================================================
void OpenerXAudioProcessorEditor::resized()
{
    auto b = getLocalBounds();
    strip = b.removeFromRight (70);
    b.removeFromTop (6);

    // barra de presets (rodapé)
    auto footer = b.removeFromBottom (40);
    auto bar = footer.reduced (10, 6);
    undoBtn.setBounds (bar.removeFromLeft (46));  bar.removeFromLeft (4);
    redoBtn.setBounds (bar.removeFromLeft (46));  bar.removeFromLeft (12);
    menuBtn.setBounds (bar.removeFromRight (34)); bar.removeFromRight (8);
    bBtn.setBounds (bar.removeFromRight (28));
    copyBtn.setBounds (bar.removeFromRight (30));
    aBtn.setBounds (bar.removeFromRight (28));    bar.removeFromRight (12);
    prevBtn.setBounds (bar.removeFromLeft (28));  bar.removeFromLeft (4);
    nextBtn.setBounds (bar.removeFromRight (28)); bar.removeFromRight (4);
    presetBtn.setBounds (bar);

    // esquerda: Crossfeed (Amount, Angle) e Equalization (Bass, Treble)
    auto left = b.removeFromLeft (170).reduced (8, 0);
    crossfeedBox = left.removeFromTop (left.getHeight() / 2).reduced (0, 4);
    eqBox = left.reduced (0, 4);
    auto place2 = [&] (juce::Rectangle<int> box, juce::Slider& a, juce::Slider& c)
    {
        auto inner = box.withTrimmedTop (28).withTrimmedBottom (24);
        a.setBounds (inner.removeFromLeft (inner.getWidth() / 2));
        c.setBounds (inner);
    };
    place2 (crossfeedBox, amount, angle);
    place2 (eqBox, bass, treble);

    // direita: Mono / Flip / Polarity / Dim, Output Gain (com medidor) e Compensation
    auto right = b.removeFromRight (120).reduced (8, 0);
    for (auto* btn : { &mono, &flip, &polarity, &dim })
    {
        btn->setBounds (right.removeFromTop (34));
        right.removeFromTop (4);
    }
    right.removeFromTop (6);
    compensation.setBounds (right.removeFromBottom (24));
    right.removeFromBottom (18);
    output.setBounds (right.withTrimmedBottom (22));

    // centro: display + balance, ou painel avançado
    centreBox = b.reduced (6, 0);
    auto centre = centreBox;
    balance.setBounds (centre.removeFromBottom (60).reduced (24, 10).withTrimmedBottom (14));
    display.setBounds (centre.withTrimmedBottom (4));

    auto adv = centreBox.reduced (4, 2);
    adv.removeFromTop (24);   // título
    realismBox.setBounds (adv.removeFromTop (28).withTrimmedLeft (110));
    adv.removeFromTop (6);
    for (auto* s : { &bassFreq, &trebFreq, &midGain, &sideGain, &softStart, &dimLevel, &inGain, &postGain })
    {
        s->setBounds (adv.removeFromTop (26).withTrimmedLeft (110));
        adv.removeFromTop (2);
    }
    adv.removeFromTop (8);
    auto toggles = adv.removeFromTop (30);
    const int tw = toggles.getWidth() / 3;
    safeGain.setBounds (toggles.removeFromLeft (tw).reduced (3, 0));
    dither.setBounds (toggles.removeFromLeft (tw).reduced (3, 0));
    hq.setBounds (toggles.reduced (3, 0));

    power.setBounds (strip.withSizeKeepingCentre (50, 50).withY (getHeight() - 170));
    dots.setBounds (strip.withSizeKeepingCentre (50, 40).withY (getHeight() - 112));
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
    header (eqBox, "EQUALIZATION");
    caption (amount, "Amount");     caption (angle, "Angle");
    caption (bass, "Bass Gain");    caption (treble, "Treble Gain");
    caption (output, "Output Gain");

    g.setColour (Theme::dim);
    g.setFont (11.5f);
    g.drawText ("Compensation", compensation.getBounds().translated (0, 20).withHeight (16), juce::Justification::centred);

    if (! advanced)
    {
        g.drawText ("L", balance.getX() - 22, balance.getY(), 18, balance.getHeight(), juce::Justification::centred);
        g.drawText ("R", balance.getRight() + 4, balance.getY(), 18, balance.getHeight(), juce::Justification::centred);
        g.drawText ("L/R Balance", balance.getBounds().translated (0, 22), juce::Justification::centred);
    }
    else
    {
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (centreBox.toFloat(), 8.f);
        g.setColour (Theme::text);
        g.setFont (juce::Font (juce::FontOptions (15.f, juce::Font::bold)));
        g.drawText ("ADVANCED", centreBox.getX() + 14, centreBox.getY() + 4, 200, 22, juce::Justification::centredLeft);

        g.setColour (Theme::dim);
        g.setFont (11.5f);
        auto label = [&] (juce::Component& c, const juce::String& t)
        {
            g.drawText (t, centreBox.getX() + 14, c.getY(), 100, c.getHeight(), juce::Justification::centredLeft);
        };
        label (realismBox, "Crossfeed Realism");
        label (bassFreq, "Bass Freq");   label (trebFreq, "Treble Freq");
        label (midGain, "Mid Gain");     label (sideGain, "Side Gain");
        label (softStart, "Soft Start"); label (dimLevel, "Dim Level");
        label (inGain, "Pre Gain");      label (postGain, "Post Gain");
    }

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
