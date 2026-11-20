#pragma once
#include <JuceHeader.h>
#include "Modes.h"
#include "DSP.h"

class HitrProcessor : public juce::AudioProcessor
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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // valeurs des divisions (en noires) pour le délai synchronisé ; index 0 = AUTO
    static const juce::StringArray& divisionNames();
    static float divisionBeats (int index);

    // pour l'interface
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> hostBpm { 120.0f };
    std::atomic<float> delayMsShown { 0.0f };

private:
    void resetDsp();

    double sr = 44100.0;
    hitr::Values cur {}, live {}, step {};

    hitr::PitchShifter pitch[2], shimmerShift[2];
    hitr::DelayLine wowDl[2], chorusDl[2], flangeDl[2], echoDl[2];
    hitr::AllpassPhaser phaser[2];
    hitr::AdaaTanh drive[2];
    juce::dsp::StateVariableTPTFilter<float> lowpass, highpass;
    juce::dsp::IIR::Filter<float> lowShelf[2], highShelf[2];
    float lastBass = -100.0f, lastAir = -100.0f;
    hitr::FdnReverb reverb;
    hitr::Granulizer granular;
    hitr::Stutter stutter;
    hitr::Dropout dropout;
    hitr::Wander wander[4];

    float wowPh = 0, chorusPh = 0, flangePh = 0, phaserPh = 0, ringPh = 0, tremPh = 0, panPh = 0, filtPh = 0;
    float compEnv = 0.0f, echoLp[2] {}, lastVerb[2] {};
    float crushHold[2] {}, crushCount = 0.0f;
    float phaserCoef = 0.0f;
    float echoDelaySmooth = 0.0f;
    float sideLow = 0.0f;
    float dcX[2] {}, dcY[2] {};
    float limGain = 1.0f;
    float agcIn = 0.0f, agcWet = 0.0f, agcGain = 1.0f;
    juce::SmoothedValue<float> mixSmooth, outSmooth, widthSmooth, grainSmooth;
    bool firstBlock = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HitrProcessor)
};
