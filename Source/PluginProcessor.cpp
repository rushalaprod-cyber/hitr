#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace hitr;

HitrProcessor::HitrProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "HITR", createLayout())
{
    cur = live = neutralValues();
    step.fill (0.0f);
    linkIn = apvts.getRawParameterValue ("in")->load();
    linkClip = apvts.getRawParameterValue ("clip")->load();
    startTimerHz (30);
}

// LINK (repris de ravenn) : IN et CLIPPER bougent en sens inverse quand LINK est actif
void HitrProcessor::timerCallback()
{
    const bool on = apvts.getRawParameterValue ("link")->load() > 0.5f;
    const float in = apvts.getRawParameterValue ("in")->load(), clip = apvts.getRawParameterValue ("clip")->load();
    if (on && linkWasOn)
    {
        const float dIn = in - linkIn, dClip = clip - linkClip;
        const bool inMoved = std::abs (dIn) > 1e-4f, clipMoved = std::abs (dClip) > 1e-4f;
        if (inMoved && ! clipMoved)
            setParamValueQuiet ("clip", juce::jlimit (-15.f, 15.f, clip - dIn));
        else if (clipMoved && ! inMoved)
            setParamValueQuiet ("in", juce::jlimit (-15.f, 15.f, in - dClip));
    }
    linkWasOn = on;
    linkIn = apvts.getRawParameterValue ("in")->load();
    linkClip = apvts.getRawParameterValue ("clip")->load();
}

void HitrProcessor::setParamValueQuiet (const juce::String& id, float value)
{
    if (auto* rp = apvts.getParameter (id))
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

void HitrProcessor::resetPage (int page)
{
    static const juce::StringArray ids[4] = {
        { "grain", "gsize", "gdens", "freeze", "sync", "division", "delayms", "transpose" },
        { "heat", "thermType", "thermMove", "thermRate", "thermTone", "thermMix" },
        { "ott", "ottTime", "ottLow", "ottMid", "ottHigh", "fatten", "fatColor", "tone", "toneLow", "toneAir" },
        { "in", "link", "clip", "clipShape", "clipBand", "clipKnee", "clipMS", "clipXLo", "clipXHi", "body" } };
    if (page < 0 || page > 3) return;
    undo.beginNewTransaction ("Reset onglet");
    for (auto& id : ids[page])
        if (auto* rp = apvts.getParameter (id))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
    undo.beginNewTransaction();
}

// RESET : tout par défaut, en une seule étape d'undo (SOBRIÉTÉ, latence et suréchantillonnage gardés)
void HitrProcessor::resetAll()
{
    undo.beginNewTransaction ("Reset");
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            if (id == "sobriety" || id == "noLatency" || id == "os") continue;
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
    undo.beginNewTransaction();
    presets.setCurrentName (juce::String::fromUTF8 (u8"Défaut"));
}

const juce::StringArray& HitrProcessor::divisionNames()
{
    static const juce::StringArray names { "AUTO", "1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.",
                                           "1/4T", "1/4", "1/4.", "1/2", "1/1" };
    return names;
}

float HitrProcessor::divisionBeats (int index)
{
    static const float beats[] = { 0.0f, 0.125f, 1.0f / 6.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f, 0.75f,
                                   2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 4.0f };
    return beats[juce::jlimit (0, 12, index)];
}

const juce::StringArray& HitrProcessor::thermalTypeNames()
{
    static const juce::StringArray n { "TUBE", "FUZZ", "FOLD", "RECTIFY", "BITS", "MELT" };
    return n;
}

const juce::StringArray& HitrProcessor::thermalRateNames()
{
    static const juce::StringArray n { "1/16", "1/8", "1/4", "1/2", "1 BAR", "2 BARS" };
    return n;
}

juce::AudioProcessorValueTreeState::ParameterLayout HitrProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto mult = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return "x" + String (v, 2); });
    auto db = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; });
    auto hz = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v >= 1000.f ? String (v / 1000.f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; });
    auto fl = [&] (const char* id, const char* name, float lo, float hi, float def, AudioParameterFloatAttributes a, float centre = 0.f)
    {
        NormalisableRange<float> r (lo, hi);
        if (centre > 0.f) r.setSkewForCentre (centre);
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a));
    };
    auto tg = [&] (const char* id, const char* name, bool def) { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto ch = [&] (const char* id, const char* name, const StringArray& c, int def) { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def)); };

    // substances + mélange
    ch ("mode",  "Mode",    modeNames(), 1);
    ch ("mode2", "Layer 2", layerNames(), 0);
    fl ("blend2", "Layer 2 Dose", 0.f, 1.f, 0.5f, pct);
    ch ("mode3", "Layer 3", layerNames(), 0);
    fl ("blend3", "Layer 3 Dose", 0.f, 1.f, 0.5f, pct);

    fl ("dose",   "Dose",   0.f, 1.f, 0.7f, pct);
    fl ("mix",    "Mix",    0.f, 1.f, 1.f, pct);
    fl ("trip",   "Trip",   0.25f, 4.f, 1.f, mult, 1.f);
    fl ("output", "Output", -24.f, 12.f, 0.f, db);
    fl ("width",  "Width",  0.f, 2.f, 1.f, pct);
    fl ("widthFreq", "Width Freq", 20.f, 500.f, 140.f, hz, 120.f);
    tg ("overdose", "Overdose", false);
    tg ("sobriety", "Sobriete", false);

    // granulizer + temps
    fl ("grain", "Grain", 0.f, 1.f, 0.5f, pct);
    fl ("gsize", "Grain Size", 0.25f, 4.f, 1.f, mult, 1.f);
    fl ("gdens", "Grain Density", 0.25f, 4.f, 1.f, mult, 1.f);
    tg ("freeze", "Freeze", false);
    tg ("sync", "Tempo Sync", true);
    ch ("division", "Delay Division", divisionNames(), 0);
    {
        NormalisableRange<float> msRange (0.0f, 2000.0f);
        msRange.setSkewForCentre (300.0f);
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "delayms", 1 }, "Delay Time", msRange, 0.0f,
                         AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 1.0f ? String ("AUTO") : String (roundToInt (v)) + " ms"; })));
    }
    tg ("transpose", "Free Key", false);

    // THERMAL
    fl ("heat", "Heat", 0.f, 1.f, 0.f, pct);
    ch ("thermType", "Thermal Type", thermalTypeNames(), 0);
    fl ("thermMove", "Thermal Move", 0.f, 1.f, 0.3f, pct);
    ch ("thermRate", "Thermal Rate", thermalRateNames(), 2);
    fl ("thermTone", "Thermal Tone", 0.f, 1.f, 0.5f, pct);
    fl ("thermMix", "Thermal Mix", 0.f, 1.f, 1.f, pct);

    // OTT, FATTENER, TONE
    fl ("ott", "OTT Depth", 0.f, 1.f, 0.f, pct);
    fl ("ottTime", "OTT Time", 0.f, 1.f, 0.5f, pct);
    fl ("ottLow", "OTT Low", 0.f, 1.f, 1.f, pct);
    fl ("ottMid", "OTT Mid", 0.f, 1.f, 1.f, pct);
    fl ("ottHigh", "OTT High", 0.f, 1.f, 1.f, pct);
    fl ("fatten", "Fatten", 0.f, 1.f, 0.f, pct);
    fl ("fatColor", "Fatten Color", 0.f, 1.f, 0.5f, pct);
    fl ("tone", "Tone", -6.f, 6.f, 0.f, db);
    fl ("toneLow", "Tone Low", -6.f, 6.f, 0.f, db);
    fl ("toneAir", "Tone Air", -6.f, 6.f, 0.f, db);

    // IN + CLIPPER (ravenn), LINK : on monte l'un, l'autre descend d'autant
    fl ("in", "In", -15.f, 15.f, 0.f, db);
    tg ("link", "Link", false);
    fl ("clip", "Clipper", -15.f, 15.f, 0.f, db);
    ch ("clipShape", "Clip Shape", { "HARD", "SOFT", "TUBE" }, 0);
    ch ("clipBand", "Clip Band", { "FULL", "LOW", "MID", "HIGH" }, 0);
    fl ("clipKnee", "Clip Knee", 0.f, 1.f, 0.44f, pct);
    tg ("clipMS", "Clip M/S", false);
    fl ("clipXLo", "Clip Low|Mid", 50.f, 1000.f, 200.f, hz, 200.f);
    fl ("clipXHi", "Clip Mid|High", 1000.f, 12000.f, 4000.f, hz, 4000.f);
    tg ("body", "Body", false);

    tg ("autoGain", "Auto Gain", false);
    tg ("noLatency", "No Latency", false);
    ch ("os", "Oversampling", { "x1", "x2", "x4", "x8" }, 1);
    return { p.begin(), p.end() };
}

