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

    // 1.2.2 : THERMAL / FATTEN — pas de pic à l'activation, volume stable, montée douce
    {
        std::printf ("\n");
        // joue 3 s de mélodie à -12 dBFS ; "change" est appliqué à 1,5 s ; renvoie crête avant / après et RMS avant / après
        auto run = [&] (std::function<void (HitrProcessor&)> before, std::function<void (HitrProcessor&)> change,
                        float& pkBefore, float& pkAfter, double& rmsBefore, double& rmsAfter)
        {
            HitrProcessor p;
            p.setPlayConfigDetails (2, 2, sr, block);
            setP (p, "mode", 42.f); setP (p, "dose", 0.f); setP (p, "grain", 0.f);   // CBD à dose 0 : on n'entend que le master
            before (p);
            p.prepareToPlay (sr, block);
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            double ph = 0, eB = 0, eA = 0; long nB = 0, nA = 0;
            pkBefore = pkAfter = 0.f;
            for (int pos = 0; pos < (int) (sr * 3.0); pos += block)
            {
                if (pos >= (int) (sr * 1.5) && pos < (int) (sr * 1.5) + block) change (p);
                for (int i = 0; i < block; ++i)
                {
                    ph += notes[((pos + i) / (int) (sr / 4)) % 8] / sr;
                    const float s = 0.25f * (float) std::sin (juce::MathConstants<double>::twoPi * ph);
                    buf.setSample (0, i, s); buf.setSample (1, i, s);
                }
                p.processBlock (buf, midi);
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (0, i);
                    const int t = pos + i;
                    if (t > (int) (sr * 0.8) && t < (int) (sr * 1.5)) { pkBefore = juce::jmax (pkBefore, std::abs (v)); eB += v * v; ++nB; }
                    if (t >= (int) (sr * 1.5) && t < (int) (sr * 1.8)) pkAfter = juce::jmax (pkAfter, std::abs (v));
                    if (t > (int) (sr * 2.2)) { eA += v * v; ++nA; }
                }
            }
            rmsBefore = std::sqrt (eB / juce::jmax (1L, nB)); rmsAfter = std::sqrt (eA / juce::jmax (1L, nA));
        };
        auto db = [] (double a, double b) { return 20.0 * std::log10 ((a + 1e-12) / (b + 1e-12)); };

        for (int type = 0; type < 6; ++type)
        {
            float pb, pa; double rb, ra;
            run ([] (HitrProcessor&) {}, [type] (HitrProcessor& p) { setP (p, "thermType", (float) type); setP (p, "heat", 0.8f); }, pb, pa, rb, ra);
            const double peakJump = db (pa, pb), level = db (ra, rb);
            const bool ok = peakJump < 1.5 && std::abs (level) < 3.0;   // crête tenue à +1,5 dB max
            if (! ok) ++failures;
            std::printf ("THERMAL %-8s activé à 80 %% : pic %+5.1f dB, volume %+5.1f dB  %s\n",
                         HitrProcessor::thermalTypeNames()[type].toRawUTF8(), peakJump, level, ok ? "OK" : "PROBLEME");
        }
        {
            // HEAT à 15 % : doit rester discret (écart au son propre bien en dessous du signal)
            HitrProcessor a, b;
            for (auto* p : { &a, &b }) { p->setPlayConfigDetails (2, 2, sr, block); setP (*p, "mode", 42.f); setP (*p, "dose", 0.f); setP (*p, "grain", 0.f); setP (*p, "thermType", 1.f); }
            setP (b, "heat", 0.15f);
            a.prepareToPlay (sr, block); b.prepareToPlay (sr, block);
            juce::AudioBuffer<float> ba (2, block), bb (2, block);
            juce::MidiBuffer midi;
            double ph = 0, eSig = 0, eDiff = 0;
            for (int pos = 0; pos < (int) (sr * 2.0); pos += block)
            {
                for (int i = 0; i < block; ++i)
                {
                    ph += notes[((pos + i) / (int) (sr / 4)) % 8] / sr;
                    const float s = 0.25f * (float) std::sin (juce::MathConstants<double>::twoPi * ph);
                    ba.setSample (0, i, s); ba.setSample (1, i, s); bb.setSample (0, i, s); bb.setSample (1, i, s);
                }
                a.processBlock (ba, midi); b.processBlock (bb, midi);
                if (pos > sr * 0.5)
                    for (int i = 0; i < block; ++i) { const float x = ba.getSample (0, i), y = bb.getSample (0, i); eSig += x * x; eDiff += (y - x) * (y - x); }
            }
            const double rel = 10.0 * std::log10 ((eDiff + 1e-12) / (eSig + 1e-12));
            const bool ok = rel < -14.0;
            if (! ok) ++failures;
            std::printf ("THERMAL FUZZ à 15 %% : distorsion ajoutée %.1f dB sous le signal  %s\n", rel, ok ? "OK (montée douce)" : "PROBLEME");
        }
        {
            float pb, pa; double rb, ra;
            run ([] (HitrProcessor&) {}, [] (HitrProcessor& p) { setP (p, "fatten", 1.f); }, pb, pa, rb, ra);
            const double level = db (ra, rb), peakJump = db (pa, pb);
            const bool ok = level < 2.5 && peakJump < 4.0;
            if (! ok) ++failures;
            std::printf ("FATTEN 0 -> 100 %% : volume %+5.1f dB, pic %+5.1f dB  %s\n", level, peakJump, ok ? "OK" : "PROBLEME");
        }
        {
            // LINK : IN +4 dB -> CLIPPER -4 dB (et RESET remet tout)
            HitrProcessor p;
            setP (p, "link", 1.f);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            setP (p, "in", 4.f);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
            const float clipAfter = p.apvts.getRawParameterValue ("clip")->load();
            setP (p, "clip", 2.f);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
            const float inAfter = p.apvts.getRawParameterValue ("in")->load();
            const bool okLink = std::abs (clipAfter + 4.f) < 0.05f && std::abs (inAfter - (4.f - 6.f)) < 0.05f;
            if (! okLink) ++failures;
            std::printf ("LINK : IN +4 -> CLIPPER %+.1f dB ; CLIPPER -4 -> +2 -> IN %+.1f dB  %s\n", clipAfter, inAfter, okLink ? "OK" : "PROBLEME");
            setP (p, "heat", 0.7f); setP (p, "mode2", 5.f); setP (p, "sobriety", 1.f);
            p.resetAll();
            const bool okReset = p.apvts.getRawParameterValue ("heat")->load() == 0.f && p.apvts.getRawParameterValue ("in")->load() == 0.f
                                 && p.apvts.getRawParameterValue ("mode2")->load() == 0.f && p.apvts.getRawParameterValue ("sobriety")->load() > 0.5f;
            if (! okReset) ++failures;
            std::printf ("RESET : tout par défaut, SOBRIÉTÉ gardée  %s\n", okReset ? "OK" : "PROBLEME");
            // RESET d'onglet : THERMAL seul
            setP (p, "heat", 0.6f); setP (p, "thermType", 3.f); setP (p, "fatten", 0.5f); setP (p, "grain", 0.9f);
            p.resetPage (1);
            const bool okPage = p.apvts.getRawParameterValue ("heat")->load() == 0.f && p.apvts.getRawParameterValue ("thermType")->load() == 0.f
                                && std::abs (p.apvts.getRawParameterValue ("fatten")->load() - 0.5f) < 1e-4f
                                && std::abs (p.apvts.getRawParameterValue ("grain")->load() - 0.9f) < 1e-4f;
            if (! okPage) ++failures;
            std::printf ("RESET onglet THERMAL : THERMAL remis à zéro, les autres onglets intacts  %s\n", okPage ? "OK" : "PROBLEME");
        }
    }

    // COMPATIBILITÉ : un projet sauvé en 1.0 ou 1.1 doit se rouvrir à l'identique
    {
        auto check = [&] (const char* label, const char* xmlText, std::initializer_list<std::pair<const char*, float>> expect)
        {
            auto xml = juce::parseXML (juce::String (xmlText));
            juce::MemoryBlock mb;
            juce::AudioProcessor::copyXmlToBinary (*xml, mb);
            HitrProcessor p;
            p.setPlayConfigDetails (2, 2, sr, block);
            p.setStateInformation (mb.getData(), (int) mb.getSize());
            p.prepareToPlay (sr, block);
            bool ok = true;
            for (auto& [id, v] : expect)
            {
                const float got = p.apvts.getRawParameterValue (id)->load();
                if (std::abs (got - v) > 1.0e-4f) { ok = false; std::printf ("   %s : %g au lieu de %g\n", id, got, v); }
            }
            // et ça doit sonner sans souci
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            for (int b = 0; b < 100; ++b)
            {
                for (int i = 0; i < block; ++i) { const float s = 0.4f * std::sin ((float) (b * block + i) * 0.05f); buf.setSample (0, i, s); buf.setSample (1, i, s); }
                p.processBlock (buf, midi);
                for (int i = 0; i < block; ++i) if (! std::isfinite (buf.getSample (0, i))) ok = false;
            }
            if (! ok) ++failures;
            std::printf ("Projet %s : %s\n", label, ok ? "OK" : "PROBLEME");
        };
        std::printf ("\n");
        // état tel que l'écrivait la 1.0 (5 paramètres)
        check ("1.0 (Salvia, dose 45 %, mix 80 %)",
               "<HITR><PARAM id=\"mode\" value=\"16\"/><PARAM id=\"dose\" value=\"0.45\"/><PARAM id=\"mix\" value=\"0.8\"/>"
               "<PARAM id=\"trip\" value=\"1.5\"/><PARAM id=\"output\" value=\"-3\"/></HITR>",
               { { "mode", 16.f }, { "dose", 0.45f }, { "mix", 0.8f }, { "trip", 1.5f }, { "output", -3.f },
                 { "mode2", 0.f }, { "heat", 0.f }, { "clip", 0.f }, { "sobriety", 0.f }, { "overdose", 0.f },
                 { "width", 1.f }, { "grain", 0.5f }, { "sync", 1.f }, { "transpose", 0.f } });
        // état tel que l'écrivait la 1.1
        check ("1.1 (DXM, MS 420 ms, freeze, width 150 %)",
               "<HITR><PARAM id=\"mode\" value=\"24\"/><PARAM id=\"dose\" value=\"0.9\"/><PARAM id=\"mix\" value=\"1\"/>"
               "<PARAM id=\"trip\" value=\"1\"/><PARAM id=\"output\" value=\"0\"/><PARAM id=\"width\" value=\"1.5\"/>"
               "<PARAM id=\"grain\" value=\"0.7\"/><PARAM id=\"gsize\" value=\"2\"/><PARAM id=\"gdens\" value=\"1\"/>"
               "<PARAM id=\"freeze\" value=\"1\"/><PARAM id=\"sync\" value=\"0\"/><PARAM id=\"division\" value=\"6\"/>"
               "<PARAM id=\"delayms\" value=\"420\"/><PARAM id=\"transpose\" value=\"1\"/></HITR>",
               { { "mode", 24.f }, { "dose", 0.9f }, { "width", 1.5f }, { "grain", 0.7f }, { "gsize", 2.f }, { "freeze", 1.f },
                 { "sync", 0.f }, { "division", 6.f }, { "delayms", 420.f }, { "transpose", 1.f },
                 { "mode2", 0.f }, { "heat", 0.f }, { "fatten", 0.f }, { "ott", 0.f }, { "sobriety", 0.f } });
    }

    std::printf (failures == 0 ? "\nTous les tests OK\n" : "\n%d test(s) a revoir\n", failures);
    return failures;
}
