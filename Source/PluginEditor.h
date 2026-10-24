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
}

class OpenerLNF : public juce::LookAndFeel_V4
{
public:
    int getSliderThumbRadius (juce::Slider&) override { return 11; }
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos,
                           float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hover, bool down) override;
};

// Display central: bolhas por frequência (vertical) e posição estéreo (horizontal)
class SpatialDisplay : public juce::Component, private juce::Timer
{
public:
    explicit SpatialDisplay (OpenerXAudioProcessor&);
    ~SpatialDisplay() override { stopTimer(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, nBands = 56;

    OpenerXAudioProcessor& proc;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 2 * fftSize> dataL {}, dataR {};
    std::array<float, nBands> pan {}, level {}, freq {};
};

class OpenerXAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit OpenerXAudioProcessorEditor (OpenerXAudioProcessor&);
    ~OpenerXAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    OpenerXAudioProcessor& proc;
    OpenerLNF lnf;
    SpatialDisplay display;

    juce::Slider amount, angle, bass, treble, balance, output;
    juce::ToggleButton mono, flip, polarity, dim, power;
    std::unique_ptr<SA> aAmount, aAngle, aBass, aTreble, aBalance, aOutput;
    std::unique_ptr<BA> aMono, aFlip, aPol, aDim, aPower;

    juce::Rectangle<int> strip, crossfeedBox, eqBox;

    void setupSlider (juce::Slider&, const juce::String& suffix, int decimals, double def, bool horizontal = false);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenerXAudioProcessorEditor)
};