bool HitrProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void HitrProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (32, juce::jmin (samplesPerBlock, 2048));
    for (int c = 0; c < 2; ++c)
    {
        pitch[c].prepare (sr);
        shimmerShift[c].prepare (sr);
        wowDl[c].prepare ((int) (sr * 0.05));
        chorusDl[c].prepare ((int) (sr * 0.05));
        flangeDl[c].prepare ((int) (sr * 0.02));
        echoDl[c].prepare ((int) (sr * 8.5));
    }

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
    lowpass.prepare (spec);   lowpass.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    highpass.prepare (spec);  highpass.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    sideSplit.prepare (spec); sideSplit.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lowpass.setResonance (0.85f);
    highpass.setResonance (0.707f);
    sideSplit.setResonance (0.707f);
    lastBass = lastAir = -100.0f;

    reverb.prepare (sr);
    granular.prepare (sr);
    stutter.prepare (sr);
    dropout.prepare (sr);
    for (auto& e : toneEq) e.reset();
    ott.prepare (sr, 0.5);

    for (int k = 0; k < 4; ++k)
    {
        oversamplers[(size_t) k] = std::make_unique<juce::dsp::Oversampling<float>> (
            2, (size_t) k, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        oversamplers[(size_t) k]->initProcessing ((size_t) maxBlock);
    }
    currentOs = -1;

    for (auto* s : { &mixSmooth, &widthSmooth, &grainSmooth, &outSmooth, &inSmooth })
        s->reset (sr, 0.05);
    inSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("in")->load()));
    bypassMix.reset (sr, 0.03);
    bypassMix.setCurrentAndTargetValue (apvts.getRawParameterValue ("sobriety")->load() > 0.5f ? 1.0f : 0.0f);

    dryBuf.setSize (2, maxBlock);
    latL.assign (8192, 0.0f);
    latR.assign (8192, 0.0f);
    latW = 0;
    resetDsp();
    updateOversampling();
}

void HitrProcessor::updateOversampling()
{
    const bool noLat = apvts.getRawParameterValue ("noLatency")->load() > 0.5f;
    const int osIdx = noLat ? 0 : juce::jlimit (0, 3, (int) apvts.getRawParameterValue ("os")->load());
    if (osIdx == currentOs) return;
    currentOs = osIdx;
    oversamplers[(size_t) osIdx]->reset();
    osRate = sr * (double) (1 << osIdx);
    thermal.prepare (osRate);
    fattener.prepare (osRate);
    for (int c = 0; c < 2; ++c)
    {
        bodyPeak[c].reset(); bodyPeak[c].peak (osRate, 60.0, 0.6, 3.6);
        clipF[c].reset(); clipF[c].lastFs = -1;
        dc1[c].reset(); dc1[c].set (osRate, 8.0);
        dc2[c].reset(); dc2[c].set (osRate, 8.0);
    }
    latency = (int) std::lround (oversamplers[(size_t) osIdx]->getLatencyInSamples());
    latencyShown.store (latency);
    setLatencySamples (latency);
}

