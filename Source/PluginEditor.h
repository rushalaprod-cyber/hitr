#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Interface h.i.t.r 1.2 — base ravenn (header, panneaux à pastille, knobs néon,
// presets, A/B/C/D, undo/redo), verre coloré selon la substance, profondeur.
namespace hui
{
inline juce::Font font (float h, bool bold = false, float kerning = 0.f)
{
    return juce::Font (juce::FontOptions (juce::jmax (6.f, h), bold ? juce::Font::bold : juce::Font::plain)
                           .withKerningFactor (kerning));
}
const juce::Colour bg     { 0xff030406 };
const juce::Colour panel  { 0xff07090d };
const juce::Colour edge   { 0xff1d2733 };
const juce::Colour track  { 0xff121821 };
const juce::Colour text   { 0xffe9eef3 };
const juce::Colour dim    { 0xff738191 };
const juce::Colour danger { 0xffff2a3d };
juce::String u (const char* s);
void glowBlob (juce::Graphics& g, juce::Colour col, float x, float y, float rad);
}

//==============================================================================
class HitrLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HitrLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hover, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool hover, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Colour accent { 0xff7fe7ff };
};

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title, const juce::String& tip = {});
    void resized() override;
    void paint (juce::Graphics&) override;
    void setColour (juce::Colour c) { slider.setColour (juce::Slider::rotarySliderFillColourId, c); repaint(); }
    juce::Slider slider;
private:
    juce::String title;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class Pill : public juce::Component
{
public:
    Pill (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text, const juce::String& tip = {});
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::TextButton button;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// Boutons exclusifs liés à un paramètre "choice" (repris de ravenn)
class Segmented : public juce::Component
{
public:
    Segmented (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& tip, int columns = 0);
    void resized() override;
    void setColour (juce::Colour c) { for (auto* b : buttons) b->setColour (juce::TextButton::buttonOnColourId, c); }
private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int columns = 0;
};

//==============================================================================
// Tuile d'une substance : clic = principale, clic droit / Maj+clic = couche de mélange
class ModeTile : public juce::Button
{
public:
    ModeTile (int index);
    void paintButton (juce::Graphics&, bool hover, bool down) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void (bool layer)> onPick;
    int index;
    int role = 0;   // 0 rien, 1 principale, 2 couche 2, 3 couche 3
};

// Couche de mélange : nom, dosage, retirer
class LayerChip : public juce::Component
{
public:
    LayerChip (HitrProcessor& p, int layerNumber);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
private:
    HitrProcessor& proc;
    int num;
    juce::Slider blend;
    juce::TextButton removeBtn { juce::String::fromUTF8 (u8"✕") };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    int shown = -2;
};

//==============================================================================
class TripView : public juce::Component, private juce::Timer
{
public:
    explicit TripView (HitrProcessor& p) : proc (p) { startTimerHz (40); setOpaque (true); }
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent&) override { mouseTarget = {}; }
private:
    void timerCallback() override;
    HitrProcessor& proc;
    double t = 0.0;
    float level = 0.0f;
    juce::Point<float> mouseTarget, parallax;
    juce::Random rng;
};

class EffectsView : public juce::Component, private juce::Timer
{
public:
    explicit EffectsView (HitrProcessor& p) : proc (p) { startTimerHz (24); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { t += 1.0 / 24.0; repaint(); }
    HitrProcessor& proc;
    double t = 0.0;
};

// Courbe de transfert THERMAL + mouvement
class ThermalView : public juce::Component, private juce::Timer
{
public:
    explicit ThermalView (HitrProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { t += 1.0 / 30.0; repaint(); }
    HitrProcessor& proc;
    double t = 0.0;
};

// Réduction du clipper
class ClipMeter : public juce::Component, private juce::Timer
{
public:
    explicit ClipMeter (HitrProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    HitrProcessor& proc;
    float disp = 0.f, hold = 0.f; int holdT = 0;
};

//==============================================================================
// Page des presets (reprise de ravenn) : catégories, recherche, tuiles
class PresetBrowser : public juce::Component
{
public:
    explicit PresetBrowser (HitrProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void syncSelection();
    std::function<void()> onClose, onSave, onImport, onExport, onDelete;
    juce::Colour accent { 0xff7fe7ff };
private:
    void rebuildTiles();
    HitrProcessor& proc;
    juce::TextEditor search;
    juce::TextButton closeBtn, saveBtn { "SAVE" }, importBtn { "IMPORT" }, exportBtn { "EXPORT" }, folderBtn, deleteBtn;
    juce::OwnedArray<juce::TextButton> catBtns, tiles;
    juce::String category { "TOUS" };
    juce::Rectangle<int> panel, tileArea;
};

//==============================================================================
class HitrEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HitrEditor (HitrProcessor&);
    ~HitrEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void setPage (int p);
    void showPresetBrowser (bool show);

private:
    void timerCallback() override;
    void setMode (int idx);
    void applyModeColours();
    void rebuildBackground();
    void refreshPresets();
    void savePreset();
    void importPresets();
    void exportPreset();
    void drawPanel (juce::Graphics&, juce::Rectangle<int> r, const juce::String& title, juce::Colour c) const;
    void drawSection (juce::Graphics&, juce::Rectangle<int> r, const juce::String& title, juce::Colour c) const;
    void place (juce::Component& c, int page);

    HitrProcessor& proc;
    HitrLookAndFeel lnf;

    // DOSAGE
    Knob dose, trip, mix, output, width, widthFreq;
    Pill overdose, sobriety, noLatency, autoGain;
    // TRIP
    Knob grain, gsize, gdens, delayMs;
    Pill freeze, transpose;
    juce::TextButton bpmBtn { "BPM" }, msBtn { "MS" };
    std::unique_ptr<juce::ParameterAttachment> syncAtt;
    juce::ComboBox divisionBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionAtt;
    EffectsView effects;
    // THERMAL
    Knob heat, thermMove, thermTone, thermMix;
    Segmented thermType, thermRate;
    ThermalView thermalView;
    // DYNAMICS
    Knob ottK, ottTime, ottLow, ottMid, ottHigh, fatten, fatColor, tone, toneLow, toneAir;
    // CLIPPER
    Knob clip, clipKnee, clipXLo, clipXHi;
    Segmented clipShape, clipBand, os;
    Pill clipMS, body;
    ClipMeter clipMeter;

    // header
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, presetNameBtn, saveBtn { "SAVE" }, copyBtn,
                     undoBtn { "UNDO" }, redoBtn { "REDO" }, randomBtn;
    juce::OwnedArray<juce::TextButton> slotBtns, tabs;
    std::unique_ptr<juce::FileChooser> chooser;

    // substances
    juce::OwnedArray<ModeTile> tiles;
    LayerChip layer2, layer3;
    TripView tripView;
    PresetBrowser browser;
    juce::TooltipWindow tooltips { this, 500 };

    std::vector<std::pair<juce::Component*, int>> pageOf;
    juce::Rectangle<int> header, tripPanel, ctrlPanel, modesPanel, pagePanel, tabArea, mixStrip, infoArea;
    std::array<juce::Rectangle<int>, 4> sections;
    std::array<juce::String, 4> sectionNames;
    juce::Image background, ravenImg, ravenCrisp;   // logo : le corbeau de ravenn
    float bgScale = 1.f;                            // fond rendu à la résolution réelle de l'écran
    int shownMode = -1, page = 0, shownSlot = -1;
    juce::String shownPreset, shownInfo, shownTimeText;
    bool shownSync = true, shownOverdose = false, shownSober = false;
    double pulse = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HitrEditor)
};
