#pragma once
#include "PluginProcessor.h"

namespace Theme
{
    const juce::Colour bg      { 0xff12151a };
    const juce::Colour panel   { 0xff1a1f26 };
    const juce::Colour line    { 0xff2a313b };
    const juce::Colour text    { 0xffe6ebf2 };
    const juce::Colour dim     { 0xff7d8794 };
    const juce::Colour accent  { 0xff2f86ff };
    const juce::Colour accent2 { 0xff1b5fd1 };
    const juce::Colour danger  { 0xffe5484d };
}

class OpenerLNF : public juce::LookAndFeel_V4
{
public:
    OpenerLNF();
    int getSliderThumbRadius (juce::Slider&) override { return 11; }
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos,
                           float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hover, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hover, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (13.f, juce::Font::bold)); }
};

// Display central: espectroscópio estéreo / goniômetro L-R / goniômetro M-S (clique para alternar)
class SpatialDisplay : public juce::Component, private juce::Timer
{
public:
    explicit SpatialDisplay (OpenerXAudioProcessor&);
    ~SpatialDisplay() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { viewMode = (viewMode + 1) % 3; repaint(); }

private:
    void timerCallback() override;
    void paintSpectroscope (juce::Graphics&, juce::Rectangle<float> area);
    void paintGoniometer (juce::Graphics&, juce::Rectangle<float> area, bool midSide);

    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, nBands = 56;

    OpenerXAudioProcessor& proc;
    int viewMode = 0;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 2 * fftSize> dataL {}, dataR {};
    std::array<float, nBands> pan {}, level {}, freq {}, width {};
};

class OpenerXAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OpenerXAudioProcessorEditor (OpenerXAudioProcessor&);
    ~OpenerXAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void setupSlider (juce::Slider&, const juce::String& suffix, int decimals, double def, bool horizontal = false);
    void setupToggle (juce::ToggleButton&, const juce::String& text, const juce::String& name);
    void setAdvanced (bool);
    void refreshPresetUi();
    void stepPreset (int delta);
    void showPresetMenu();
    static juce::File presetFolder();

    OpenerXAudioProcessor& proc;
    OpenerLNF lnf;
    SpatialDisplay display;
    bool advanced = false;

    // principais
    juce::Slider amount, angle, bass, treble, balance, output, compensation;
    juce::ToggleButton mono, flip, polarity, dim, power, dots;
    // avançados
    juce::Slider bassFreq, trebFreq, midGain, sideGain, softStart, dimLevel, inGain, postGain;
    juce::ToggleButton safeGain, dither, hq;
    juce::ComboBox realismBox;

    std::unique_ptr<SA> aAmount, aAngle, aBass, aTreble, aBalance, aOutput, aComp,
                        aBassF, aTrebF, aMid, aSide, aSoft, aDimLvl, aIn, aPost;
    std::unique_ptr<BA> aMono, aFlip, aPol, aDim, aPower, aSafe, aDither, aHq;
    std::unique_ptr<CA> aRealism;

    // barra de presets
    juce::TextButton undoBtn { "Undo" }, redoBtn { "Redo" }, prevBtn { "<" }, nextBtn { ">" },
                     presetBtn, aBtn { "A" }, bBtn { "B" }, copyBtn { "->" }, menuBtn { "..." };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> strip, crossfeedBox, eqBox, centreBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenerXAudioProcessorEditor)
};