void HitrProcessor::resetDsp()
{
    for (int c = 0; c < 2; ++c)
    {
        pitch[c].clear(); shimmerShift[c].clear();
        wowDl[c].clear(); chorusDl[c].clear(); flangeDl[c].clear(); echoDl[c].clear();
        phaser[c].clear(); drive[c].reset();
        lowShelf[c].reset(); highShelf[c].reset();
        echoLp[c] = lastVerb[c] = crushHold[c] = dcX[c] = dcY[c] = 0.0f;
    }
    lowpass.reset(); highpass.reset(); sideSplit.reset();
    reverb.clear();
    granular.clear();
    ott.reset();
    thermal.reset();
    fattener.reset();
    compEnv = 0.0f; finalLimGain = 1.0f;
    agcIn = agcWet = 0.0f; agcGain = 1.0f;
    agIn = agOut = 0; autoG = 1.0;
    echoDelaySmooth = 0.0f;
    firstBlock = true;
}

static inline float lfo (float phase) { return std::sin (twoPi * phase); }
static inline void advance (float& ph, float hz, float sr) { ph += hz / sr; ph -= std::floor (ph); }

// une modulation de délai ne doit jamais désaccorder de plus de ~7 cents
static inline float capDepthMs (float depthMs, float rateHz, float maxDev = 0.004f)
{
    return juce::jmin (depthMs, maxDev / (twoPi * juce::jmax (0.01f, rateHz)) * 1000.0f);
}

void HitrProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    if (numOut == 0 || total == 0) return;
    if (numIn == 1 && numOut > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, total);
    for (int c = 2; c < numOut; ++c) buffer.clear (c, 0, total);

    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    bpm = juce::jlimit (30.0, 300.0, bpm);
    hostBpm.store ((float) bpm);

    updateOversampling();

    float* L = buffer.getWritePointer (0);
    float* R = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    float tmpR[2048];
    for (int pos = 0; pos < total;)
    {
        const int n = juce::jmin (maxBlock, total - pos);
        float* r = R != nullptr ? R + pos : tmpR;
        if (R == nullptr) std::copy (L + pos, L + pos + n, tmpR);
        processChunk (L + pos, r, n, bpm, ppq, playing);
        ppq += (double) n / sr * bpm / 60.0;
        pos += n;
    }
}

