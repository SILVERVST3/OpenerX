#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

class OpenerXAudioProcessor : public juce::AudioProcessor
{
public:
    OpenerXAudioProcessor();
    ~OpenerXAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "OpenerX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // buffers circulares usados pelo display do editor
    static constexpr int fifoSize = 4096;
    std::array<float, fifoSize> fifoL {}, fifoR {};
    std::atomic<int> fifoPos { 0 };
    std::atomic<double> currentSampleRate { 44100.0 };

private:
    using Delay = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;
    Delay delay { 2048 };

    juce::dsp::IIR::Filter<float> bassF[2], trebF[2];
    float lpState[2][2] { { 0, 0 }, { 0, 0 } };
    float lastBass = 1000.f, lastTreble = 1000.f;

    juce::SmoothedValue<float> amountSm, outSm;

    std::atomic<float> *pAmount, *pAngle, *pBass, *pTreble, *pBalance, *pOutput,
                       *pMono, *pFlip, *pPolarity, *pDim, *pPower;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenerXAudioProcessor)
};
