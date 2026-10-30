#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout OpenerXAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto f = [&] (const char* id, const char* name, float lo, float hi, float def, float centre = 0.f)
    {
        NormalisableRange<float> range (lo, hi, 0.01f);
        if (centre > 0.f) range.setSkewForCentre (centre);
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def));
    };
    auto b = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    // principais
    f ("amount",       "Amount",         0.f, 150.f, 100.f);
    f ("angle",        "Angle",          0.f,  90.f,  60.f);
    f ("bass",         "Bass Gain",    -12.f,  12.f,   0.f);
    f ("treble",       "Treble Gain",  -12.f,  12.f,   0.f);
    f ("balance",      "L/R Balance", -100.f, 100.f,   0.f);
    f ("output",       "Output Gain",  -24.f,  24.f,   0.f);
    f ("compensation", "Compensation",   0.f, 200.f, 100.f);
    b ("mono",     "Mono",     false);
    b ("flip",     "Flip L/R", false);
    b ("polarity", "Polarity", false);
    b ("dim",      "Dim",      false);
    b ("power",    "Master On/Off", true);

    // avançados
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "realism", 1 }, "Crossfeed Realism",
                     StringArray { "Hyperrealistic", "Realistic", "Standard" }, 0));
    f ("bassfreq",   "Bass Freq",     40.f,   500.f,  250.f,  150.f);
    f ("treblefreq", "Treble Freq", 1000.f, 16000.f, 2000.f, 4000.f);
    f ("midgain",    "Mid Gain",     -12.f,    12.f,    0.f);
    f ("sidegain",   "Side Gain",    -12.f,    12.f,    0.f);
    f ("softstart",  "Soft Start Time", 0.f, 2000.f, 250.f);
    f ("dimlevel",   "Dim Level",    -40.f,     0.f,  -20.f);
    b ("safegain",   "Safe Gain", false);
    b ("dither",     "Dither",    false);
    b ("hq",         "HQ Mode",   false);
    f ("ingain",     "Preprocess Input Gain",   -24.f, 24.f, 0.f);
    f ("postgain",   "Postprocess Output Gain", -24.f, 24.f, 0.f);

    return { p.begin(), p.end() };
}

OpenerXAudioProcessor::OpenerXAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "STATE", createLayout())
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pAmount = g ("amount");   pAngle = g ("angle");     pBass = g ("bass");      pTreble = g ("treble");
    pBassF  = g ("bassfreq"); pTrebF = g ("treblefreq"); pBalance = g ("balance"); pOutput = g ("output");
    pComp   = g ("compensation"); pMid = g ("midgain"); pSide = g ("sidegain"); pSoft = g ("softstart");
    pDimLvl = g ("dimlevel"); pIn = g ("ingain");       pPost = g ("postgain"); pRealism = g ("realism");
    pMono   = g ("mono");     pFlip = g ("flip");       pPol = g ("polarity");  pDim = g ("dim");
    pPower  = g ("power");    pSafe = g ("safegain");   pDither = g ("dither"); pHQ = g ("hq");
}

//==============================================================================
// Shelves de 1a ordem (inclinação suave de 6 dB/oitava, estilo Baxandall), via transformada bilinear
static juce::dsp::IIR::Coefficients<float>::Ptr makeShelf1 (double sr, float fc, float gainLin, bool low)
{
    const float K = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
    const float G = gainLin;
    if (low) return new juce::dsp::IIR::Coefficients<float> (1.f + G * K, G * K - 1.f, 1.f + K, K - 1.f);
    return new juce::dsp::IIR::Coefficients<float> (G + K, K - G, 1.f + K, K - 1.f);
}

//==============================================================================
// Níveis de realismo do modelo de alto-falantes
struct RealismCfg { float cutHi, cutLo; bool useDelay; float leak; int stages; };
static const RealismCfg kRealism[] = {
    { 1800.f,  600.f, true,  0.07f, 2 },   // Hyperrealistic: atraso + sombra da cabeça que varia com o ângulo
    { 1000.f, 1000.f, true,  0.00f, 1 },   // Realistic: atraso simplificado, filtro fixo
    {  700.f,  700.f, false, 0.00f, 1 },   // Standard: crossfeed clássico, sem atraso
};

//==============================================================================
// Presets de fábrica (categorias e valores conforme a página de presets do manual do CanOpener Studio)
using Over = std::vector<std::pair<const char*, float>>;
struct PresetDef { const char* cat; const char* name; Over v; };

