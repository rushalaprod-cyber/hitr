#include "PresetManager.h"

namespace
{
    juce::String u8s (const char* s) { return juce::String::fromUTF8 (s); }
    // index d'une substance pour les couches de mélange (0 = AUCUN)
    constexpr float L (int modeIndex) { return (float) (modeIndex + 1); }

    enum M { Cannabis, Ket, LSD, Psilo, DMT, MDMA, Coke, Speed, Meth, Heroin, Alcool, GHB, Lean, Xanax, Mescaline,
             Aya, Salvia, TwoCB, Proto, Crack, Cafe, Poppers, Datura, MMC3, DXM, Morphine, Opium, Fenta, Trama,
             Kratom, XTC, MMC4, PCP, Iboga, FiveMeo, LSA, DOM, Amanite, Muscade, Absinthe, Adderall, Modaf, CBD,
             Nico, Kava, Khat, Zolpi, Spice, Solvants, DPH };
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : state (s)
{
    auto add = [this] (const char* cat, const char* name, std::initializer_list<std::pair<const char*, float>> v)
    {
        Factory f { u8s (name), u8s (cat), {} };
        for (auto& p : v) f.values.push_back ({ p.first, p.second });
        factory.push_back (f);
    };

    add ("BASE", "Défaut", {});

    add ("MÉLANGES", "Candyflip",     { { "mode", LSD }, { "mode2", L (MDMA) }, { "blend2", 0.7f } });
    add ("MÉLANGES", "Hippie Flip",   { { "mode", Psilo }, { "mode2", L (MDMA) }, { "blend2", 0.6f } });
    add ("MÉLANGES", "Kitty Flip",    { { "mode", Ket }, { "mode2", L (MDMA) }, { "blend2", 0.6f } });
    add ("MÉLANGES", "Calvin Klein",  { { "mode", Coke }, { "mode2", L (Ket) }, { "blend2", 0.7f } });
    add ("MÉLANGES", "Speedball",     { { "mode", Coke }, { "mode2", L (Heroin) }, { "blend2", 0.8f } });
    add ("MÉLANGES", "Purple Haze",   { { "mode", Lean }, { "mode2", L (Cannabis) }, { "blend2", 0.6f } });
    add ("MÉLANGES", "Nexus Flip",    { { "mode", TwoCB }, { "mode2", L (LSD) }, { "blend2", 0.6f } });
    add ("MÉLANGES", "Afterparty",    { { "mode", Xanax }, { "mode2", L (Alcool) }, { "blend2", 0.5f } });
    add ("MÉLANGES", "Trainwreck",    { { "mode", DMT }, { "mode2", L (Salvia) }, { "blend2", 0.5f }, { "mode3", L (Ket) }, { "blend3", 0.5f } });

    add ("CHILL", "Lofi Weed",        { { "mode", Cannabis }, { "dose", 0.55f }, { "heat", 0.2f }, { "thermType", 5.f }, { "tone", -2.f } });
    add ("CHILL", "Coton",            { { "mode", Heroin }, { "dose", 0.6f }, { "fatten", 0.2f } });
    add ("CHILL", "Nuage CBD",        { { "mode", CBD }, { "dose", 0.8f } });
    add ("CHILL", "Kava Sunset",      { { "mode", Kava }, { "mode2", L (Cannabis) }, { "blend2", 0.4f } });

    add ("PSYCHÉ", "Hyperespace",     { { "mode", DMT }, { "dose", 1.f }, { "grain", 0.7f } });
    add ("PSYCHÉ", "Lumière Blanche", { { "mode", FiveMeo }, { "dose", 0.9f } });
    add ("PSYCHÉ", "Forêt Sacrée",    { { "mode", Aya }, { "dose", 0.8f }, { "heat", 0.15f }, { "thermMove", 0.4f } });
    add ("PSYCHÉ", "Fractales",       { { "mode", DOM }, { "grain", 0.7f } });
    add ("PSYCHÉ", "Fée Verte",       { { "mode", Absinthe }, { "mode2", L (LSA) }, { "blend2", 0.5f } });

    add ("SOMBRE", "K-Hole",          { { "mode", Ket }, { "dose", 1.f }, { "grain", 0.8f } });
    add ("SOMBRE", "Délire Datura",   { { "mode", Datura }, { "dose", 0.9f } });
    add ("SOMBRE", "Ombres DPH",      { { "mode", DPH }, { "mode2", L (Zolpi) }, { "blend2", 0.4f } });
    add ("SOMBRE", "Fond du Puits",   { { "mode", Fenta }, { "dose", 0.85f }, { "fatten", 0.15f } });

    add ("STIMULANTS", "Club 3h du Mat", { { "mode", XTC }, { "dose", 0.9f }, { "ott", 0.35f }, { "clip", 3.f }, { "clipShape", 1.f } });
    add ("STIMULANTS", "Focus",          { { "mode", Adderall } });
    add ("STIMULANTS", "Rush",           { { "mode", Crack }, { "heat", 0.4f }, { "thermType", 1.f } });
    add ("STIMULANTS", "Nuit Blanche",   { { "mode", Speed }, { "mode2", L (Cafe) }, { "blend2", 0.8f } });

    add ("THERMAL & MASTER", "Thermal Fuzz",     { { "mode", Speed }, { "heat", 0.6f }, { "thermType", 1.f }, { "thermMove", 0.5f } });
    add ("THERMAL & MASTER", "Thermal Fold",     { { "mode", MMC4 }, { "heat", 0.5f }, { "thermType", 2.f }, { "thermMove", 0.6f } });
    add ("THERMAL & MASTER", "Gros Saucisson",   { { "mode", MDMA }, { "fatten", 0.55f }, { "fatColor", 0.6f }, { "clip", 2.f }, { "clipShape", 1.f } });
    add ("THERMAL & MASTER", "OTT Écrasé",       { { "mode", MMC4 }, { "ott", 0.5f }, { "ottTime", 0.4f }, { "clip", 3.f }, { "clipShape", 1.f } });
    add ("THERMAL & MASTER", "Live Sans Latence",{ { "mode", LSD }, { "noLatency", 1.f } });
    add ("THERMAL & MASTER", "Overdose",         { { "mode", DMT }, { "mode2", L (Salvia) }, { "blend2", 0.5f }, { "overdose", 1.f } });

    current = factory.front().name;
}

juce::File PresetManager::getDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("hitr").getChildFile ("Presets");
}