void HitrProcessor::processChunk (float* L, float* R, int n, double bpm, double ppq, bool playing)
{
    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const int   modeIdx   = juce::jlimit (0, numModes - 1, (int) P ("mode"));
    const int   layer[2]  = { (int) P ("mode2") - 1, (int) P ("mode3") - 1 };
    const float blend[2]  = { P ("blend2"), P ("blend3") };
    const bool  overdose  = P ("overdose") > 0.5f;
    const float dose      = P ("dose");
    const float trip      = P ("trip") * (overdose ? 1.35f : 1.0f);
    const bool  sync      = P ("sync") > 0.5f;
    const bool  transpose = P ("transpose") > 0.5f;
    const bool  freeze    = P ("freeze") > 0.5f;
    const float gSizeMul  = P ("gsize");
    const float gDensMul  = P ("gdens");
    const int   division  = (int) P ("division");
    const float delayMsP  = P ("delayms");
    const float widthFreq = P ("widthFreq");
    mixSmooth.setTargetValue (P ("mix"));
    outSmooth.setTargetValue (juce::Decibels::decibelsToGain (P ("output")));
    widthSmooth.setTargetValue (P ("width"));
    grainSmooth.setTargetValue (P ("grain") * (overdose ? 1.5f : 1.0f));
    bypassMix.setTargetValue (P ("sobriety") > 0.5f ? 1.0f : 0.0f);

    const float fs = (float) sr;
    const float beatSamples = (float) (60.0 / bpm) * fs;

    // signal sec gardé pour SOBRIÉTÉ et l'AUTO GAIN (avant IN : SOBRIÉTÉ = vraiment le son d'origine)
    for (int i = 0; i < n; ++i) { dryBuf.setSample (0, i, L[i]); dryBuf.setSample (1, i, R[i]); }

    // IN (comme ravenn) : gain d'entrée, tout le reste réagit à ce qu'on envoie
    inSmooth.setTargetValue (juce::Decibels::decibelsToGain (P ("in")));
    for (int i = 0; i < n; ++i) { const float g = inSmooth.getNextValue(); L[i] *= g; R[i] *= g; }

    auto quantise = [&] (float hz, float& beatsOut)
    {
        static const float divs[] = { 0.125f, 0.25f, 0.375f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f,
                                      6.0f, 8.0f, 12.0f, 16.0f, 24.0f, 32.0f, 48.0f, 64.0f };
        const float beats = (float) (bpm / 60.0) / juce::jmax (0.001f, hz);
        float best = divs[0], bestErr = 1.0e9f;
        for (float d : divs)
        {
            const float e = std::abs (std::log (beats / d));
            if (e < bestErr) { bestErr = e; best = d; }
        }
        beatsOut = best;
        return (float) (bpm / 60.0) / best;
    };

    // ---- substances fusionnées, dosées (OVERDOSE pousse au-delà), lissées ----
    int layers[2]; float weights[2];
    for (int k = 0; k < 2; ++k)
    {
        layers[k] = (layer[k] >= 0 && layer[k] < numModes && layer[k] != modeIdx) ? layer[k] : -1;
        weights[k] = blend[k];
    }
    const Values merged = mergeModes (modeIdx, layers, weights, 2);
    const auto neutral = neutralValues();
    const float doseEff = dose * (overdose ? 1.7f : 1.0f);
    const float k = firstBlock ? 1.0f : 1.0f - std::exp (-(float) n / (0.3f * fs));
    for (int i = 0; i < NUM_P; ++i)
    {
        float target = isDoseScaled (i) ? neutral[(size_t) i] + (merged[(size_t) i] - neutral[(size_t) i]) * doseEff
                                        : merged[(size_t) i];
        if (overdose)
        {
            if (i == CHAOS)   target += 0.35f;
            if (i == STUTTER) target += 0.15f;
            if (i == SHIMMER) target += 0.15f;
        }
        target = clampValue (i, target);
        cur[(size_t) i] += (target - cur[(size_t) i]) * k;
        if (firstBlock) live[(size_t) i] = cur[(size_t) i];
        step[(size_t) i] = (cur[(size_t) i] - live[(size_t) i]) / (float) n;
    }
    firstBlock = false;
    auto V = [this] (int i) { return live[(size_t) i]; };

    if (std::abs (cur[BASS] - lastBass) > 0.05f)
    {
        lastBass = cur[BASS];
        auto co = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 160.0f, 0.7f, juce::Decibels::decibelsToGain (lastBass));
        lowShelf[0].coefficients = co; lowShelf[1].coefficients = co;
    }
    if (std::abs (cur[AIR] - lastAir) > 0.05f)
    {
        lastAir = cur[AIR];
        auto co = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 7000.0f, 0.7f, juce::Decibels::decibelsToGain (lastAir));
        highShelf[0].coefficients = co; highShelf[1].coefficients = co;
    }
    reverb.setParams (cur[VERB_SIZE], cur[VERB_DAMP]);
    highpass.setCutoffFrequency (juce::jmin (20.0f * std::pow (50.0f, cur[HP]), fs * 0.45f));
    sideSplit.setCutoffFrequency (juce::jlimit (20.0f, 1000.0f, widthFreq));

    float tremBeats = 0, panBeats = 0, filtBeats = 0;
    const float tremHz = sync ? quantise (cur[TREM_RATE] * trip, tremBeats) : cur[TREM_RATE] * trip;
    const float panHz  = sync ? quantise (cur[PAN_RATE]  * trip, panBeats)  : cur[PAN_RATE]  * trip;
    const float filtHz = sync ? quantise (cur[FLFO_RATE] * trip, filtBeats) : cur[FLFO_RATE] * trip;
    static const float thermBeats[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
    const float thermB = thermBeats[juce::jlimit (0, 5, (int) P ("thermRate"))];
    const float thermHz = (float) (bpm / 60.0) / thermB;
    if (playing)
    {
        auto lock = [ppq] (float& ph, float beats) { if (beats > 0) { const double x = ppq / beats; ph = (float) (x - std::floor (x)); } };
        if (sync) { lock (tremPh, tremBeats); lock (panPh, panBeats); lock (filtPh, filtBeats); }
        lock (thermPh, thermB);
    }

    float delayMs;
    if (sync)
    {
        float beats = divisionBeats (division);
        if (division == 0)
        {
            float best = 0.5f, bestErr = 1.0e9f;
            const float want = cur[DLY_MS] / 1000.0f * (float) (bpm / 60.0);
            for (int d = 1; d <= 12; ++d)
            {
                const float e = std::abs (std::log (want / divisionBeats (d)));
                if (e < bestErr) { bestErr = e; best = divisionBeats (d); }
            }
            beats = best;
        }
        delayMs = beats * (float) (60000.0 / bpm);
    }
    else
        delayMs = delayMsP >= 1.0f ? delayMsP : cur[DLY_MS];
    delayMs = juce::jlimit (5.0f, 8000.0f, delayMs);
    delayMsShown.store (delayMs);
    const float delayTarget = delayMs * fs / 1000.0f;
    if (echoDelaySmooth <= 0.0f) echoDelaySmooth = delayTarget;

    Granulizer::Settings gs;
    gs.sizeMs = juce::jlimit (10.0f, 1000.0f, cur[G_SIZE] * gSizeMul);
    float gHz = juce::jmax (0.5f, cur[G_DENS] * gDensMul);
    if (sync) { float b; gHz = quantise (gHz, b); }
    gs.intervalSamples = fs / gHz;
    gs.sprayMs = cur[G_SPRAY];
    gs.reverse = cur[G_REV];
    gs.feedback = juce::jlimit (0.0f, 0.85f, cur[G_FB]);
    gs.spread = cur[G_SPREAD];
    gs.pitchSet = juce::jlimit (0, 5, (int) std::round (merged[G_PITCH]));
    gs.transpose = transpose ? cur[PITCH] : 0.0f;
    gs.freeze = freeze;

    //==========================================================================
    // 1) MOTEUR DES SUBSTANCES (échantillon par échantillon)
    for (int s = 0; s < n; ++s)
    {
        for (size_t i = 0; i < (size_t) NUM_P; ++i) live[i] += step[i];

        const float inL = L[s], inR = R[s];
        float x[2] = { inL, inR };
        const float chaos = V (CHAOS);

        const float w0 = wander[0].next (0.7f * trip, fs);
        const float w1 = wander[1].next (1.3f * trip, fs);
        const float w2 = wander[2].next (2.1f * trip, fs);
        const float w3 = wander[3].next (0.4f * trip, fs);
        const float rateMod = juce::jmax (0.05f, 1.0f + chaos * 0.6f * w0);

        if ((s & 15) == 0)
        {
            const float baseLp = 20000.0f * std::pow (0.01f, V (LP));
            const float mod = std::pow (2.0f, (V (FLFO) * 2.5f) * (lfo (filtPh) + chaos * 0.5f * w3) - V (FLFO) * 1.25f);
            lowpass.setCutoffFrequency (juce::jlimit (60.0f, fs * 0.45f, baseLp * mod));
            const float pf = 250.0f * std::pow (2.0f, 3.5f * (0.5f + 0.5f * lfo (phaserPh)));
            const float t = std::tan (pi * juce::jmin (pf, fs * 0.45f) / fs);
            phaserCoef = (1.0f - t) / (1.0f + t);
        }

        const float semis = transpose ? V (PITCH) + chaos * 0.5f * w1 : 0.0f;
        const float pitchBlend = juce::jlimit (0.0f, 1.0f, std::abs (semis) / 0.2f);
        const float ratio = std::pow (2.0f, semis / 12.0f);

        const float wowRate = V (WOW_RATE) * trip * rateMod;
        const float wowDepth = capDepthMs (V (WOW) * 0.35f, wowRate) * fs / 1000.0f;
        const float chRate = 0.45f * trip * rateMod;
        const float chDepth = capDepthMs (2.5f, chRate, 0.0035f) * fs / 1000.0f;
        const float flRate = V (FLANGE_RATE) * trip * rateMod;
        const float flDepth = capDepthMs (3.5f, flRate, 0.006f) * fs / 1000.0f;
        const float crush = V (CRUSH);

        for (int c = 0; c < 2; ++c)
        {
            float v = x[c];
            if (pitchBlend > 0.0f)
            {
                const float ps = pitch[c].process (v, ratio);
                v += (ps - v) * pitchBlend;
            }
            wowDl[c].push (v);
            if (wowDepth > 0.05f)
                v = wowDl[c].read (1.0f + wowDepth * (0.5f + 0.5f * lfo (wowPh + 0.08f * (float) c)));

            chorusDl[c].push (v);
            if (V (CHORUS) > 0.001f)
            {
                const float base = 9.0f * fs / 1000.0f;
                const float a = chorusDl[c].read (base + chDepth * (0.5f + 0.5f * lfo (chorusPh + 0.25f * (float) c)));
                const float b = chorusDl[c].read (base * 1.6f + chDepth * (0.5f + 0.5f * lfo (chorusPh + 0.5f + 0.25f * (float) c)));
                v = v * (1.0f - 0.35f * V (CHORUS)) + (a + b) * 0.4f * V (CHORUS);
            }
            {
                const float d = 0.6f * fs / 1000.0f + flDepth * (0.5f + 0.5f * lfo (flangePh + 0.15f * (float) c));
                const float fl = flangeDl[c].read (d);
                flangeDl[c].push (v + fl * 0.7f * V (FLANGE));
                if (V (FLANGE) > 0.001f)
                    v = v * (1.0f - 0.35f * V (FLANGE)) + fl * 0.65f * V (FLANGE);
            }
            if (V (PHASER) > 0.001f)
            {
                const float ph = phaser[c].process (v, phaserCoef);
                v = v * (1.0f - 0.5f * V (PHASER)) + ph * 0.5f * V (PHASER);
            }
            if (V (RING) > 0.001f)
                v = v * (1.0f - V (RING)) + v * std::sin (twoPi * ringPh) * V (RING) * 1.4f;
            if (V (DRIVE) > 0.001f)
            {
                const float kd = 1.0f + V (DRIVE) * 7.0f;
                v = drive[c].process (v * kd) / kd * (1.0f + V (DRIVE) * 2.0f);
            }
            else drive[c].x1 = 0.0f;
            if (crush > 0.001f)
            {
                const float bits = 16.0f - crush * 12.0f;
                const float stp = std::pow (2.0f, -(bits - 1.0f));
                if (c == 0) crushCount += 1.0f;
                const float factor = 1.0f + crush * 14.0f;
                if (crushCount >= factor)
                {
                    crushHold[c] = std::round (v / stp) * stp;
                    if (c == 1) crushCount -= factor;
                }
                v = v * (1.0f - crush) + crushHold[c] * crush;
            }
            v = highpass.processSample (c, v);
            v = lowpass.processSample (c, v);
            v = lowShelf[c].processSample (v);
            v = highShelf[c].processSample (v);
            x[c] = v;
        }

        {
            const float gm = juce::jlimit (0.0f, 1.0f, V (G_MIX) * grainSmooth.getNextValue() * 2.0f);
            float gl = 0.0f, gr = 0.0f;
            granular.process (x[0], x[1], gl, gr, gs);
            if (gm > 0.0005f)
            {
                const float dryKeep = freeze ? 1.0f - gm : 1.0f - 0.5f * gm;
                x[0] = x[0] * dryKeep + gl * gm;
                x[1] = x[1] * dryKeep + gr * gm;
            }
        }

        if (V (COMP) > 0.001f)
        {
            const float lev = juce::jmax (std::abs (x[0]), std::abs (x[1]));
            const float coef = lev > compEnv ? 1.0f - std::exp (-1.0f / (0.003f * fs)) : 1.0f - std::exp (-1.0f / (0.12f * fs));
            compEnv += (lev - compEnv) * coef;
            const float levDb = juce::Decibels::gainToDecibels (compEnv + 1.0e-6f);
            const float thr = -6.0f - 30.0f * V (COMP);
            const float ratioC = 1.0f + V (COMP) * 11.0f;
            const float over = levDb - thr;
            const float knee = 6.0f;
            float gr = 0.0f;
            if (over > knee * 0.5f) gr = over * (1.0f - 1.0f / ratioC);
            else if (over > -knee * 0.5f) { const float o = over + knee * 0.5f; gr = (1.0f - 1.0f / ratioC) * o * o / (2.0f * knee); }
            const float g = juce::Decibels::decibelsToGain (V (COMP) * 8.0f - gr);
            x[0] *= g; x[1] *= g;
        }

        stutter.process (x[0], x[1], V (STUTTER), chaos, sync ? beatSamples : 0.0f);

        {
            echoDelaySmooth += (delayTarget - echoDelaySmooth) * (1.0f / (0.08f * fs));
            const float d = juce::jlimit (2.0f, (float) echoDl[0].size() - 8.0f, echoDelaySmooth);
            const float eL = echoDl[0].read (d), eR = echoDl[1].read (d);
            echoLp[0] += (eR - echoLp[0]) * 0.35f;
            echoLp[1] += (eL - echoLp[1]) * 0.35f;
            const float fb = juce::jlimit (0.0f, 0.9f, V (DLY_FB));
            echoDl[0].push (x[0] + echoLp[0] * fb);
            echoDl[1].push (x[1] + echoLp[1] * fb);
            x[0] += eL * V (DLY);
            x[1] += eR * V (DLY);
        }

        {
            float vl = x[0], vr = x[1];
            if (V (SHIMMER) > 0.001f)
            {
                vl += std::tanh (shimmerShift[0].process (lastVerb[0], 2.0f)) * V (SHIMMER) * 0.55f;
                vr += std::tanh (shimmerShift[1].process (lastVerb[1], 2.0f)) * V (SHIMMER) * 0.55f;
            }
            float ol, orr;
            reverb.process (vl, vr, ol, orr);
            lastVerb[0] = ol; lastVerb[1] = orr;
            const float wet = juce::jmax (V (VERB), V (SHIMMER) * 0.6f);
            x[0] = x[0] * (1.0f - 0.45f * wet) + ol * wet;
            x[1] = x[1] * (1.0f - 0.45f * wet) + orr * wet;
        }

        if (V (TREM) > 0.001f)
        {
            const float g = 1.0f - V (TREM) * (0.5f + 0.5f * lfo (tremPh));
            x[0] *= g; x[1] *= g;
        }
        if (V (PAN) > 0.001f)
        {
            const float pn = juce::jlimit (-1.0f, 1.0f, V (PAN) * (lfo (panPh) * (1.0f - chaos) + w2 * chaos));
            const float a = (pn + 1.0f) * pi * 0.25f;
            x[0] *= std::cos (a) * juce::MathConstants<float>::sqrt2;
            x[1] *= std::sin (a) * juce::MathConstants<float>::sqrt2;
        }
        {
            const float g = dropout.next (V (DROPOUT));
            x[0] *= g; x[1] *= g;
        }

        // WIDTH : largeur du mode x WIDTH ; sous WIDTH FREQ les basses ne sont jamais élargies
        {
            const float wv = juce::jlimit (0.0f, 3.0f, V (WIDTH) * widthSmooth.getNextValue());
            const float m = 0.5f * (x[0] + x[1]);
            const float side = 0.5f * (x[0] - x[1]);
            const float sideLow = sideSplit.processSample (0, side);
            const float sd = (side - sideLow) * wv + sideLow * juce::jmin (1.0f, wv);
            x[0] = m + sd; x[1] = m - sd;
        }

        // niveau des substances (largeur comprise) : même volume perçu d'un mode ou d'un mélange à l'autre
        {
            const float a = 1.0f / (0.4f * fs);
            agcIn  += (0.5f * (inL * inL + inR * inR) - agcIn) * a;
            agcWet += (0.5f * (x[0] * x[0] + x[1] * x[1]) - agcWet) * a;
            if (agcIn > 1.0e-6f && agcWet > 1.0e-9f)
            {
                const float want = juce::jlimit (0.1f, 8.0f, std::sqrt (agcIn / agcWet));
                agcGain += (want - agcGain) * (1.0f / (0.15f * fs));
            }
            x[0] *= agcGain; x[1] *= agcGain;
        }

        const float mx = mixSmooth.getNextValue();
        const float dryG = std::cos (mx * pi * 0.5f), wetG = std::sin (mx * pi * 0.5f);
        float o[2] = { inL * dryG + x[0] * wetG, inR * dryG + x[1] * wetG };
        if (! std::isfinite (o[0]) || ! std::isfinite (o[1])) { o[0] = o[1] = 0.0f; resetDsp(); }
        for (int c = 0; c < 2; ++c)
        {
            const float y = o[c] - dcX[c] + 0.9997f * dcY[c];
            dcX[c] = o[c]; dcY[c] = y; o[c] = y;
        }
        L[s] = o[0]; R[s] = o[1];

        advance (wowPh,    wowRate, fs);
        advance (chorusPh, chRate, fs);
        advance (flangePh, flRate, fs);
        advance (phaserPh, V (PHASER_RATE) * trip * rateMod, fs);
        advance (ringPh,   V (RING_HZ) * (1.0f + chaos * 0.3f * w1), fs);
        advance (tremPh,   sync ? tremHz : tremHz * rateMod, fs);
        advance (panPh,    sync ? panHz : panHz * rateMod, fs);
        advance (filtPh,   sync ? filtHz : filtHz * rateMod, fs);
    }

    //==========================================================================
    // 2) TONE + OTT (fréquence d'origine)
    {
        const double tTilt = P ("tone"), tLow = P ("toneLow"), tAir = P ("toneAir");
        const bool toneOn = std::abs (tTilt) > 0.01 || std::abs (tLow) > 0.01 || std::abs (tAir) > 0.01;
        for (auto& e : toneEq) e.set (sr, tTilt, tLow, tAir);
        rOtt.set (P ("ott"), n);
        ott.setTime (sr, P ("ottTime"));
        const double bandAmt[3] = { P ("ottLow"), P ("ottMid"), P ("ottHigh") };
        for (int s = 0; s < n; ++s)
        {
            double x[2] = { L[s], R[s] };
            if (toneOn) { x[0] = toneEq[0].process (x[0]); x[1] = toneEq[1].process (x[1]); }
            const double depth = rOtt.next();
            if (depth > 0.0005) ott.process (x, 2, depth, bandAmt);
            L[s] = (float) x[0]; R[s] = (float) x[1];
        }
        rOtt.finish();
    }

    //==========================================================================
    // 3) THERMAL -> FATTENER -> BODY -> CLIPPER, suréchantillonnés (NO LATENCY = x1)
    {
        float* chans[2] = { L, R };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        auto& os = *oversamplers[(size_t) currentOs];
        auto up = os.processSamplesUp (block);
        const int un = (int) up.getNumSamples();
        float* uL = up.getChannelPointer (0);
        float* uR = up.getChannelPointer (1);

        const double heatP = juce::jlimit (0.0f, 1.0f, P ("heat") + (overdose ? 0.35f : 0.0f));
        const int thType = juce::jlimit (0, 5, (int) P ("thermType"));
        const double tMove = P ("thermMove"), tMix = P ("thermMix");
        thermal.setTone (P ("thermTone"));
        const double fatAmt = P ("fatten");
        fattener.set (fatAmt, P ("fatColor"));
        const double clipGain = juce::Decibels::decibelsToGain ((double) P ("clip"));
        const int clipShape = (int) P ("clipShape"), clipBand = (int) P ("clipBand");
        const bool clipMS = P ("clipMS") > 0.5f;
        const double softK = 1.0 - 0.9 * P ("clipKnee");
        const double xl = P ("clipXLo"), xh = juce::jmax ((double) P ("clipXHi"), xl * 1.5);
        const double cLo[4] = { 0.0, 0.0, xl, xh }, cHi[4] = { 0.0, xl, xh, 0.0 };
        for (auto& f : clipF) f.set (osRate, cLo[clipBand], cHi[clipBand]);
        rClip.set (clipGain, un); rBody.set (P ("body") > 0.5f ? 1.0 : 0.0, un);
        rHeat.set (heatP, un); rFat.set (fatAmt, un);

        auto clipFn = [&] (double v)
        {
            switch (clipShape)
            {
                case 1:  return gdsp::shape::soft (v, softK);
                case 2:  return gdsp::shape::analog (v);
                default: return gdsp::shape::hard (v);
            }
        };

        const double tInc = (double) thermHz / osRate;
        double grPre = 0, grPost = 0;
        double modShow = 0;
        for (int i = 0; i < un; ++i)
        {
            double x[2] = { uL[i], uR[i] };
            const double h = rHeat.next(), fa = rFat.next(), cg = rClip.next(), bo = rBody.next();

            {
                // toujours traités (même à 0) : leur rattrapage de niveau est prêt avant qu'on les monte
                const double lf = std::sin (twoPi * thermPh);
                thermal.process (x, 2, h, thType, tMove, lf, tMix);
                if (h > 0.0005) modShow = lf * tMove;
            }
            thermPh += (float) tInc; thermPh -= std::floor (thermPh);

            fattener.process (x, 2, fa);

            if (bo > 0.0005)
            {
                for (int c = 0; c < 2; ++c) x[c] += bo * (bodyPeak[c].process (x[c]) - x[c]);
                const double M = 0.5 * (x[0] + x[1]), S = 0.5 * (x[0] - x[1]) * (1.0 + 0.122 * bo);
                x[0] = M + S; x[1] = M - S;
            }

            if (clipMS && clipBand == 0)
            {
                const double l = dc1[0].process (x[0]) * cg, r = dc1[1].process (x[1]) * cg;
                grPre = std::max (grPre, std::max (std::abs (l), std::abs (r)));
                const double M = clipFn (0.5 * (l + r)), S = clipFn (0.5 * (l - r));
                x[0] = M + S; x[1] = M - S;
                for (int c = 0; c < 2; ++c)
                {
                    if (clipShape == 2) x[c] = dc2[c].process (x[c]);
                    grPost = std::max (grPost, std::abs (x[c]));
                }
            }
            else
            {
                for (int c = 0; c < 2; ++c)
                {
                    double v = dc1[c].process (x[c]);
                    if (clipBand == 0)
                    {
                        v *= cg;
                        grPre = std::max (grPre, std::abs (v));
                        v = clipFn (v);
                        grPost = std::max (grPost, std::abs (v));
                    }
                    else
                    {
                        const double b = clipF[c].process (v);
                        const double bd = b * cg, cb = clipFn (bd);
                        grPre = std::max (grPre, std::abs (bd));
                        grPost = std::max (grPost, std::abs (cb));
                        v += cb / cg - b;
                    }
                    if (clipShape == 2) v = dc2[c].process (v);
                    x[c] = v;
                }
            }
            uL[i] = (float) x[0]; uR[i] = (float) x[1];
        }
        rHeat.finish(); rFat.finish(); rClip.finish(); rBody.finish();
        os.processSamplesDown (block);

        const float gr = grPre > 1.0e-6 ? (float) juce::Decibels::gainToDecibels (grPost / grPre) : 0.0f;
        clipGrDb.store (juce::jmin (0.0f, gr));
        thermalMod.store ((float) modShow);
    }

    //==========================================================================
    // 4) SORTIE : gain, AUTO GAIN, garde-fou, SOBRIÉTÉ (fondu, latence compensée)
    {
        const bool autoGain = P ("autoGain") > 0.5f;
        const double ag = 1.0 - std::exp (-1.0 / (1.2 * sr));
        const int latSize = (int) latL.size();
        double sumSq = 0.0;
        for (int s = 0; s < n; ++s)
        {
            const float dL = dryBuf.getSample (0, s), dR = dryBuf.getSample (1, s);
            const float og = outSmooth.getNextValue();
            float o[2] = { L[s] * og, R[s] * og };

            agIn  += ag * (0.5 * (dL * dL + dR * dR) - agIn);
            agOut += ag * (0.5 * (o[0] * o[0] + o[1] * o[1]) - agOut);
            if (autoGain && agIn > 1e-9 && agOut > 1e-9)
                autoG += (1.0 / (0.3 * sr)) * (juce::jlimit (0.0625, 16.0, std::sqrt (agIn / agOut)) - autoG);
            else if (! autoGain)
                autoG += (1.0 / (0.05 * sr)) * (1.0 - autoG);
            o[0] *= (float) autoG; o[1] *= (float) autoG;

            const float pk = juce::jmax (std::abs (o[0]), std::abs (o[1]));
            const float need = pk * finalLimGain > 0.995f ? 0.995f / pk : 1.0f;
            if (need < finalLimGain) finalLimGain = need;
            else finalLimGain += (1.0f - finalLimGain) * (1.0f / (0.08f * (float) sr));
            o[0] *= finalLimGain; o[1] *= finalLimGain;

            // signal sec retardé de la latence du suréchantillonnage
            latL[(size_t) latW] = dL; latR[(size_t) latW] = dR;
            const int rd = (latW - latency + latSize) % latSize;
            latW = (latW + 1) % latSize;
            const float b = bypassMix.getNextValue();
            o[0] = o[0] * (1.0f - b) + latL[(size_t) rd] * b;
            o[1] = o[1] * (1.0f - b) + latR[(size_t) rd] * b;

            L[s] = juce::jlimit (-1.0f, 1.0f, o[0]);
            R[s] = juce::jlimit (-1.0f, 1.0f, o[1]);
            sumSq += 0.5 * (L[s] * L[s] + R[s] * R[s]);
        }
        autoGainDb.store ((float) juce::Decibels::gainToDecibels (autoG));
        const float rms = (float) std::sqrt (sumSq / (double) n);
        outputLevel.store (juce::jmax (rms, outputLevel.load() * 0.85f));
    }
}