static const std::vector<PresetDef>& presetDefs()
{
    static const std::vector<PresetDef> defs = {
        { "Crossfeed", "Default Crossfeed",   {} },
        { "Crossfeed", "Music Before 1970",   { { "amount", 150.f } } },
        { "Crossfeed", "Lifelike",            { { "amount", 150.f }, { "angle", 45.f } } },
        { "Crossfeed", "Modern Music",        { { "amount", 75.f } } },
        { "Crossfeed", "Wide Soundfield",     { { "amount", 50.f }, { "angle", 75.f } } },
        { "Crossfeed", "Crossfeed, No Delay", { { "realism", 2.f } } },
        { "Mixing / Mastering", "Mix Engineer",           { { "safegain", 1.f }, { "dither", 1.f }, { "hq", 1.f } } },
        { "Mixing / Mastering", "Mix Engineer, Lifelike", { { "amount", 150.f }, { "angle", 45.f }, { "safegain", 1.f }, { "dither", 1.f }, { "hq", 1.f } } },
        { "Mixing / Mastering", "Mix Engineer, Flat",     { { "realism", 2.f }, { "safegain", 1.f }, { "dither", 1.f }, { "hq", 1.f } } },
        { "Equalization", "Bass Boost",        { { "bass", 6.f }, { "bassfreq", 80.f } } },
        { "Equalization", "More Air",          { { "treble", 6.f }, { "treblefreq", 12500.f } } },
        { "Equalization", "A Lil' Mo Hi-Fi",   { { "bass", 4.5f }, { "treble", 4.5f }, { "bassfreq", 63.f }, { "treblefreq", 8000.f } } },
        { "Equalization", "Warmth",            { { "bass", 3.f }, { "treble", -1.5f }, { "bassfreq", 350.f } } },
        { "Equalization", "Brighter",          { { "bass", -3.f }, { "treble", 3.f } } },
        { "Equalization", "Darker",            { { "treble", -4.5f } } },
        { "Equalization", "Hear the Mix (Over the Drummer)", { { "bass", -4.5f }, { "treble", 3.f }, { "bassfreq", 100.f }, { "treblefreq", 1000.f } } },
        { "Equalization", "Light on the Bass", { { "bass", -7.5f }, { "bassfreq", 100.f } } },
        { "Monitoring", "Ear Break (Dim)",     { { "amount", 0.f }, { "dim", 1.f } } },
        { "Monitoring", "Solo Left",           { { "amount", 0.f }, { "balance", -100.f } } },
        { "Monitoring", "Solo Right",          { { "amount", 0.f }, { "balance", 100.f } } },
        { "Monitoring", "Mono Sum",            { { "amount", 0.f }, { "mono", 1.f } } },
        { "Monitoring", "Mono Sum to Left",    { { "amount", 0.f }, { "mono", 1.f }, { "balance", -100.f } } },
        { "Monitoring", "Mono Sum to Right",   { { "amount", 0.f }, { "mono", 1.f }, { "balance", 100.f } } },
        { "Monitoring", "Sides Only",          { { "amount", 0.f }, { "mono", 1.f }, { "polarity", 1.f } } },
    };
    return defs;
}

int OpenerXAudioProcessor::getNumPrograms() { return (int) presetDefs().size(); }

const juce::String OpenerXAudioProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String (presetDefs()[(size_t) i].name) : juce::String();
}

juce::String OpenerXAudioProcessor::getPresetCategory (int i)
{
    return juce::isPositiveAndBelow (i, (int) presetDefs().size()) ? juce::String (presetDefs()[(size_t) i].cat) : juce::String();
}

void OpenerXAudioProcessor::resetAllToDefaults()
{
    undoManager.beginNewTransaction();
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (rp->paramID != "power")
                rp->setValueNotifyingHost (rp->getDefaultValue());
}

void OpenerXAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentPreset = index;
    resetAllToDefaults();
    for (const auto& kv : presetDefs()[(size_t) index].v)
        if (auto* rp = apvts.getParameter (kv.first))
            rp->setValueNotifyingHost (rp->convertTo0to1 (kv.second));
}

//==============================================================================
void OpenerXAudioProcessor::selectSlot (int index)
{
    if (index == abCurrent) return;
    abState[abCurrent] = apvts.copyState();
    abCurrent = index;
    if (abState[index].isValid()) apvts.replaceState (abState[index].createCopy());
    else                          abState[index] = apvts.copyState();
}

void OpenerXAudioProcessor::copyToOtherSlot()
{
    abState[1 - abCurrent] = apvts.copyState();
}

