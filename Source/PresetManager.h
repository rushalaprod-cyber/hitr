#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Presets (repris de ravenn) : presets d'usine dans le code + presets utilisateur
// (fichiers .preset dans Documents/hitr/Presets), import, export, dossier.
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& s);

    juce::StringArray getFactoryNames() const;
    std::vector<std::pair<juce::String, juce::StringArray>> getFactoryByCategory() const;
    juce::StringArray getUserNames() const;
    juce::StringArray getAllNames() const;
    bool isFactory (const juce::String& name) const;

    void load (const juce::String& name);
    bool save (const juce::String& name);
    bool remove (const juce::String& name);
    bool exportTo (const juce::File& file) const;
    int importFiles (const juce::Array<juce::File>& files);
    void openFolder() const;
    void next();
    void previous();

    juce::String getCurrent() const { return current; }
    void setCurrentName (const juce::String& n) { current = n; }

    static juce::File getDirectory();

private:
    struct Factory
    {
        juce::String name, category;
        std::vector<std::pair<juce::String, float>> values;
    };

    void resetAll();
    void setParam (const juce::String& id, float value);

    juce::AudioProcessorValueTreeState& state;
    std::vector<Factory> factory;
    juce::String current;
};
