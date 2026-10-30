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

    // presets de fábrica (expostos ao host como "programas")
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    static juce::String getPresetCategory (int index);
    void resetAllToDefaults();

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::UndoManager undoManager;                 // deve vir antes do apvts
    juce::AudioProcessorValueTreeState apvts;

    // A/B
    int abCurrent = 0;
    juce::ValueTree abState[2];
    void selectSlot (int index);
    void copyToOtherSlot();

    // dados para o display / medidor do editor
    static constexpr int fifoSize = 4096;
    std::array<float, fifoSize> fifoL {}, fifoR {};
    std::atomic<int> fifoPos { 0 };
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<float> outPeak { 0.f };            // pico linear da saída

private:
    // linha de atraso fracionário (linear, ou Lagrange de 3a ordem no modo HQ)
    struct FracDelay
    {
        static constexpr int size = 4096, mask = size - 1;
        std::array<float, size> buf[2];
        int w = 0;
        void reset() { for (auto& b : buf) b.fill (0.f); w = 0; }
        void push (int ch, float x) { buf[ch][(size_t) w] = x; }
        void advance() { w = (w + 1) & mask; }
        float read (int ch, float d, bool hq) const
        {
            const int di = (int) d;
            const float fd = d - (float) di;
            auto at = [&] (int back) { return buf[ch][(size_t) ((w - back) & mask)]; };
            if (! hq)
                return at (di) * (1.f - fd) + at (di + 1) * fd;
            const float f = 1.f - fd;
            const float y0 = at (di + 2), y1 = at (di + 1), y2 = at (di), y3 = at (di - 1);
            return y0 * (-f * (f - 1.f) * (f - 2.f) / 6.f)
                 + y1 * ((f + 1.f) * (f - 1.f) * (f - 2.f) / 2.f)
                 + y2 * (-(f + 1.f) * f * (f - 2.f) / 2.f)
                 + y3 * ((f + 1.f) * f * (f - 1.f) / 6.f);
        }
    };
    FracDelay delay;

    using Filt = juce::dsp::IIR::Filter<float>;
    Filt bassF[2], trebF[2], cLowF[2], cHighF[2];
    float lpState[2][2] { { 0, 0 }, { 0, 0 } };
    float lastBassDb = 1000.f, lastTrebDb = 1000.f, lastBassHz = 0.f, lastTrebHz = 0.f;
    float lastCLow = 1000.f, lastCHigh = 1000.f;

    juce::SmoothedValue<float> amountSm, inSm, midSm, sideSm, balLSm, balRSm, finalSm, softSm, powerSm;
    juce::Random rng;
    bool wasPlaying = false;
    int currentPreset = 0;

    std::atomic<float> *pAmount, *pAngle, *pBass, *pTreble, *pBassF, *pTrebF, *pBalance, *pOutput,
                       *pComp, *pMid, *pSide, *pSoft, *pDimLvl, *pIn, *pPost, *pRealism,
                       *pMono, *pFlip, *pPol, *pDim, *pPower, *pSafe, *pDither, *pHQ;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenerXAudioProcessor)
};
