#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout OpenerXAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto f = [&] (const char* id, const char* name, float lo, float hi, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                            NormalisableRange<float> (lo, hi, 0.01f), def));
    };
    auto b = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    f ("amount",  "Amount",  0.f, 100.f, 50.f);
    f ("angle",   "Angle",   0.f, 90.f, 55.f);
    f ("bass",    "Bass",   -12.f, 12.f, 0.f);
    f ("treble",  "Treble", -12.f, 12.f, 0.f);
    f ("balance", "L/R Balance", -100.f, 100.f, 0.f);
    f ("output",  "Output", -24.f, 24.f, 0.f);
    b ("mono", "Mono", false);
    b ("flip", "Flip L/R", false);
    b ("polarity", "Polarity", false);
    b ("dim", "Dim", false);
    b ("power", "Power", true);

    return { p.begin(), p.end() };
}

OpenerXAudioProcessor::OpenerXAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pAmount   = apvts.getRawParameterValue ("amount");
    pAngle    = apvts.getRawParameterValue ("angle");
    pBass     = apvts.getRawParameterValue ("bass");
    pTreble   = apvts.getRawParameterValue ("treble");
    pBalance  = apvts.getRawParameterValue ("balance");
    pOutput   = apvts.getRawParameterValue ("output");
    pMono     = apvts.getRawParameterValue ("mono");
    pFlip     = apvts.getRawParameterValue ("flip");
    pPolarity = apvts.getRawParameterValue ("polarity");
    pDim      = apvts.getRawParameterValue ("dim");
    pPower    = apvts.getRawParameterValue ("power");
}

bool OpenerXAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void OpenerXAudioProcessor::prepareToPlay (double sr, int blockSize)
{
    currentSampleRate = sr;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) blockSize, 2 };
    delay.prepare (spec);
    delay.reset();
    for (int c = 0; c < 2; ++c)
    {
        bassF[c].reset(); trebF[c].reset();
        lpState[c][0] = lpState[c][1] = 0.f;
    }
    lastBass = lastTreble = 1000.f; // força recálculo dos filtros
    amountSm.reset (sr, 0.03);  amountSm.setCurrentAndTargetValue (*pAmount * 0.01f);
    outSm.reset (sr, 0.03);     outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
}

void OpenerXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() < 2) return;

    const int n = buffer.getNumSamples();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    const double sr = getSampleRate();

    if (pPower->load() > 0.5f)
    {
        // --- parâmetros por bloco ---
        const float angleDeg = pAngle->load();
        const float th = juce::degreesToRadians (angleDeg);
        // Woodworth: atraso interaural (cabeça de raio 8,75 cm)
        const float delaySamples = juce::jmax (1.f, (float) (0.0875 / 343.0 * (th + std::sin (th)) * sr));
        delay.setDelay (delaySamples);
        // sombra da cabeça: quanto mais aberto o ângulo, mais escuro o sinal cruzado
        const float fc = juce::jmap (angleDeg, 0.f, 90.f, 1800.f, 600.f);
        const float lpA = 1.f - std::exp (-juce::MathConstants<float>::twoPi * fc / (float) sr);

        const float bassDb = pBass->load(), trebDb = pTreble->load();
        if (bassDb != lastBass)
        {
            auto c = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 250.f, 0.707f, juce::Decibels::decibelsToGain (bassDb));
            for (auto& f : bassF) f.coefficients = c;
            lastBass = bassDb;
        }
        if (trebDb != lastTreble)
        {
            auto c = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 4000.f, 0.707f, juce::Decibels::decibelsToGain (trebDb));
            for (auto& f : trebF) f.coefficients = c;
            lastTreble = trebDb;
        }

        amountSm.setTargetValue (pAmount->load() * 0.01f);
        outSm.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

        const bool mono = pMono->load() > 0.5f, flip = pFlip->load() > 0.5f,
                   pol = pPolarity->load() > 0.5f, dim = pDim->load() > 0.5f;
        const float bal = pBalance->load() * 0.01f;
        const float gL = bal > 0.f ? 1.f - bal : 1.f;
        const float gR = bal < 0.f ? 1.f + bal : 1.f;
        const float dimGain = dim ? 0.1f : 1.f;

        auto lp2 = [&] (int ch, float x)
        {
            lpState[ch][0] += lpA * (x - lpState[ch][0]);
            lpState[ch][1] += lpA * (lpState[ch][0] - lpState[ch][1]);
            return lpState[ch][1];
        };

        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (flip) std::swap (l, r);
            if (pol) { l = -l; r = -r; }
            if (mono) { l = r = 0.5f * (l + r); }

            const float g  = amountSm.getNextValue() * 0.5f;   // até -6 dB cruzado
            const float dg = 1.f / (1.f + g);                  // compensa nível em baixas

            delay.pushSample (0, lp2 (0, r));   // R -> ouvido esquerdo
            delay.pushSample (1, lp2 (1, l));   // L -> ouvido direito
            const float crossToL = delay.popSample (0, delaySamples);
            const float crossToR = delay.popSample (1, delaySamples);

            float oL = (l + g * crossToL) * dg;
            float oR = (r + g * crossToR) * dg;

            oL = trebF[0].processSample (bassF[0].processSample (oL));
            oR = trebF[1].processSample (bassF[1].processSample (oR));

            const float out = outSm.getNextValue() * dimGain;
            L[i] = oL * gL * out;
            R[i] = oR * gR * out;
        }
    }

    // alimenta o display
    int pos = fifoPos.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        fifoL[(size_t) pos] = L[i];
        fifoR[(size_t) pos] = R[i];
        pos = (pos + 1) & (fifoSize - 1);
    }
    fifoPos.store (pos, std::memory_order_release);
}

juce::AudioProcessorEditor* OpenerXAudioProcessor::createEditor()
{
    return new OpenerXAudioProcessorEditor (*this);
}

void OpenerXAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void OpenerXAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenerXAudioProcessor();
}