//==============================================================================
bool OpenerXAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void OpenerXAudioProcessor::prepareToPlay (double sr, int)
{
    currentSampleRate = sr;
    delay.reset();
    for (int c = 0; c < 2; ++c)
    {
        bassF[c].reset(); trebF[c].reset(); cLowF[c].reset(); cHighF[c].reset();
        lpState[c][0] = lpState[c][1] = 0.f;
    }
    lastBassDb = lastTrebDb = lastCLow = lastCHigh = 1000.f;   // força recálculo
    lastBassHz = lastTrebHz = 0.f;

    auto init = [sr] (juce::SmoothedValue<float>& s, float v, double secs = 0.03)
    {
        s.reset (sr, secs);
        s.setCurrentAndTargetValue (v);
    };
    init (amountSm, *pAmount * 0.01f);
    init (inSm,   juce::Decibels::decibelsToGain (pIn->load()));
    init (midSm,  juce::Decibels::decibelsToGain (pMid->load()));
    init (sideSm, juce::Decibels::decibelsToGain (pSide->load()));
    init (balLSm, 1.f);
    init (balRSm, 1.f);
    init (finalSm, juce::Decibels::decibelsToGain (pOutput->load()));
    init (softSm, 1.f);
    init (powerSm, pPower->load() > 0.5f ? 1.f : 0.f, 0.02);   // liga/desliga sem cliques
    wasPlaying = false;
}

void OpenerXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() < 2) return;

    const int n = buffer.getNumSamples();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    const double sr = getSampleRate();

    // --- Soft Start: rampa de volume quando o transporte começa a tocar ---
    bool playing = wasPlaying;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            playing = pos->getIsPlaying();
    if (playing && ! wasPlaying)
    {
        const float ms = pSoft->load();
        if (ms > 0.5f)
        {
            softSm.reset (sr, (double) ms * 0.001);
            softSm.setCurrentAndTargetValue (0.f);
            softSm.setTargetValue (1.f);
        }
    }
    wasPlaying = playing;

    powerSm.setTargetValue (pPower->load() > 0.5f ? 1.f : 0.f);
    const bool needProcess = powerSm.isSmoothing() || powerSm.getTargetValue() > 0.5f;

    if (needProcess)
    {
        const bool hq = pHQ->load() > 0.5f;
        const auto& M = kRealism[juce::jlimit (0, 2, (int) pRealism->load())];

        // --- crossfeed: atraso interaural (Woodworth) + sombra da cabeça ---
        const float angleDeg = pAngle->load();
        const float th = juce::degreesToRadians (angleDeg);
        const float delaySamples = juce::jmax (2.f, (float) (0.0875 / 343.0 * (th + std::sin (th)) * sr));
        const float fc = juce::jmap (angleDeg, 0.f, 90.f, M.cutHi, M.cutLo);
        const float lpA = 1.f - std::exp (-juce::MathConstants<float>::twoPi * fc / (float) sr);

        // --- equalizador (shelves de 1a ordem) ---
        const float nyq = (float) (0.45 * sr);
        const float bassDb = pBass->load(),  trebDb = pTreble->load();
        const float bassHz = juce::jmin (pBassF->load(), nyq), trebHz = juce::jmin (pTrebF->load(), nyq);
        if (bassDb != lastBassDb || bassHz != lastBassHz)
        {
            auto c = makeShelf1 (sr, bassHz, juce::Decibels::decibelsToGain (bassDb), true);
            for (auto& f : bassF) f.coefficients = c;
            lastBassDb = bassDb; lastBassHz = bassHz;
        }
        if (trebDb != lastTrebDb || trebHz != lastTrebHz)
        {
            auto c = makeShelf1 (sr, trebHz, juce::Decibels::decibelsToGain (trebDb), false);
            for (auto& f : trebF) f.coefficients = c;
            lastTrebDb = trebDb; lastTrebHz = trebHz;
        }

        // --- Compensation: compensação de sonoridade conforme o Output Gain (inativa em 0 dB) ---
        const float outDb = pOutput->load();
        const float cAmt = pComp->load() * 0.01f;
        const float cLowDb  = juce::jlimit (-12.f, 12.f, -outDb * 0.30f * cAmt);
        const float cHighDb = juce::jlimit (-12.f, 12.f, -outDb * 0.10f * cAmt);
        if (std::abs (cLowDb - lastCLow) > 0.01f)
        {
            auto c = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 150.f, 0.5f, juce::Decibels::decibelsToGain (cLowDb));
            for (auto& f : cLowF) f.coefficients = c;
            lastCLow = cLowDb;
        }
        if (std::abs (cHighDb - lastCHigh) > 0.01f)
        {
            auto c = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, std::min (6000.f, nyq), 0.5f, juce::Decibels::decibelsToGain (cHighDb));
            for (auto& f : cHighF) f.coefficients = c;
            lastCHigh = cHighDb;
        }

        // --- ganhos ---
        const float midLin = juce::Decibels::decibelsToGain (pMid->load());
        const float sideLin = juce::Decibels::decibelsToGain (pSide->load());
        const float inLin = juce::Decibels::decibelsToGain (pIn->load());
        const float bal = pBalance->load() * 0.01f;
        const float dimLin = pDim->load() > 0.5f ? juce::Decibels::decibelsToGain (pDimLvl->load()) : 1.f;
        float finalLin = juce::Decibels::decibelsToGain (outDb) * dimLin * juce::Decibels::decibelsToGain (pPost->load());

        // Safe Gain: se o ganho total máximo estimado passar de 0 dB, abaixa o volume
        if (pSafe->load() > 0.5f)
        {
            const float eqMax = std::max (1.f, juce::Decibels::decibelsToGain (bassDb))
                              * std::max (1.f, juce::Decibels::decibelsToGain (trebDb))
                              * std::max (1.f, juce::Decibels::decibelsToGain (cLowDb))
                              * std::max (1.f, juce::Decibels::decibelsToGain (cHighDb));
            const float bound = inLin * (1.f + M.leak) * std::max (midLin, sideLin) * eqMax * finalLin;
            if (bound > 1.f) finalLin /= bound;
        }

        amountSm.setTargetValue (pAmount->load() * 0.01f);
        inSm.setTargetValue (inLin);
        midSm.setTargetValue (midLin);
        sideSm.setTargetValue (sideLin);
        balLSm.setTargetValue (bal > 0.f ? 1.f - bal : 1.f);
        balRSm.setTargetValue (bal < 0.f ? 1.f + bal : 1.f);
        finalSm.setTargetValue (finalLin);

        const bool mono = pMono->load() > 0.5f, flip = pFlip->load() > 0.5f, pol = pPol->load() > 0.5f;

        auto lp2 = [&] (int ch, float x)
        {
            lpState[ch][0] += lpA * (x - lpState[ch][0]);
            lpState[ch][1] += lpA * (lpState[ch][0] - lpState[ch][1]);
            const float lo = (M.stages == 2) ? lpState[ch][1] : lpState[ch][0];
            return lo + M.leak * (x - lo);
        };

        for (int i = 0; i < n; ++i)
        {
            const float l0 = L[i], r0 = R[i];
            const float inG = inSm.getNextValue();
            float l = l0 * inG, r = r0 * inG;

            if (flip) std::swap (l, r);
            if (pol)  r = -r;                       // Polarity inverte apenas o canal direito
            if (mono) l = r = 0.5f * (l + r);

            // crossfeed
            const float g  = amountSm.getNextValue() * 0.5f;   // 100% = -6 dB cruzado
            const float dg = 1.f / (1.f + g);                  // mantém o nível do conteúdo central
            const float xR = lp2 (0, r);                       // R -> ouvido esquerdo
            const float xL = lp2 (1, l);                       // L -> ouvido direito
            float cL = xR, cR = xL;
            delay.push (0, xR);
            delay.push (1, xL);
            if (M.useDelay)
            {
                cL = delay.read (0, delaySamples, hq);
                cR = delay.read (1, delaySamples, hq);
            }
            delay.advance();

            float oL = (l + g * cL) * dg;
            float oR = (r + g * cR) * dg;

            // Mid / Side
            const float mG = midSm.getNextValue(), sG = sideSm.getNextValue();
            const float mid  = 0.5f * (oL + oR) * mG;
            const float side = 0.5f * (oL - oR) * sG;
            oL = mid + side;
            oR = mid - side;

            // equalizador + compensação
            oL = cHighF[0].processSample (cLowF[0].processSample (trebF[0].processSample (bassF[0].processSample (oL))));
            oR = cHighF[1].processSample (cLowF[1].processSample (trebF[1].processSample (bassF[1].processSample (oR))));

            // balance + ganho de saída
            const float fg = finalSm.getNextValue();
            oL *= balLSm.getNextValue() * fg;
            oR *= balRSm.getNextValue() * fg;

            // Master On/Off sem cliques
            const float pw = powerSm.getNextValue();
            L[i] = l0 + pw * (oL - l0);
            R[i] = r0 + pw * (oR - r0);
        }
    }

    // Soft Start + Dither + medidor de pico
    const bool dither = pDither->load() > 0.5f && needProcess;
    constexpr float lsb = 1.f / 8388608.f;           // 24 bits
    float peak = 0.f;
    for (int i = 0; i < n; ++i)
    {
        const float s = softSm.getNextValue();
        L[i] *= s;  R[i] *= s;
        if (dither)
        {
            L[i] += (rng.nextFloat() - rng.nextFloat()) * lsb;   // TPDF
            R[i] += (rng.nextFloat() - rng.nextFloat()) * lsb;
        }
        peak = std::max ({ peak, std::abs (L[i]), std::abs (R[i]) });
    }
    outPeak.store (std::max (peak, outPeak.load() * 0.92f));

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
