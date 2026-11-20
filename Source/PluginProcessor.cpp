#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace hitr;

HitrProcessor::HitrProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HITR", createLayout())
{
    cur = live = neutralValues();
    step.fill (0.0f);
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

juce::AudioProcessorValueTreeState::ParameterLayout HitrProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto mult = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return "x" + String (v, 2); });

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", modeNames(), 1));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "dose", 1 }, "Dose", NormalisableRange<float> (0.0f, 1.0f), 0.7f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));

    NormalisableRange<float> centred (0.25f, 4.0f);
    centred.setSkewForCentre (1.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "trip", 1 }, "Trip", centred, 1.0f, mult));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f), 0.0f,
                     AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Width", NormalisableRange<float> (0.0f, 2.0f), 1.0f, pct));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "grain", 1 }, "Grain", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gsize", 1 }, "Grain Size", centred, 1.0f, mult));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gdens", 1 }, "Grain Density", centred, 1.0f, mult));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "freeze", 1 }, "Freeze", false));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Tempo Sync", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Delay Division", divisionNames(), 0));
    NormalisableRange<float> msRange (0.0f, 2000.0f);
    msRange.setSkewForCentre (300.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "delayms", 1 }, "Delay Time", msRange, 0.0f,
                     AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 1.0f ? String ("AUTO") : String (roundToInt (v)) + " ms"; })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "transpose", 1 }, "Free Key", false));
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
    for (int c = 0; c < 2; ++c)
    {
        pitch[c].prepare (sr);
        shimmerShift[c].prepare (sr);
        wowDl[c].prepare ((int) (sr * 0.05));
        chorusDl[c].prepare ((int) (sr * 0.05));
        flangeDl[c].prepare ((int) (sr * 0.02));
        echoDl[c].prepare ((int) (sr * 8.5));
    }

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 2 };
    lowpass.prepare (spec);  lowpass.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    highpass.prepare (spec); highpass.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    lowpass.setResonance (0.85f);
    highpass.setResonance (0.707f);
    lastBass = lastAir = -100.0f;

    reverb.prepare (sr);
    granular.prepare (sr);
    stutter.prepare (sr);
    dropout.prepare (sr);

    for (auto* s : { &mixSmooth, &outSmooth, &widthSmooth, &grainSmooth })
        s->reset (sr, 0.05);
    resetDsp();
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
    lowpass.reset(); highpass.reset();
    reverb.clear();
    granular.clear();
    compEnv = 0.0f; sideLow = 0.0f; limGain = 1.0f;
    agcIn = agcWet = 0.0f; agcGain = 1.0f;
    echoDelaySmooth = 0.0f;
    firstBlock = true;
}

static inline float lfo (float phase) { return std::sin (twoPi * phase); }
static inline void advance (float& ph, float hz, float sr) { ph += hz / sr; ph -= std::floor (ph); }

// limite la profondeur d'une modulation de délai pour que la hauteur ne bouge
// jamais de plus de ~maxDev (0.004 = ~7 cents) : fini le "wobble" qui désaccorde
static inline float capDepthMs (float depthMs, float rateHz, float maxDev = 0.004f)
{
    const float cap = maxDev / (twoPi * juce::jmax (0.01f, rateHz)) * 1000.0f;
    return juce::jmin (depthMs, cap);
}

void HitrProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    if (numOut == 0 || n == 0) return;
    if (numIn == 1 && numOut > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const int   modeIdx   = juce::jlimit (0, 24, (int) P ("mode"));
    const float dose      = P ("dose");
    const float trip      = P ("trip");
    const bool  sync      = P ("sync") > 0.5f;
    const bool  transpose = P ("transpose") > 0.5f;
    const bool  freeze    = P ("freeze") > 0.5f;
    const float gSizeMul  = P ("gsize");
    const float gDensMul  = P ("gdens");
    const int   division  = (int) P ("division");
    const float delayMsP  = P ("delayms");
    mixSmooth.setTargetValue (P ("mix"));
    outSmooth.setTargetValue (juce::Decibels::decibelsToGain (P ("output")));
    widthSmooth.setTargetValue (P ("width"));
    grainSmooth.setTargetValue (P ("grain"));

    // ---- tempo de l'hôte ----------------------------------------------------
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
    const float fs = (float) sr;
    const float beatSamples = (float) (60.0 / bpm) * fs;

    // fréquence -> division musicale la plus proche (en noires)
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

    // ---- cibles du mode, dosées, lissées puis interpolées échantillon par échantillon
    const auto& mode = modes()[(size_t) modeIdx];
    const auto neutral = neutralValues();
    const float k = firstBlock ? 1.0f : 1.0f - std::exp (-(float) n / (0.3f * fs));
    for (size_t i = 0; i < (size_t) NUM_P; ++i)
    {
        const float target = isDoseScaled ((int) i) ? neutral[i] + (mode.values[i] - neutral[i]) * dose : mode.values[i];
        cur[i] += (target - cur[i]) * k;
        if (firstBlock) live[i] = cur[i];
        step[i] = (cur[i] - live[i]) / (float) n;
    }
    firstBlock = false;
    auto V = [this] (int i) { return live[(size_t) i]; };

    // ---- paramètres lents (par bloc) -----------------------------------------
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

    // LFOs rythmiques : calés sur le tempo (et sur la position de lecture) en mode BPM
    float tremBeats = 0, panBeats = 0, filtBeats = 0;
    const float tremHz = sync ? quantise (cur[TREM_RATE] * trip, tremBeats) : cur[TREM_RATE] * trip;
    const float panHz  = sync ? quantise (cur[PAN_RATE]  * trip, panBeats)  : cur[PAN_RATE]  * trip;
    const float filtHz = sync ? quantise (cur[FLFO_RATE] * trip, filtBeats) : cur[FLFO_RATE] * trip;
    if (sync && playing)
    {
        auto lock = [ppq] (float& ph, float beats) { if (beats > 0) { const double x = ppq / beats; ph = (float) (x - std::floor (x)); } };
        lock (tremPh, tremBeats); lock (panPh, panBeats); lock (filtPh, filtBeats);
    }

    // temps de l'écho : division musicale (BPM) ou millisecondes (MS)
    float delayMs;
    if (sync)
    {
        float beats = divisionBeats (division);
        if (division == 0)   // AUTO : la division la plus proche du réglage du mode
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

    // granulizer
    Granulizer::Settings gs;
    gs.sizeMs = juce::jlimit (10.0f, 1000.0f, cur[G_SIZE] * gSizeMul);
    float gHz = juce::jmax (0.5f, cur[G_DENS] * gDensMul);
    if (sync) { float b; gHz = quantise (gHz, b); }
    gs.intervalSamples = fs / gHz;
    gs.sprayMs = cur[G_SPRAY];
    gs.reverse = cur[G_REV];
    gs.feedback = juce::jlimit (0.0f, 0.85f, cur[G_FB]);
    gs.spread = cur[G_SPREAD];
    gs.pitchSet = juce::jlimit (0, 5, (int) std::round (mode.values[G_PITCH]));
    gs.transpose = transpose ? cur[PITCH] : 0.0f;
    gs.freeze = freeze;

    float* L = buffer.getWritePointer (0);
    float* R = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    double sumSq = 0.0;
    const float sideLpCoef = std::exp (-twoPi * 140.0f / fs);

    for (int s = 0; s < n; ++s)
    {
        for (size_t i = 0; i < (size_t) NUM_P; ++i) live[i] += step[i];

        const float inL = L[s];
        const float inR = R ? R[s] : inL;
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

        // transposition : seulement si GAMME LIBRE est activée
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

            // wow très léger (la mélodie respire sans se désaccorder)
            wowDl[c].push (v);
            if (wowDepth > 0.05f)
                v = wowDl[c].read (1.0f + wowDepth * (0.5f + 0.5f * lfo (wowPh + 0.08f * (float) c)));

            // chorus 2 voix
            chorusDl[c].push (v);
            if (V (CHORUS) > 0.001f)
            {
                const float base = 9.0f * fs / 1000.0f;
                const float a = chorusDl[c].read (base + chDepth * (0.5f + 0.5f * lfo (chorusPh + 0.25f * (float) c)));
                const float b = chorusDl[c].read (base * 1.6f + chDepth * (0.5f + 0.5f * lfo (chorusPh + 0.5f + 0.25f * (float) c)));
                v = v * (1.0f - 0.35f * V (CHORUS)) + (a + b) * 0.4f * V (CHORUS);
            }

            // flanger
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

            // saturation anti-aliasée
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

        // ---- granulizer (s'intègre au son de chaque mode) ------------------
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

        // ---- compression ---------------------------------------------------
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

        // ---- écho ping-pong (BPM ou ms) -------------------------------------
        {
            echoDelaySmooth += (delayTarget - echoDelaySmooth) * (1.0f / (0.08f * fs));
            const float d = juce::jlimit (2.0f, (float) echoDl[0].size() - 8.0f, echoDelaySmooth);
            const float eL = echoDl[0].read (d);
            const float eR = echoDl[1].read (d);
            echoLp[0] += (eR - echoLp[0]) * 0.35f;
            echoLp[1] += (eL - echoLp[1]) * 0.35f;
            const float fb = juce::jlimit (0.0f, 0.9f, V (DLY_FB));
            echoDl[0].push (x[0] + echoLp[0] * fb);
            echoDl[1].push (x[1] + echoLp[1] * fb);
            x[0] += eL * V (DLY);
            x[1] += eR * V (DLY);
        }

        // ---- reverb FDN + shimmer -----------------------------------------
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

        // ---- gain auto : même volume perçu dans tous les modes ----------------
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

        // ---- largeur : celle du mode x WIDTH, basses gardées en mono -----------
        {
            const float wGlobal = widthSmooth.getNextValue();
            const float wv = juce::jlimit (0.0f, 3.0f, V (WIDTH) * wGlobal);
            const float m = 0.5f * (x[0] + x[1]);
            const float side = 0.5f * (x[0] - x[1]);
            sideLow = side * (1.0f - sideLpCoef) + sideLow * sideLpCoef;
            const float sd = (side - sideLow) * wv + sideLow * juce::jmin (1.0f, wv);
            x[0] = m + sd; x[1] = m - sd;
        }

        // ---- mix à puissance constante + sortie --------------------------------
        const float mx = mixSmooth.getNextValue();
        const float dryG = std::cos (mx * pi * 0.5f), wetG = std::sin (mx * pi * 0.5f);
        const float og = outSmooth.getNextValue();
        float o[2] = { (inL * dryG + x[0] * wetG) * og, (inR * dryG + x[1] * wetG) * og };

        if (! std::isfinite (o[0]) || ! std::isfinite (o[1])) { o[0] = o[1] = 0.0f; resetDsp(); }

        for (int c = 0; c < 2; ++c)   // anti-DC
        {
            const float y = o[c] - dcX[c] + 0.9997f * dcY[c];
            dcX[c] = o[c]; dcY[c] = y; o[c] = y;
        }

        // limiteur de crête -1 dBFS (attaque instantanée, relâchement 80 ms) + sécurité douce
        {
            const float ceiling = 0.891f;
            const float pk = juce::jmax (std::abs (o[0]), std::abs (o[1]));
            const float need = pk * limGain > ceiling ? ceiling / pk : 1.0f;
            if (need < limGain) limGain = need;
            else limGain += (1.0f - limGain) * (1.0f / (0.08f * fs));
            for (auto& v : o)
            {
                v *= limGain;
                v = juce::jlimit (-0.999f, 0.999f, v);
            }
        }

        L[s] = o[0];
        if (R) R[s] = o[1];
        sumSq += 0.5 * (o[0] * o[0] + o[1] * o[1]);

        advance (wowPh,    wowRate, fs);
        advance (chorusPh, chRate, fs);
        advance (flangePh, flRate, fs);
        advance (phaserPh, V (PHASER_RATE) * trip * rateMod, fs);
        advance (ringPh,   V (RING_HZ) * (1.0f + chaos * 0.3f * w1), fs);
        advance (tremPh,   sync ? tremHz : tremHz * rateMod, fs);
        advance (panPh,    sync ? panHz : panHz * rateMod, fs);
        advance (filtPh,   sync ? filtHz : filtHz * rateMod, fs);
    }

    for (int c = 2; c < numOut; ++c)
        buffer.clear (c, 0, n);

    const float rms = (float) std::sqrt (sumSq / (double) n);
    outputLevel.store (juce::jmax (rms, outputLevel.load() * 0.85f));
}

juce::AudioProcessorEditor* HitrProcessor::createEditor() { return new HitrEditor (*this); }

void HitrProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void HitrProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HitrProcessor(); }
