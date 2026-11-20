// Test hors DAW : chaque mode traite 6 s de mélodie, on vérifie niveau et stabilité.
#include "../Source/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    // hitrTest --snap <dossier> : capture de l'interface pour chaque mode
    if (argc > 2 && juce::String (argv[1]) == "--snap")
    {
        juce::File dir (juce::String::fromUTF8 (argv[2]));
        dir.createDirectory();
        HitrProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        for (int m = 0; m < 25; ++m)
        {
            auto* modeP = p.apvts.getParameter ("mode");
            modeP->setValueNotifyingHost (modeP->convertTo0to1 ((float) m));
            p.outputLevel.store (0.15f);
            // laisse tourner les timers pour animer le visuel
            for (int i = 0; i < 30; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds());
            juce::File f = dir.getChildFile (juce::String (m + 1).paddedLeft ('0', 2) + ".png");
            f.deleteFile();
            juce::FileOutputStream out (f);
            juce::PNGImageFormat().writeImageToStream (img, out);
        }
        return 0;
    }
    const double sr = 48000.0;
    const int block = 512, seconds = 6;
    const float notes[] = { 440.f, 523.25f, 659.25f, 587.33f, 493.88f, 392.f, 440.f, 329.63f };
    int failures = 0;

    for (int config = 0; config < 2; ++config)
    {
    std::printf (config == 0 ? "== Réglages par défaut (BPM)\n" : "\n== Extrêmes : MS, FREEZE, GRAIN 100%%, WIDTH 200%%, GAMME LIBRE\n");
    for (int m = 0; m < 25; ++m)
    {
        HitrProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        p.prepareToPlay (sr, block);
        auto* modeP = p.apvts.getParameter ("mode");
        modeP->setValueNotifyingHost (modeP->convertTo0to1 ((float) m));
        p.apvts.getParameter ("dose")->setValueNotifyingHost (1.0f);
        if (config == 1)
        {
            p.apvts.getParameter ("sync")->setValueNotifyingHost (0.0f);
            p.apvts.getParameter ("grain")->setValueNotifyingHost (1.0f);
            p.apvts.getParameter ("width")->setValueNotifyingHost (1.0f);
            p.apvts.getParameter ("transpose")->setValueNotifyingHost (1.0f);
            p.apvts.getParameter ("gsize")->setValueNotifyingHost (1.0f);
            p.apvts.getParameter ("gdens")->setValueNotifyingHost (1.0f);
        }

        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        double phase = 0.0;
        float peak = 0.f; double inE = 0.0, outE = 0.0;
        bool bad = false;
        const int total = (int) (sr * seconds);
        for (int pos = 0; pos < total; pos += block)
        {
            if (config == 1 && pos >= (int) (sr * 1.5) && pos < (int) (sr * 1.5) + block)
                p.apvts.getParameter ("freeze")->setValueNotifyingHost (1.0f);   // on gèle pendant que ça joue
            for (int i = 0; i < block; ++i)
            {
                const int n = ((pos + i) / (int) (sr / 4)) % 8;
                phase += notes[n] / sr;
                const float env = 0.5f;
                const float s = env * (float) std::sin (juce::MathConstants<double>::twoPi * phase);
                buf.setSample (0, i, s); buf.setSample (1, i, s);
                if (pos > sr * 1.5) inE += s * s;   // on mesure après la mise en route
            }
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) bad = true;
                    peak = juce::jmax (peak, std::abs (v));
                    if (pos > sr * 1.5) outE += 0.5 * v * v;
                }
        }
        const double gainDb = 10.0 * std::log10 ((outE + 1e-12) / (inE + 1e-12));
        // les modes à "trous noirs" (Xanax, Datura...) baissent volontairement en moyenne
        const bool holes = hitr::modes()[(size_t) m].values[hitr::DROPOUT] > 0.0f;
        // réglages extrêmes : le limiteur -1 dBFS travaille davantage, tolérance un peu plus large
        const double floorDb = (holes ? -9.0 : -4.0) - (config == 1 ? 1.5 : 0.0);
        const bool ok = ! bad && peak <= 1.0f && gainDb < 4.0 && gainDb > floorDb;
        if (! ok) ++failures;
        std::printf ("%2d %-18s peak %.2f  gain %+6.1f dB  %s\n", m + 1,
                     hitr::modes()[(size_t) m].shortName, peak, gainDb, ok ? "OK" : "PROBLEME");
    }
    }
    std::printf (failures == 0 ? "\nTous les modes OK\n" : "\n%d mode(s) a revoir\n", failures);
    return failures;
}
