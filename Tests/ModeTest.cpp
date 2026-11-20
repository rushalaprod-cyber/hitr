// Test hors DAW : chaque substance traite 6 s de mélodie dans 3 configurations,
// on vérifie stabilité, crêtes et volume. --snap <dossier> : captures de l'interface.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static void setP (HitrProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void snapshot (juce::Component& c, const juce::File& f)
{
    for (int i = 0; i < 30; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    auto img = c.createComponentSnapshot (c.getLocalBounds());
    f.deleteFile();
    juce::FileOutputStream out (f);
    juce::PNGImageFormat().writeImageToStream (img, out);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc > 2 && juce::String (argv[1]) == "--snap")
    {
        juce::File dir (juce::String::fromUTF8 (argv[2]));
        dir.createDirectory();
        HitrProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        p.outputLevel.store (0.15f);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* he = dynamic_cast<HitrEditor*> (ed.get());
        for (int m = 0; m < hitr::numModes; ++m)
        {
            setP (p, "mode", (float) m);
            snapshot (*ed, dir.getChildFile (juce::String (m + 1).paddedLeft ('0', 2) + ".png"));
        }
        // mélange + pages
        setP (p, "mode", 2.f); setP (p, "mode2", 6.f); setP (p, "blend2", 0.7f); setP (p, "mode3", 17.f);
        setP (p, "heat", 0.6f); setP (p, "thermType", 2.f);
        for (int pg = 0; pg < 4; ++pg) { he->setPage (pg); snapshot (*ed, dir.getChildFile ("page" + juce::String (pg) + ".png")); }
        he->setPage (0);
        setP (p, "overdose", 1.f);
        snapshot (*ed, dir.getChildFile ("overdose.png"));
        setP (p, "overdose", 0.f); setP (p, "sobriety", 1.f);
        snapshot (*ed, dir.getChildFile ("sobriete.png"));
        setP (p, "sobriety", 0.f);
        he->showPresetBrowser (true);
        snapshot (*ed, dir.getChildFile ("presets.png"));
        return 0;
    }

    const double sr = 48000.0;
    const int block = 512, seconds = 6;
    const float notes[] = { 440.f, 523.25f, 659.25f, 587.33f, 493.88f, 392.f, 440.f, 329.63f };
    int failures = 0;
    const char* titles[] = { "== Réglages par défaut (BPM)",
                             "== Extrêmes : MS, FREEZE, GRAIN 100%, WIDTH 200%, GAMME LIBRE",
                             "== Mélange 3 couches + OVERDOSE + THERMAL + FATTENER + OTT + CLIP +6 + BODY + x4 + AUTO GAIN" };

    for (int config = 0; config < 3; ++config)
    {
        std::printf ("%s%s\n", config ? "\n" : "", titles[config]);
        for (int m = 0; m < hitr::numModes; ++m)
        {
            HitrProcessor p;
            p.setPlayConfigDetails (2, 2, sr, block);
            setP (p, "mode", (float) m);
            setP (p, "dose", 1.0f);
            if (config == 1)
            {
                setP (p, "sync", 0.f); setP (p, "grain", 1.f); setP (p, "width", 2.f); setP (p, "transpose", 1.f);
                setP (p, "gsize", 4.f); setP (p, "gdens", 4.f);
            }
            if (config == 2)
            {
                setP (p, "mode2", (float) ((m + 7) % hitr::numModes + 1)); setP (p, "blend2", 0.8f);
                setP (p, "mode3", (float) ((m + 23) % hitr::numModes + 1)); setP (p, "blend3", 0.6f);
                setP (p, "overdose", 1.f); setP (p, "heat", 0.7f); setP (p, "thermType", (float) (m % 6));
                setP (p, "thermMove", 0.8f); setP (p, "fatten", 0.4f); setP (p, "ott", 0.5f);
                setP (p, "clip", 6.f); setP (p, "clipShape", 1.f); setP (p, "body", 1.f); setP (p, "os", 2.f);
                setP (p, "autoGain", 1.f); setP (p, "tone", 3.f); setP (p, "widthFreq", 300.f);
            }
            p.prepareToPlay (sr, block);

            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            double phase = 0.0, inE = 0.0, outE = 0.0;
            float peak = 0.f;
            bool bad = false;
            const int total = (int) (sr * seconds);
            for (int pos = 0; pos < total; pos += block)
            {
                if (config == 1 && pos >= (int) (sr * 1.5) && pos < (int) (sr * 1.5) + block)
                    setP (p, "freeze", 1.f);
                for (int i = 0; i < block; ++i)
                {
                    const int nIdx = ((pos + i) / (int) (sr / 4)) % 8;
                    phase += notes[nIdx] / sr;
                    const float s = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * phase);
                    buf.setSample (0, i, s); buf.setSample (1, i, s);
                    if (pos > sr * 1.5) inE += s * s;
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
            const bool holes = hitr::modes()[(size_t) m].values[hitr::DROPOUT] > 0.0f || config == 2;
            const double floorDb = (holes ? -9.0 : -4.0) - (config == 1 ? 1.5 : 0.0);
            const bool ok = ! bad && peak <= 1.0f && gainDb < 4.0 && gainDb > floorDb;
            if (! ok) ++failures;
            std::printf ("%2d %-10s peak %.2f  gain %+6.1f dB  %s\n", m + 1, hitr::modes()[(size_t) m].shortName, peak, gainDb, ok ? "OK" : "PROBLEME");
        }
    }

    // SOBRIÉTÉ : la sortie doit être le signal d'origine (retardé de la latence)
    {
        HitrProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        setP (p, "sobriety", 1.f);
        p.prepareToPlay (sr, block);
        const int lat = p.getLatencySamples();
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        std::vector<float> in, out;
        juce::Random rnd (1);
        for (int b = 0; b < 40; ++b)
        {
            for (int i = 0; i < block; ++i) { const float s = rnd.nextFloat() * 0.5f - 0.25f; buf.setSample (0, i, s); buf.setSample (1, i, s); in.push_back (s); }
            p.processBlock (buf, midi);
            for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
        }
        double err = 0;
        for (size_t i = (size_t) block * 10; i < out.size(); ++i) err = juce::jmax (err, (double) std::abs (out[i] - in[i - (size_t) lat]));
        const bool ok = err < 1.0e-5;
        if (! ok) ++failures;
        std::printf ("\nSOBRIÉTÉ (bypass) : latence %d éch., écart max %.2e  %s\n", lat, err, ok ? "OK" : "PROBLEME");
    }

    std::printf (failures == 0 ? "\nTous les tests OK\n" : "\n%d test(s) a revoir\n", failures);
    return failures;
}