int PresetManager::importFiles (const juce::Array<juce::File>& files)
{
    auto dir = getDirectory();
    dir.createDirectory();
    int n = 0;
    for (auto& f : files)
    {
        if (! f.existsAsFile()) continue;
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName (state.state.getType())) continue;   // un vrai preset h.i.t.r
        auto dest = dir.getNonexistentChildFile (f.getFileNameWithoutExtension(), ".preset", false);
        if (f.copyFileTo (dest)) ++n;
    }
    return n;
}

bool PresetManager::exportTo (const juce::File& file) const
{
    auto st = state.copyState();
    st.setProperty ("preset", current, nullptr);
    if (auto xml = st.createXml())
        return xml->writeTo (file.withFileExtension (".preset"));
    return false;
}

void PresetManager::openFolder() const
{
    auto dir = getDirectory();
    dir.createDirectory();
    dir.startAsProcess();
}

juce::StringArray PresetManager::getFactoryNames() const
{
    juce::StringArray names;
    for (auto& f : factory) names.add (f.name);
    return names;
}

std::vector<std::pair<juce::String, juce::StringArray>> PresetManager::getFactoryByCategory() const
{
    std::vector<std::pair<juce::String, juce::StringArray>> out;
    for (auto& f : factory)
    {
        auto it = std::find_if (out.begin(), out.end(), [&] (auto& p) { return p.first == f.category; });
        if (it == out.end()) out.push_back ({ f.category, { f.name } });
        else it->second.add (f.name);
    }
    return out;
}

juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray names;
    for (auto& f : getDirectory().findChildFiles (juce::File::findFiles, false, "*.preset"))
        if (! isFactory (f.getFileNameWithoutExtension()))
            names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

juce::StringArray PresetManager::getAllNames() const
{
    auto all = getFactoryNames();
    all.addArray (getUserNames());
    return all;
}

bool PresetManager::isFactory (const juce::String& name) const
{
    for (auto& f : factory)
        if (f.name == name) return true;
    return false;
}

void PresetManager::resetAll()
{
    // SOBRIÉTÉ et la latence ne bougent pas quand on change de preset
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->getParameterID() != "sobriety" && rp->getParameterID() != "noLatency" && rp->getParameterID() != "os")
                rp->setValueNotifyingHost (rp->getDefaultValue());
}

void PresetManager::setParam (const juce::String& id, float value)
{
    if (auto* rp = state.getParameter (id))
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

void PresetManager::load (const juce::String& name)
{
    for (auto& f : factory)
    {
        if (f.name == name)
        {
            resetAll();
            for (auto& [id, v] : f.values) setParam (id, v);
            current = name;
            return;
        }
    }

    auto file = getDirectory().getChildFile (name + ".preset");
    if (! file.existsAsFile()) return;
    if (auto xml = juce::parseXML (file))
    {
        if (xml->hasTagName (state.state.getType()))
        {
            const bool sober = state.getRawParameterValue ("sobriety")->load() > 0.5f;
            state.replaceState (juce::ValueTree::fromXml (*xml));
            setParam ("sobriety", sober ? 1.f : 0.f);
            current = name;
        }
    }
}

bool PresetManager::save (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty() || isFactory (name)) return false;
    auto dir = getDirectory();
    if (! dir.createDirectory()) return false;
    auto st = state.copyState();
    st.setProperty ("preset", name, nullptr);
    if (auto xml = st.createXml())
        if (xml->writeTo (dir.getChildFile (name + ".preset")))
        {
            current = name;
            return true;
        }
    return false;
}

bool PresetManager::remove (const juce::String& name)
{
    if (isFactory (name)) return false;
    auto file = getDirectory().getChildFile (name + ".preset");
    if (! file.existsAsFile()) return false;
    const bool ok = file.moveToTrash();
    if (ok) current = factory.front().name;
    return ok;
}

void PresetManager::next()
{
    auto all = getAllNames();
    if (all.isEmpty()) return;
    load (all[(all.indexOf (current) + 1) % all.size()]);
}

void PresetManager::previous()
{
    auto all = getAllNames();
    if (all.isEmpty()) return;
    const int idx = all.indexOf (current);
    load (all[(idx <= 0 ? all.size() : idx) - 1]);
}
