#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Interface inspirée de ravenn : header avec nom en minuscules, panneaux sombres à
// pastille, knobs néon gradués. Chaque mode injecte sa palette et son visuel ;
// ombres, biseaux, parallaxe et grille en perspective donnent de la profondeur.
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
}

//==============================================================================
class HitrLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HitrLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
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
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& title);
    void resized() override;
    void paint (juce::Graphics&) override;
    void setColour (juce::Colour c) { slider.setColour (juce::Slider::rotarySliderFillColourId, c); repaint(); }
    juce::Slider slider;

private:
    juce::String title;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Bouton on/off lié à un paramètre booléen
class Pill : public juce::Component
{
public:
    Pill (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text);
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::TextButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

//==============================================================================
// Écran psychédélique en 3 plans (étoiles lointaines, visuel du mode, particules
// proches) avec parallaxe à la souris ; réagit au son, à la DOSE et au TRIP.
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

//==============================================================================
class HitrEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HitrEditor (HitrProcessor&);
    ~HitrEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setMode (int idx);
    void applyModeColours();
    void rebuildBackground();
    void drawPanel (juce::Graphics&, juce::Rectangle<int> r, const juce::String& title, juce::Colour c) const;

    HitrProcessor& proc;
    HitrLookAndFeel lnf;

    Knob dose, trip, mix, output, width, grain, gsize, gdens, delayMs;
    Pill freeze, transpose;
    juce::TextButton bpmBtn { "BPM" }, msBtn { "MS" };
    std::unique_ptr<juce::ParameterAttachment> syncAtt;
    juce::ComboBox modeBox, divisionBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt, divisionAtt;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::OwnedArray<juce::TextButton> modeBtns;
    TripView tripView;
    EffectsView effects;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Rectangle<int> header, tripPanel, ctrlPanel, fxPanel, grainPanel, timePanel, modesPanel, timeInfo;
    juce::Image background;
    int shownMode = -1;
    bool shownSync = true;
    juce::String shownTimeText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HitrEditor)
};