//==============================================================================
void HitrProcessor::setParam (const juce::String& id, float value)
{
    if (auto* rp = apvts.getParameter (id))
    {
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        rp->endChangeGesture();
    }
}

void HitrProcessor::toggleLayer (int m)
{
    const int primary = (int) apvts.getRawParameterValue ("mode")->load();
    if (m == primary) return;
    const int l2 = (int) apvts.getRawParameterValue ("mode2")->load() - 1;
    const int l3 = (int) apvts.getRawParameterValue ("mode3")->load() - 1;
    undo.beginNewTransaction ("Layer");
    if (l2 == m) { setParam ("mode2", 0.f); return; }
    if (l3 == m) { setParam ("mode3", 0.f); return; }
    if (l2 < 0) setParam ("mode2", (float) (m + 1));
    else if (l3 < 0) setParam ("mode3", (float) (m + 1));
    else setParam ("mode3", (float) (m + 1));   // les deux couches sont prises : la 3e est remplacée
}

void HitrProcessor::randomize()
{
    juce::Random r;
    undo.beginNewTransaction ("Randomize");
    auto pick = [&] (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); };
    const int primary = r.nextInt (numModes);
    setParam ("mode", (float) primary);
    setParam ("mode2", r.nextFloat() < 0.55f ? (float) (r.nextInt (numModes) + 1) : 0.f);
    setParam ("blend2", pick (0.2f, 0.8f));
    setParam ("mode3", r.nextFloat() < 0.25f ? (float) (r.nextInt (numModes) + 1) : 0.f);
    setParam ("blend3", pick (0.2f, 0.7f));
    setParam ("dose", pick (0.45f, 1.0f));
    setParam ("grain", pick (0.2f, 0.85f));
    setParam ("gsize", std::pow (2.0f, pick (-1.2f, 1.2f)));
    setParam ("gdens", std::pow (2.0f, pick (-1.0f, 1.0f)));
    setParam ("width", pick (0.7f, 1.6f));
    setParam ("heat", r.nextFloat() < 0.5f ? 0.f : pick (0.1f, 0.6f));
    setParam ("thermType", (float) r.nextInt (6));
    setParam ("thermMove", pick (0.f, 0.8f));
    setParam ("thermTone", pick (0.25f, 0.75f));
    setParam ("fatten", r.nextFloat() < 0.6f ? 0.f : pick (0.05f, 0.4f));
    setParam ("ott", r.nextFloat() < 0.6f ? 0.f : pick (0.1f, 0.45f));
    setParam ("tone", pick (-3.f, 3.f));
    undo.beginNewTransaction();
    presets.setCurrentName (juce::String::fromUTF8 (u8"Aléatoire"));
}

//==============================================================================
void HitrProcessor::copySlotTo (int dest)
{
    if (dest < 0 || dest > 3 || dest == abSlot) return;
    abState[(size_t) dest] = apvts.copyState();
}

bool HitrProcessor::slotHasState (int slot) const
{
    return slot == abSlot || (slot >= 0 && slot < 4 && abState[(size_t) slot].isValid());
}

void HitrProcessor::selectSlot (int slot)
{
    if (slot < 0 || slot > 3 || slot == abSlot) return;
    abState[(size_t) abSlot] = apvts.copyState();
    abSlot = slot;
    if (abState[(size_t) abSlot].isValid())
        apvts.replaceState (abState[(size_t) abSlot].createCopy());
}

juce::AudioProcessorEditor* HitrProcessor::createEditor() { return new HitrEditor (*this); }

void HitrProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("presetName", presets.getCurrent(), nullptr);
    if (auto xml = st.createXml())
        copyXmlToBinary (*xml, dest);
}

void HitrProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            if (st.hasProperty ("presetName")) presets.setCurrentName (st["presetName"].toString());
            apvts.replaceState (st);
            undo.clearUndoHistory();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HitrProcessor(); }
