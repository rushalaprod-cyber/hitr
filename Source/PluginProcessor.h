#pragma once
#include <JuceHeader.h>
#include "Modes.h"
#include "DSP.h"
#include "Mastering.h"
#include "PresetManager.h"

class HitrProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    HitrProcessor();
    ~HitrProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "h.i.t.r"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("sobriety"); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo { 400000, 100 };
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets { apvts };
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static const juce::StringArray& divisionNames();
    static float divisionBeats (int index);
    static const juce::StringArray& thermalTypeNames();
    static const juce::StringArray& thermalRateNames();

    // A / B / C / D (comme ravenn)
    void selectSlot (int slot);
    void copySlotTo (int dest);
    bool slotHasState (int slot) const;
    int getABSlot() const { return abSlot; }

    // tire tout au sort, sauf le temps (sync, division, écho, trip) et la sortie
    void randomize();
    // couche de mélange : ajoute / retire une substance (clic droit / Maj+clic)
    void toggleLayer (int modeIndex);
    void setParam (const juce::String& id, float value);
    // RESET : tous les réglages par défaut (une étape d'undo)
    void resetAll();
    // RESET d'un onglet : 0 TRIP, 1 THERMAL, 2 DYNAMICS, 3 CLIPPER
    void resetPage (int page);

    // pour l'interface
    std::atomic<float> outputLevel { 0.0f }, hostBpm { 120.0f }, delayMsShown { 0.0f };
    std::atomic<float> clipGrDb { 0.0f }, thermalMod { 0.0f }, autoGainDb { 0.0f };
    std::atomic<int> latencyShown { 0 };

private:
    void timerCallback() override;   // LINK IN <-> CLIPPER (comme ravenn)
    void setParamValueQuiet (const juce::String& id, float value);
    float linkIn = 0.f, linkClip = 0.f;
    bool linkWasOn = false;
    juce::SmoothedValue<float> inSmooth;
    void resetDsp();
    void processChunk (float* L, float* R, int n, double bpm, double ppq, bool playing);
    void updateOversampling();

    double sr = 44100.0;
    int maxBlock = 512;
    hitr::Values cur {}, live {}, step {};

    // moteur des substances
    hitr::PitchShifter pitch[2], shimmerShift[2];
    hitr::DelayLine wowDl[2], chorusDl[2], flangeDl[2], echoDl[2];
    hitr::AllpassPhaser phaser[2];
    hitr::AdaaTanh drive[2];
    juce::dsp::StateVariableTPTFilter<float> lowpass, highpass, sideSplit;
    juce::dsp::IIR::Filter<float> lowShelf[2], highShelf[2];
    float lastBass = -100.0f, lastAir = -100.0f;
    hitr::FdnReverb reverb;
    hitr::Granulizer granular;
    hitr::Stutter stutter;
    hitr::Dropout dropout;
    hitr::Wander wander[4];
    float wowPh = 0, chorusPh = 0, flangePh = 0, phaserPh = 0, ringPh = 0, tremPh = 0, panPh = 0, filtPh = 0, thermPh = 0;
    float compEnv = 0.0f, echoLp[2] {}, lastVerb[2] {};
    float crushHold[2] {}, crushCount = 0.0f, phaserCoef = 0.0f, echoDelaySmooth = 0.0f;
    float dcX[2] {}, dcY[2] {};
    float agcIn = 0.0f, agcWet = 0.0f, agcGain = 1.0f;
    juce::SmoothedValue<float> mixSmooth, widthSmooth, grainSmooth;
    bool firstBlock = true;

    // section master
    gdsp::ColorEQ toneEq[2];
    gdsp::OTT ott;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 4> oversamplers;
    int currentOs = -1;
    gdsp::Thermal thermal;
    gdsp::Fattener fattener;
    gdsp::Biquad bodyPeak[2];
    gdsp::BandFilter clipF[2];
    gdsp::DCBlock dc1[2], dc2[2];
    gdsp::Ramp rClip, rBody, rHeat, rFat, rOtt;
    double osRate = 48000.0;

    // sortie
    juce::SmoothedValue<float> outSmooth, bypassMix;
    float finalLimGain = 1.0f;
    double agIn = 0, agOut = 0, autoG = 1.0;
    juce::AudioBuffer<float> dryBuf;
    std::vector<float> latL, latR;   // retard du signal sec (SOBRIÉTÉ sans saut de phase)
    int latW = 0, latency = 0;

    std::array<juce::ValueTree, 4> abState;
    int abSlot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HitrProcessor)
};
