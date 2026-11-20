#include "PluginEditor.h"

using namespace juce;
using namespace hitr;

String hui::u (const char* s) { return String::fromUTF8 (s); }

void hui::glowBlob (Graphics& g, Colour col, float x, float y, float rad)
{
    g.setGradientFill (ColourGradient (col, x, y, col.withAlpha (0.f), x + rad, y, true));
    g.fillEllipse (x - rad, y - rad, rad * 2.f, rad * 2.f);
}
using hui::glowBlob;
using hui::u;

static float param (HitrProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
static int currentModeIndex (HitrProcessor& p) { return jlimit (0, numModes - 1, (int) param (p, "mode")); }
static const Mode& currentMode (HitrProcessor& p) { return modes()[(size_t) currentModeIndex (p)]; }
static int layerIndex (HitrProcessor& p, int n)   // couche 2 ou 3 -> index de substance, -1 si vide
{
    const int v = (int) param (p, n == 2 ? "mode2" : "mode3") - 1;
    return (v >= 0 && v < numModes && v != currentModeIndex (p)) ? v : -1;
}

// Panneau "verre" : ombre portée, corps translucide biseauté, lueur, pastille + titre (style ravenn)
static void drawGlassPanel (Graphics& g, Rectangle<int> area, const String& title, Colour c, float s = 1.f)
{
    auto r = area.toFloat();
    DropShadow (Colours::black.withAlpha (0.75f), roundToInt (22 * s), { 0, roundToInt (10 * s) }).drawForRectangle (g, area);
    g.setGradientFill (ColourGradient (Colour (0xff111722).withAlpha (0.88f), r.getX(), r.getY(),
                                       Colour (0xff040508).withAlpha (0.93f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 9.f * s);
    g.setGradientFill (ColourGradient (c.withAlpha (0.13f), r.getX(), r.getY(), c.withAlpha (0.0f), r.getX(), r.getY() + 54.f * s, false));
    g.fillRoundedRectangle (r.withHeight (54.f * s), 9.f * s);
    g.setGradientFill (ColourGradient (c.withAlpha (0.7f), r.getX(), r.getY(), hui::edge.withAlpha (0.8f), r.getRight(), r.getBottom(), false));
    g.drawRoundedRectangle (r.reduced (0.5f), 9.f * s, 1.f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawLine (r.getX() + 12.f, r.getY() + 1.f, r.getRight() - 12.f, r.getY() + 1.f, 1.f);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawLine (r.getX() + 12.f, r.getBottom() - 1.f, r.getRight() - 12.f, r.getBottom() - 1.f, 1.f);
    if (title.isEmpty()) return;
    const float dotX = r.getX() + 17.f * s, dotY = r.getY() + 18.f * s;
    glowBlob (g, c.withAlpha (0.55f), dotX, dotY, 10.f * s);
    g.setColour (c);
    g.fillEllipse (dotX - 3.f * s, dotY - 3.f * s, 6.f * s, 6.f * s);
    g.setColour (hui::text);
    g.setFont (hui::font (11.5f * s, true, 0.22f));
    g.drawText (title, Rectangle<float> (dotX + 11.f * s, dotY - 9.f * s, r.getWidth() - 40.f * s, 18.f * s), Justification::centredLeft);
}

//==============================================================================
HitrLookAndFeel::HitrLookAndFeel()
{
    setColour (PopupMenu::backgroundColourId, Colour (0xff0a0d12));
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff3d7bff).withAlpha (0.35f));
    setColour (PopupMenu::textColourId, hui::text);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (PopupMenu::headerTextColourId, hui::dim);
    setColour (ComboBox::textColourId, hui::text);
    setColour (TextButton::buttonColourId, hui::panel);
    setColour (TextButton::textColourOffId, hui::text.withAlpha (0.82f));
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff10141c));
    setColour (TooltipWindow::textColourId, hui::text);
    setColour (TooltipWindow::outlineColourId, hui::edge);
    setColour (AlertWindow::backgroundColourId, Colour (0xff0f1220));
    setColour (AlertWindow::textColourId, hui::text);
    setColour (AlertWindow::outlineColourId, hui::edge);
    setColour (TextEditor::backgroundColourId, Colour (0xff0b0f15));
    setColour (TextEditor::textColourId, hui::text);
    setColour (TextEditor::outlineColourId, hui::edge);
}

void HitrLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, Slider& slider)
{
    const float size = (float) jmin (w, h);
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).withSizeKeepingCentre (size, size).reduced (size * 0.05f);
    const float radius = b.getWidth() * 0.5f;
    if (radius < 8.f) return;
    const auto c = b.getCentre();
    const float lw = jmax (2.5f, radius * 0.085f);
    const float arcR = radius - lw;
    const auto fill = slider.findColour (Slider::rotarySliderFillColourId);
    const auto fill2 = fill.withRotatedHue (-0.09f).brighter (0.25f);
    const PathStrokeType stroke (lw, PathStrokeType::curved, PathStrokeType::rounded);
    const float kr = arcR - lw * 1.75f;

    glowBlob (g, Colours::black.withAlpha (0.75f), c.x, c.y + kr * 0.28f, kr * 1.35f);
    for (int i = 0; i <= 30; ++i)
    {
        const float a = startAngle + (float) i / 30.f * (endAngle - startAngle);
        const auto pt = c.getPointOnCircumference (radius - lw * 0.1f + lw * 0.55f, a);
        const float d = (i % 5 == 0) ? lw * 0.30f : lw * 0.16f;
        g.setColour ((a <= startAngle + pos * (endAngle - startAngle) ? fill : hui::dim).withAlpha (i % 5 == 0 ? 0.65f : 0.32f));
        g.fillEllipse (pt.x - d, pt.y - d, d * 2.f, d * 2.f);
    }
    Path trackPath;
    trackPath.addCentredArc (c.x, c.y, arcR, arcR, 0.f, startAngle, endAngle, true);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.strokePath (trackPath, PathStrokeType (lw + 2.f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (hui::track);
    g.strokePath (trackPath, stroke);

    float zero = 0.f;
    if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
        zero = (float) slider.valueToProportionOfLength (0.0);
    const float a0 = startAngle + zero * (endAngle - startAngle);
    const float a1 = startAngle + pos * (endAngle - startAngle);
    if (std::abs (a1 - a0) > 0.002f)
    {
        Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.f, jmin (a0, a1), jmax (a0, a1), true);
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (fill.withAlpha (0.065f));
            g.strokePath (val, PathStrokeType (lw * (1.f + (float) i * 0.9f), PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setGradientFill (ColourGradient (fill, b.getX(), b.getBottom(), fill2, b.getRight(), b.getY(), false));
        g.strokePath (val, stroke);
    }

    g.setGradientFill (ColourGradient (Colour (0xff2c3644), c.x - kr * 0.3f, c.y - kr * 0.8f,
                                       Colour (0xff040609), c.x + kr * 0.4f, c.y + kr, true));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f), c.x, c.y - kr,
                                       Colours::white.withAlpha (0.0f), c.x, c.y + kr * 0.2f, false));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f, 1.2f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f), c.x, c.y - kr * 0.9f,
                                       Colours::white.withAlpha (0.0f), c.x, c.y - kr * 0.1f, false));
    g.fillEllipse (c.x - kr * 0.7f, c.y - kr * 0.92f, kr * 1.4f, kr * 0.9f);
    g.setColour (fill.withAlpha (0.12f));
    g.drawEllipse (c.x - kr * 0.80f, c.y - kr * 0.80f, kr * 1.6f, kr * 1.6f, 1.f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - kr - 1.5f, c.y - kr - 1.5f, kr * 2.f + 3.f, kr * 2.f + 3.f, 1.2f);
    const auto p0 = c.getPointOnCircumference (kr * 0.62f, a1), p1 = c.getPointOnCircumference (kr * 0.93f, a1);
    const float bw = jmax (1.6f, lw * 0.32f);
    g.setColour (fill.withAlpha (0.30f));
    g.drawLine ({ p0, p1 }, bw * 3.2f);
    g.setColour (Colours::white.withAlpha (0.95f));
    g.drawLine ({ p0, p1 }, bw);

    g.setColour (hui::text);
    g.setFont (hui::font (jmax (8.5f, kr * 0.30f), true));
    g.drawFittedText (slider.getTextFromValue (slider.getValue()), Rectangle<float> (kr * 1.55f, kr * 0.6f).withCentre (c).toNearestInt(),
                      Justification::centred, 1, 0.65f);
}

void HitrLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        Slider::SliderStyle, Slider& s)
{
    auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    auto trackR = r.withSizeKeepingCentre (r.getWidth(), 6.f);
    const auto c = s.findColour (Slider::thumbColourId);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (trackR.translated (0, -1.f), 3.f);
    g.setColour (hui::track);
    g.fillRoundedRectangle (trackR, 3.f);
    auto fillR = trackR.withRight (pos);
    g.setGradientFill (ColourGradient (c.darker (0.4f), fillR.getX(), 0, c, fillR.getRight(), 0, false));
    g.fillRoundedRectangle (fillR, 3.f);
    glowBlob (g, c.withAlpha (0.5f), pos, r.getCentreY(), 10.f);
    g.setColour (Colours::white);
    g.fillEllipse (pos - 5.f, r.getCentreY() - 5.f, 10.f, 10.f);
}

void HitrLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hover, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (3.f);
    const float rad = jmin (6.f, r.getHeight() * 0.24f);
    const auto acc = b.findColour (TextButton::buttonOnColourId);
    const float alpha = b.isEnabled() ? 1.f : 0.35f;

    if (b.getProperties().contains ("tab"))
    {
        const bool on = b.getToggleState();
        g.setGradientFill (ColourGradient (Colour (on ? 0xff151d2a : 0xff090c11).withAlpha (0.95f), r.getX(), r.getY(),
                                           Colour (0xff030406).withAlpha (0.95f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (hui::edge.brighter (hover ? 0.5f : 0.f));
        g.drawRoundedRectangle (r, rad, 1.f);
        if (on)
        {
            auto line = r.removeFromBottom (2.f).reduced (6.f, 0.f);
            g.setGradientFill (ColourGradient (acc.withAlpha (0.f), line.getX(), 0, acc, line.getCentreX(), 0, true));
            g.fillRect (line);
            glowBlob (g, acc.withAlpha (0.25f), r.getCentreX(), r.getBottom(), r.getWidth() * 0.35f);
        }
        return;
    }

    g.setColour (Colours::black.withAlpha (0.55f * alpha));
    g.fillRoundedRectangle (r.translated (0.f, down ? 1.f : 2.5f), rad);
    if (b.getToggleState())
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (acc.withAlpha (0.13f * alpha));
            g.fillRoundedRectangle (r.expanded ((float) i), rad + (float) i);
        }
        g.setGradientFill (ColourGradient (acc.brighter (0.35f).withMultipliedAlpha (alpha), r.getX(), r.getY(),
                                           acc.darker (0.5f).withMultipliedAlpha (alpha), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (Colours::white.withAlpha (0.3f * alpha));
        g.drawLine (r.getX() + rad, r.getY() + 1.f, r.getRight() - rad, r.getY() + 1.f, 1.f);
    }
    else
    {
        g.setGradientFill (ColourGradient (Colour (0xff151b26).brighter (hover ? 0.15f : 0.f).withMultipliedAlpha (alpha), r.getX(), r.getY(),
                                           Colour (0xff07090d).withMultipliedAlpha (alpha), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (Colours::white.withAlpha (0.07f * alpha));
        g.drawLine (r.getX() + rad, r.getY() + 1.f, r.getRight() - rad, r.getY() + 1.f, 1.f);
        g.setColour (acc.withAlpha ((hover ? 0.85f : 0.38f) * alpha));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.f);
    }
    if (down)
    {
        g.setColour (Colours::black.withAlpha (0.2f));
        g.fillRoundedRectangle (r, rad);
    }
}

void HitrLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    auto col = b.findColour (b.getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId);
    if (b.getProperties().contains ("tab")) col = b.getToggleState() ? Colours::white : hui::dim;
    else if (b.getToggleState() && b.findColour (TextButton::buttonOnColourId).getPerceivedBrightness() > 0.7f)
        col = Colour (0xff05080d);
    g.setColour (col.withMultipliedAlpha (b.isEnabled() ? 1.f : 0.4f));
    const float h = (float) b.getHeight();
    g.setFont (hui::font (jlimit (8.f, 15.f, h * 0.34f), true, 0.1f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 2), Justification::centred, 1, 0.55f);
}

void HitrLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (3.f);
    const float rad = jmin (6.f, r.getHeight() * 0.24f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.f, 2.5f), rad);
    g.setGradientFill (ColourGradient (Colour (0xff141a24), r.getX(), r.getY(), Colour (0xff07090d), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (accent.withAlpha (box.isMouseOver (true) ? 0.85f : 0.5f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.f);
    Path arrow;
    const float ax = r.getRight() - r.getHeight() * 0.5f - 6.f, ay = r.getCentreY(), as = jmax (3.f, r.getHeight() * 0.12f);
    arrow.startNewSubPath (ax - as, ay - as * 0.5f);
    arrow.lineTo (ax, ay + as * 0.5f);
    arrow.lineTo (ax + as, ay - as * 0.5f);
    g.setColour (hui::dim);
    g.strokePath (arrow, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
}

void HitrLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (box.getLocalBounds().reduced (box.getHeight() / 2, 1).withTrimmedRight (box.getHeight() / 3));
    label.setFont (getComboBoxFont (box));
}
Font HitrLookAndFeel::getComboBoxFont (ComboBox& b) { return hui::font (jlimit (9.f, 17.f, (float) b.getHeight() * 0.42f), true, 0.06f); }
Font HitrLookAndFeel::getPopupMenuFont() { return hui::font (15.f); }

//==============================================================================
Knob::Knob (AudioProcessorValueTreeState& s, const String& id, const String& t, const String& tip) : title (t)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (220);
    slider.setTooltip (tip);
    addAndMakeVisible (slider);
    att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* prm = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}
void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (jmax (14, getHeight() / 7));
    slider.setBounds (r);
}
void Knob::paint (Graphics& g)
{
    const int lh = jmax (14, getHeight() / 7);
    auto r = getLocalBounds().removeFromBottom (lh);
    g.setColour (hui::text.withAlpha (0.8f));
    g.setFont (hui::font (jlimit (8.f, 13.f, (float) lh * 0.72f), true, 0.16f));
    g.drawFittedText (title, r, Justification::centred, 1, 0.6f);
}

Pill::Pill (AudioProcessorValueTreeState& s, const String& id, const String& text, const String& tip)
{
    button.setButtonText (text);
    button.setClickingTogglesState (true);
    button.setTooltip (tip);
    addAndMakeVisible (button);
    att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, id, button);
}

Segmented::Segmented (AudioProcessorValueTreeState& s, const String& id, const String& tip, int cols) : columns (cols)
{
    auto* prm = dynamic_cast<AudioParameterChoice*> (s.getParameter (id));
    jassert (prm != nullptr);
    for (int i = 0; i < prm->choices.size(); ++i)
    {
        auto* b = buttons.add (new TextButton (prm->choices[i]));
        b->setTooltip (tip);
        b->onClick = [this, i] { attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    attachment = std::make_unique<ParameterAttachment> (*prm, [this] (float v)
    {
        const int idx = roundToInt (v);
        for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == idx, dontSendNotification);
    });
    attachment->sendInitialUpdate();
}
void Segmented::resized()
{
    const int n = buttons.size();
    const int cols = columns > 0 ? columns : n;
    const int rows = (n + cols - 1) / cols;
    const int w = getWidth() / cols, h = getHeight() / jmax (1, rows);
    for (int i = 0; i < n; ++i)
        buttons[i]->setBounds ((i % cols) * w, (i / cols) * h, w, h);
}

//==============================================================================
ModeTile::ModeTile (int i) : Button (String (i)), index (i)
{
    setTooltip (u (modes()[(size_t) i].name) + u (u8" — clic : substance principale · clic droit / Maj+clic : l'ajouter au mélange"));
}

void ModeTile::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu() || e.mods.isShiftDown())
    {
        if (onPick) onPick (true);
        return;
    }
    Button::mouseDown (e);
}

void ModeTile::paintButton (Graphics& g, bool hover, bool down)
{
    const auto& m = modes()[(size_t) index];
    const Colour c (m.c1), c2 (m.c2);
    auto r = getLocalBounds().toFloat().reduced (2.5f);
    const float rad = 6.f;

    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.f, down ? 1.f : 2.5f), rad);
    if (role == 1)
    {
        for (int i = 4; i >= 1; --i) { g.setColour (c.withAlpha (0.11f)); g.fillRoundedRectangle (r.expanded ((float) i), rad + (float) i); }
        g.setGradientFill (ColourGradient (c.brighter (0.25f), r.getX(), r.getY(), c2.darker (0.3f), r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawLine (r.getX() + rad, r.getY() + 1.f, r.getRight() - rad, r.getY() + 1.f, 1.f);
    }
    else
    {
        g.setGradientFill (ColourGradient (Colour (0xff161c27).brighter (hover ? 0.18f : 0.f), r.getX(), r.getY(),
                                           Colour (0xff06080b), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        // liseré de couleur de la substance
        g.setGradientFill (ColourGradient (c.withAlpha (0.55f), r.getX(), r.getY(), c2.withAlpha (0.0f), r.getX() + r.getWidth() * 0.7f, r.getY(), false));
        g.fillRoundedRectangle (r.withHeight (3.f), 1.5f);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (r, rad, 1.f);
        if (role >= 2)
        {
            g.setColour (c);
            g.drawRoundedRectangle (r.reduced (1.f), rad, 1.8f);
            glowBlob (g, c.withAlpha (0.35f), r.getRight() - 9.f, r.getY() + 9.f, 12.f);
            g.setColour (c);
            g.fillEllipse (r.getRight() - 16.f, r.getY() + 3.f, 13.f, 13.f);
            g.setColour (Colours::black);
            g.setFont (hui::font (9.f, true));
            g.drawText ("+" + String (role), Rectangle<float> (r.getRight() - 16.f, r.getY() + 3.f, 13.f, 13.f), Justification::centred);
        }
        else if (hover)
        {
            g.setColour (c.withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.f);
        }
    }
    const bool dark = role == 1 && c.getPerceivedBrightness() > 0.65f;
    g.setColour (dark ? Colour (0xff05080d) : (role == 1 ? Colours::white : hui::text.withAlpha (0.85f)));
    g.setFont (hui::font (jlimit (8.f, 12.5f, r.getHeight() * 0.24f), true, 0.08f));
    g.drawFittedText (u (m.shortName), r.reduced (4.f, 2.f).toNearestInt(), Justification::centred, 1, 0.6f);
}

//==============================================================================
LayerChip::LayerChip (HitrProcessor& p, int n) : proc (p), num (n)
{
    blend.setSliderStyle (Slider::LinearHorizontal);
    blend.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    blend.setTooltip (u (u8"Dosage de cette couche dans le mélange."));
    addAndMakeVisible (blend);
    att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, n == 2 ? "blend2" : "blend3", blend);
    removeBtn.setTooltip (u (u8"Retire cette substance du mélange."));
    removeBtn.onClick = [this] { proc.undo.beginNewTransaction(); proc.setParam (num == 2 ? "mode2" : "mode3", 0.f); };
    addAndMakeVisible (removeBtn);
    refresh();
}

void LayerChip::refresh()
{
    const int idx = layerIndex (proc, num);
    if (idx == shown) return;
    shown = idx;
    blend.setVisible (idx >= 0);
    removeBtn.setVisible (idx >= 0);
    if (idx >= 0)
    {
        const Colour c (modes()[(size_t) idx].c1);
        blend.setColour (Slider::thumbColourId, c);
        removeBtn.setColour (TextButton::buttonOnColourId, c);
    }
    repaint();
}

void LayerChip::resized()
{
    auto r = getLocalBounds().reduced (4);
    removeBtn.setBounds (r.removeFromRight (r.getHeight()).reduced (1));
    blend.setBounds (r.removeFromRight (r.getWidth() / 2).reduced (2, 0));
}

void LayerChip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.f);
    const int idx = layerIndex (proc, num);
    if (idx < 0)
    {
        Path p;
        p.addRoundedRectangle (r, 7.f);
        Path dashed;
        const float dash[] = { 4.f, 4.f };
        PathStrokeType (1.f).createDashedStroke (dashed, p, dash, 2);
        g.setColour (hui::dim.withAlpha (0.6f));
        g.fillPath (dashed);
        g.setFont (hui::font (10.5f, true, 0.08f));
        g.drawFittedText (u (u8"+ COUCHE ") + String (num) + u (u8" · clic droit sur une substance"), r.reduced (8.f, 2.f).toNearestInt(),
                          Justification::centred, 1, 0.6f);
        return;
    }
    const auto& m = modes()[(size_t) idx];
    const Colour c (m.c1);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0.f, 2.f), 7.f);
    g.setGradientFill (ColourGradient (c.withAlpha (0.28f), r.getX(), r.getY(), Colour (0xff07090d), r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 7.f);
    g.setColour (c.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 7.f, 1.2f);
    glowBlob (g, c.withAlpha (0.6f), r.getX() + 14.f, r.getCentreY(), 9.f);
    g.setColour (c);
    g.fillEllipse (r.getX() + 10.f, r.getCentreY() - 4.f, 8.f, 8.f);
    g.setColour (Colours::white);
    g.setFont (hui::font (11.5f, true, 0.1f));
    auto text = r.withTrimmedLeft (24.f).withWidth (r.getWidth() * 0.5f - 26.f);
    g.drawFittedText ("+ " + u (m.name), text.toNearestInt(), Justification::centredLeft, 1, 0.6f);
}

//==============================================================================
//  TRIP VIEW
//==============================================================================
namespace
{
struct Ctx
{
    Graphics& g;
    Rectangle<float> r;
    float t, lv, dose;
    Colour c[3];
    float cx() const { return r.getCentreX(); }
    float cy() const { return r.getCentreY(); }
    float maxR() const { return std::hypot (r.getWidth(), r.getHeight()) * 0.5f; }
};

void drawSmoke (Ctx& k)
{
    for (int i = 0; i < 14; ++i)
    {
        const float fi = (float) i;
        const float x = k.r.getX() + k.r.getWidth() * (0.5f + 0.45f * std::sin (k.t * 0.07f * (fi * 0.3f + 1.f) + fi * 1.7f));
        const float y = k.r.getY() + k.r.getHeight() * (0.5f + 0.42f * std::cos (k.t * 0.05f * (fi * 0.25f + 1.f) + fi));
        const float rad = jmin (k.r.getWidth(), k.r.getHeight()) * (0.28f + 0.14f * std::sin (k.t * 0.1f + fi)) * (1.f + k.lv * 0.5f);
        glowBlob (k.g, k.c[i % 3].withAlpha (0.10f + 0.16f * k.dose), x, y, rad);
    }
}
void drawTunnel (Ctx& k)
{
    const float cx = k.cx() + std::sin (k.t * 0.3f) * k.r.getWidth() * 0.06f;
    const float cy = k.cy() + std::cos (k.t * 0.23f) * k.r.getHeight() * 0.05f;
    const int n = 26;
    for (int i = 0; i < n; ++i)
    {
        const float z = std::fmod ((float) i / (float) n + k.t * 0.07f, 1.f);
        const float rad = std::pow (z, 2.3f) * k.maxR() * 1.5f;
        const float squash = 1.f + 0.12f * std::sin (k.t * 0.8f + (float) i);
        k.g.setColour (k.c[i % 3].withAlpha (jmin (1.f, z * 1.2f) * (0.4f + 0.5f * k.dose)));
        k.g.drawEllipse (cx - rad * squash, cy - rad / squash, rad * 2.f * squash, rad * 2.f / squash, 1.f + z * 7.f + k.lv * 3.f);
    }
    glowBlob (k.g, k.c[2].withAlpha (0.6f), cx, cy, 40.f + k.lv * 60.f);
    glowBlob (k.g, Colours::black, cx, cy, 22.f);
}
void drawKaleido (Ctx& k)
{
    const float R = jmin (k.r.getWidth(), k.r.getHeight()) * 0.55f;
    const float hue = k.t * 0.04f;
    const int n = 12;
    for (int layer = 0; layer < 4; ++layer)
    {
        const float dir = (layer % 2) ? 1.f : -1.f;
        const float len = R * (0.35f + 0.22f * (float) layer) * (1.f + 0.18f * std::sin (k.t * 0.9f + (float) layer) + k.lv * 0.25f);
        for (int i = 0; i < n; ++i)
        {
            const float ang = (float) i * MathConstants<float>::twoPi / (float) n + dir * k.t * 0.12f + (float) layer * 0.26f;
            Path petal;
            petal.addEllipse (-len * 0.09f, -len, len * 0.18f, len);
            k.g.setColour (k.c[(size_t) ((i + layer) % 3)].withRotatedHue (hue + (float) layer * 0.07f).withAlpha (0.22f + 0.2f * k.dose));
            k.g.fillPath (petal, AffineTransform::rotation (ang).translated (k.cx(), k.cy()));
        }
    }
    glowBlob (k.g, Colours::white.withAlpha (0.5f), k.cx(), k.cy(), 30.f + k.lv * 50.f);
}
void drawWaves (Ctx& k)
{
    const int lines = 18;
    for (int i = 0; i < lines; ++i)
    {
        Path p;
        const float baseY = k.r.getY() + k.r.getHeight() * ((float) i + 0.5f) / (float) lines;
        const float amp = k.r.getHeight() * (0.03f + 0.05f * k.dose) * (1.f + k.lv);
        bool first = true;
        for (float x = k.r.getX() - 20.f; x <= k.r.getRight() + 20.f; x += 8.f)
        {
            const float y = baseY + amp * std::sin (x * 0.012f + k.t * 1.1f + (float) i * 0.5f)
                                  + amp * 0.6f * std::sin (x * 0.004f - k.t * 0.5f + (float) i);
            if (first) { p.startNewSubPath (x, y); first = false; } else p.lineTo (x, y);
        }
        k.g.setColour (k.c[i % 3].withAlpha (0.18f));
        k.g.strokePath (p, PathStrokeType (10.f + k.lv * 8.f, PathStrokeType::curved));
        k.g.setColour (k.c[i % 3].withAlpha (0.65f));
        k.g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::curved));
    }
}
void drawGeometry (Ctx& k)
{
    const float R = jmin (k.r.getWidth(), k.r.getHeight()) * 0.5f;
    for (int layer = 0; layer < 9; ++layer)
    {
        const int sides = 3 + layer % 4;
        const float rad = R * (0.12f + 0.11f * (float) layer) * (1.f + 0.07f * std::sin (k.t * 2.f + (float) layer) + k.lv * 0.25f);
        const float rot = k.t * (0.15f + 0.05f * (float) layer) * ((layer % 2) ? -1.f : 1.f);
        Path poly, star;
        for (int s = 0; s <= sides; ++s)
        {
            const float a = rot + (float) s * MathConstants<float>::twoPi / (float) sides;
            const Point<float> pt (k.cx() + rad * std::cos (a), k.cy() + rad * std::sin (a));
            if (s == 0) poly.startNewSubPath (pt); else poly.lineTo (pt);
            star.startNewSubPath (k.cx(), k.cy());
            star.lineTo (pt);
        }
        k.g.setColour (k.c[layer % 3].withAlpha (0.12f));
        k.g.strokePath (star, PathStrokeType (1.f));
        k.g.setColour (k.c[layer % 3].withAlpha (0.25f));
        k.g.strokePath (poly, PathStrokeType (7.f));
        k.g.setColour (k.c[layer % 3].withAlpha (0.9f));
        k.g.strokePath (poly, PathStrokeType (1.5f));
    }
}
void drawPulse (Ctx& k)
{
    const int n = 9;
    for (int i = 0; i < n; ++i)
    {
        const float ph = std::fmod (k.t * 0.45f + (float) i / (float) n, 1.f);
        const float rad = ph * k.maxR() * 1.2f;
        k.g.setColour (k.c[i % 3].withAlpha ((1.f - ph) * (0.35f + 0.4f * k.dose)));
        k.g.drawEllipse (k.cx() - rad, k.cy() - rad, rad * 2.f, rad * 2.f, 2.f + 12.f * (1.f - ph));
    }
    const float beat = std::pow (0.5f + 0.5f * std::sin (k.t * MathConstants<float>::twoPi * 2.f), 4.f);
    glowBlob (k.g, k.c[0].withAlpha (0.7f), k.cx(), k.cy(), 50.f + 60.f * beat + k.lv * 80.f);
    glowBlob (k.g, k.c[1].withAlpha (0.4f), k.cx(), k.cy(), 140.f + k.lv * 120.f);
}
void drawStrobe (Ctx& k)
{
    for (int i = 0; i < 48; ++i)
    {
        const float fi = (float) i;
        const float x = k.r.getX() + std::fmod (fi * 37.3f + k.t * 260.f * (float) (i % 3 + 1), k.r.getWidth());
        const float on = std::sin (fi * 12.9f + k.t * 22.f);
        if (on < 0.2f) continue;
        k.g.setColour ((i % 4 == 0 ? k.c[2] : k.c[i % 2]).withAlpha (on * (0.3f + 0.5f * k.dose)));
        k.g.fillRect (x, k.r.getY(), 1.f + (float) (i % 3) * 1.5f, k.r.getHeight());
    }
    const float scanY = k.r.getY() + std::fmod (k.t * 420.f, k.r.getHeight());
    k.g.setColour (Colours::white.withAlpha (0.6f));
    k.g.fillRect (k.r.getX(), scanY, k.r.getWidth(), 2.f);
    k.g.setColour (k.c[0].withAlpha (jmin (0.45f, k.lv * 0.9f)));
    k.g.fillRect (k.r);
}
void drawGlitch (Ctx& k)
{
    Random rng ((int64) (k.t * 12.0));
    for (int i = 0; i < 40; ++i)
    {
        const float w = rng.nextFloat() * k.r.getWidth() * 0.4f;
        const float h = 2.f + rng.nextFloat() * k.r.getHeight() * 0.12f;
        const float x = k.r.getX() + rng.nextFloat() * k.r.getWidth() - w * 0.3f;
        const float y = k.r.getY() + rng.nextFloat() * k.r.getHeight();
        k.g.setColour (k.c[(size_t) rng.nextInt (3)].withAlpha (0.2f + 0.45f * rng.nextFloat() * k.dose));
        k.g.fillRect (x, y, w, h);
        k.g.setColour (Colour (0xffff0040).withAlpha (0.25f));
        k.g.fillRect (x + 6.f, y, w * 0.6f, 1.f);
        k.g.setColour (Colour (0xff00e5ff).withAlpha (0.25f));
        k.g.fillRect (x - 6.f, y + h, w * 0.6f, 1.f);
    }
    for (int i = 0; i < 400; ++i)
    {
        k.g.setColour (Colours::white.withAlpha (rng.nextFloat() * 0.35f));
        k.g.fillRect (k.r.getX() + rng.nextFloat() * k.r.getWidth(), k.r.getY() + rng.nextFloat() * k.r.getHeight(), 1.5f, 1.5f);
    }
}
void drawMelt (Ctx& k)
{
    const int cols = 34;
    const float cw = k.r.getWidth() / (float) cols;
    for (int i = 0; i < cols; ++i)
    {
        const float fi = (float) i;
        const float len = k.r.getHeight() * (0.25f + 0.45f * (0.5f + 0.5f * std::sin (k.t * 0.35f + fi * 0.7f)))
                          * (0.7f + 0.3f * k.dose) * (1.f + k.lv * 0.3f);
        const float x = k.r.getX() + fi * cw;
        const auto col = k.c[i % 3];
        k.g.setGradientFill (ColourGradient (col.withAlpha (0.85f), x, k.r.getY(), col.withAlpha (0.05f), x, k.r.getY() + len, false));
        k.g.fillRoundedRectangle (x, k.r.getY() - cw, cw + 1.f, len + cw, cw * 0.5f);
        glowBlob (k.g, col.withAlpha (0.6f), x + cw * 0.5f, k.r.getY() + len, cw * 0.9f);
    }
}
void drawRays (Ctx& k)
{
    const int n = 36;
    const float R = k.maxR() * 1.3f;
    for (int i = 0; i < n; ++i)
    {
        const float a = (float) i * MathConstants<float>::twoPi / (float) n + k.t * 0.12f;
        const float half = MathConstants<float>::pi / (float) n * (0.5f + 0.3f * std::sin (k.t + (float) i));
        Path ray;
        ray.startNewSubPath (k.cx(), k.cy());
        ray.lineTo (k.cx() + R * std::cos (a - half), k.cy() + R * std::sin (a - half));
        ray.lineTo (k.cx() + R * std::cos (a + half), k.cy() + R * std::sin (a + half));
        ray.closeSubPath();
        k.g.setColour (k.c[i % 2].withAlpha (0.16f + 0.2f * k.dose));
        k.g.fillPath (ray);
    }
    glowBlob (k.g, k.c[2].withAlpha (0.9f), k.cx(), k.cy(), 70.f + k.lv * 90.f);
    glowBlob (k.g, k.c[0].withAlpha (0.4f), k.cx(), k.cy(), 180.f);
}
void drawVortex (Ctx& k)
{
    const int arms = 6;
    for (int a = 0; a < arms; ++a)
    {
        Path p;
        for (int s = 0; s < 140; ++s)
        {
            const float fs = (float) s;
            const float th = fs * 0.075f + (float) a * MathConstants<float>::twoPi / (float) arms + k.t * 0.6f;
            const float rad = 3.f * std::exp (fs * 0.034f) * (1.f + k.lv * 0.2f);
            const float x = k.cx() + rad * std::cos (th), y = k.cy() + rad * std::sin (th);
            if (s == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        k.g.setColour (k.c[a % 3].withAlpha (0.2f));
        k.g.strokePath (p, PathStrokeType (14.f, PathStrokeType::curved, PathStrokeType::rounded));
        k.g.setColour (k.c[a % 3].withAlpha (0.8f));
        k.g.strokePath (p, PathStrokeType (2.f + k.dose * 2.f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    glowBlob (k.g, Colours::black, k.cx(), k.cy(), 26.f);
}
// 1.2 : aurore boréale
void drawAurora (Ctx& k)
{
    for (int c = 0; c < 6; ++c)
    {
        Path p;
        const float baseY = k.r.getY() + k.r.getHeight() * (0.15f + 0.1f * (float) c);
        p.startNewSubPath (k.r.getX(), k.r.getBottom());
        for (float x = k.r.getX(); x <= k.r.getRight() + 10.f; x += 10.f)
        {
            const float y = baseY + 30.f * std::sin (x * 0.008f + k.t * (0.4f + 0.1f * (float) c) + (float) c)
                                  + 14.f * std::sin (x * 0.021f - k.t * 0.7f) * (1.f + k.lv);
            p.lineTo (x, y);
        }
        p.lineTo (k.r.getRight(), k.r.getBottom());
        p.closeSubPath();
        const auto col = k.c[c % 3];
        k.g.setGradientFill (ColourGradient (col.withAlpha (0.35f * (0.5f + 0.5f * k.dose)), 0, baseY - 30.f,
                                             col.withAlpha (0.0f), 0, baseY + k.r.getHeight() * 0.45f, false));
        k.g.fillPath (p);
    }
}
// 1.2 : grille qui se déforme
void drawLattice (Ctx& k)
{
    const int nx = 22, ny = 12;
    auto pt = [&] (int i, int j)
    {
        const float u0 = (float) i / (float) (nx - 1), v0 = (float) j / (float) (ny - 1);
        const float dx = 18.f * std::sin (v0 * 6.f + k.t * 1.3f) * (0.5f + k.dose) * (1.f + k.lv);
        const float dy = 14.f * std::cos (u0 * 7.f - k.t * 1.1f) * (0.5f + k.dose);
        return Point<float> (k.r.getX() + u0 * k.r.getWidth() + dx, k.r.getY() + v0 * k.r.getHeight() + dy);
    };
    for (int j = 0; j < ny; ++j)
    {
        Path p;
        for (int i = 0; i < nx; ++i) { auto q = pt (i, j); if (i == 0) p.startNewSubPath (q); else p.lineTo (q); }
        k.g.setColour (k.c[j % 3].withAlpha (0.55f));
        k.g.strokePath (p, PathStrokeType (1.3f, PathStrokeType::curved));
    }
    for (int i = 0; i < nx; ++i)
    {
        Path p;
        for (int j = 0; j < ny; ++j) { auto q = pt (i, j); if (j == 0) p.startNewSubPath (q); else p.lineTo (q); }
        k.g.setColour (k.c[(i + 1) % 3].withAlpha (0.35f));
        k.g.strokePath (p, PathStrokeType (1.f, PathStrokeType::curved));
    }
    for (int i = 0; i < nx; i += 3)
        for (int j = 0; j < ny; j += 2)
        {
            auto q = pt (i, j);
            glowBlob (k.g, k.c[2].withAlpha (0.4f * (0.5f + 0.5f * std::sin (k.t * 3.f + (float) (i + j)))), q.x, q.y, 9.f);
        }
}
// 1.2 : mandala
void drawMandala (Ctx& k)
{
    const float R = jmin (k.r.getWidth(), k.r.getHeight()) * 0.5f;
    const int counts[] = { 6, 10, 14, 18, 24 };
    for (int ring = 0; ring < 5; ++ring)
    {
        const int n = counts[ring];
        const float rad = R * (0.18f + 0.17f * (float) ring) * (1.f + 0.05f * std::sin (k.t * 1.5f + (float) ring) + k.lv * 0.15f);
        const float rot = k.t * 0.1f * ((ring % 2) ? -1.f : 1.f) * (1.f + 0.3f * (float) ring);
        for (int i = 0; i < n; ++i)
        {
            const float a = rot + (float) i * MathConstants<float>::twoPi / (float) n;
            Path petal;
            const float pl = R * 0.16f, pw = pl * 0.45f;
            petal.addEllipse (-pw * 0.5f, -pl * 0.5f, pw, pl);
            const auto tr = AffineTransform::rotation (a + MathConstants<float>::halfPi)
                                .translated (k.cx() + rad * std::cos (a), k.cy() + rad * std::sin (a));
            k.g.setColour (k.c[(size_t) ((ring + i) % 3)].withAlpha (0.25f + 0.2f * k.dose));
            k.g.fillPath (petal, tr);
            k.g.setColour (k.c[(size_t) ((ring + 1) % 3)].withAlpha (0.7f));
            k.g.strokePath (petal, PathStrokeType (1.f), tr);
        }
        k.g.setColour (k.c[ring % 3].withAlpha (0.3f));
        k.g.drawEllipse (k.cx() - rad, k.cy() - rad, rad * 2.f, rad * 2.f, 1.f);
    }
    glowBlob (k.g, k.c[2].withAlpha (0.8f), k.cx(), k.cy(), 26.f + k.lv * 30.f);
}
// 1.2 : pluie et ondes
void drawRain (Ctx& k)
{
    Random rng (31);
    for (int i = 0; i < 90; ++i)
    {
        const float x = k.r.getX() + rng.nextFloat() * k.r.getWidth();
        const float sp = 80.f + rng.nextFloat() * 160.f;
        const float len = 20.f + rng.nextFloat() * 50.f;
        const float y = k.r.getY() - len + std::fmod (rng.nextFloat() * k.r.getHeight() + k.t * sp, k.r.getHeight() + len);
        k.g.setGradientFill (ColourGradient (k.c[i % 3].withAlpha (0.0f), x, y, k.c[i % 3].withAlpha (0.7f), x, y + len, false));
        k.g.fillRect (x, y, 1.4f, len);
    }
    for (int i = 0; i < 7; ++i)
    {
        const float ph = std::fmod (k.t * 0.35f + (float) i / 7.f, 1.f);
        const float x = k.r.getX() + k.r.getWidth() * (0.1f + 0.8f * std::fmod ((float) i * 0.37f, 1.f));
        const float y = k.r.getBottom() - k.r.getHeight() * 0.18f;
        const float rad = ph * 90.f * (1.f + k.lv);
        k.g.setColour (k.c[i % 3].withAlpha ((1.f - ph) * 0.6f));
        k.g.drawEllipse (x - rad, y - rad * 0.3f, rad * 2.f, rad * 0.6f, 1.5f);
    }
}
void drawFarLayer (Ctx& k)
{
    Random rng (4242);
    for (int i = 0; i < 140; ++i)
    {
        const float x = k.r.getX() + std::fmod (rng.nextFloat() * k.r.getWidth() + k.t * (4.f + rng.nextFloat() * 6.f), k.r.getWidth());
        const float y = k.r.getY() + rng.nextFloat() * k.r.getHeight();
        const float tw = 0.4f + 0.6f * (0.5f + 0.5f * std::sin (k.t * (1.5f + rng.nextFloat() * 4.f) + (float) i));
        const float s = 0.8f + rng.nextFloat() * 1.4f;
        k.g.setColour (k.c[i % 3].interpolatedWith (Colours::white, 0.6f).withAlpha (tw * 0.55f));
        k.g.fillEllipse (x, y, s, s);
    }
}
void drawNearLayer (Ctx& k)
{
    Random rng (777);
    for (int i = 0; i < 10; ++i)
    {
        const float sp = 10.f + rng.nextFloat() * 25.f;
        const float x = k.r.getX() - 60.f + std::fmod (rng.nextFloat() * (k.r.getWidth() + 120.f) + k.t * sp, k.r.getWidth() + 120.f);
        const float y = k.r.getY() + k.r.getHeight() * (0.1f + 0.8f * rng.nextFloat()) + std::sin (k.t * 0.6f + (float) i) * 12.f;
        const float rad = 14.f + rng.nextFloat() * 34.f + k.lv * 10.f;
        glowBlob (k.g, k.c[i % 3].withAlpha (0.10f + 0.10f * k.dose), x, y, rad);
        k.g.setColour (k.c[i % 3].withAlpha (0.08f));
        k.g.drawEllipse (x - rad * 0.6f, y - rad * 0.6f, rad * 1.2f, rad * 1.2f, 1.f);
    }
}
void drawVisual (Ctx& k, Visual v)
{
    switch (v)
    {
        case Visual::Smoke:    drawSmoke (k); break;
        case Visual::Tunnel:   drawTunnel (k); break;
        case Visual::Kaleido:  drawKaleido (k); break;
        case Visual::Waves:    drawWaves (k); break;
        case Visual::Geometry: drawGeometry (k); break;
        case Visual::Pulse:    drawPulse (k); break;
        case Visual::Strobe:   drawStrobe (k); break;
        case Visual::Glitch:   drawGlitch (k); break;
        case Visual::Melt:     drawMelt (k); break;
        case Visual::Rays:     drawRays (k); break;
        case Visual::Vortex:   drawVortex (k); break;
        case Visual::Aurora:   drawAurora (k); break;
        case Visual::Lattice:  drawLattice (k); break;
        case Visual::Mandala:  drawMandala (k); break;
        case Visual::Rain:     drawRain (k); break;
    }
}
} // namespace

void TripView::mouseMove (const MouseEvent& e)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    mouseTarget = { (e.position.x - c.x) / jmax (1.f, (float) getWidth()), (e.position.y - c.y) / jmax (1.f, (float) getHeight()) };
}

void TripView::timerCallback()
{
    const bool sober = param (proc, "sobriety") > 0.5f;
    const bool od = param (proc, "overdose") > 0.5f;
    const float speed = param (proc, "trip") * (od ? 1.8f : 1.f) * (sober ? 0.08f : 1.f);
    t += (1.0 / 40.0) * (double) speed;
    level += (jmin (1.f, proc.outputLevel.load() * 4.f) - level) * 0.25f;
    parallax += (mouseTarget - parallax) * 0.08f;
    repaint();
}

void TripView::paint (Graphics& g)
{
    const auto& m = currentMode (proc);
    const int idx = currentModeIndex (proc);
    const float dose = param (proc, "dose");
    const bool od = param (proc, "overdose") > 0.5f;
    const bool sober = param (proc, "sobriety") > 0.5f;
    const float doseV = jmin (1.5f, dose * (od ? 1.6f : 1.f));
    auto r = getLocalBounds().toFloat();

    g.fillAll (Colour (0xff020305));
    const Point<float> shake = od ? Point<float> ((rng.nextFloat() - 0.5f) * 10.f * (0.4f + level), (rng.nextFloat() - 0.5f) * 8.f * (0.4f + level)) : Point<float>();

    auto layer = [&] (float depth, const Mode& md, float alpha, auto&& fn)
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (getLocalBounds());
        const float ox = -parallax.x * depth * 40.f + shake.x * depth, oy = -parallax.y * depth * 26.f + shake.y * depth;
        const float zoom = 1.f + depth * 0.04f + level * 0.03f * depth;
        g.addTransform (AffineTransform::scale (zoom, zoom, r.getCentreX(), r.getCentreY()).translated (ox, oy));
        if (alpha < 0.999f) g.beginTransparencyLayer (alpha);
        Ctx k { g, r, (float) t, level, doseV, { Colour (md.c1), Colour (md.c2), Colour (md.c3) } };
        fn (k);
        if (alpha < 0.999f) g.endTransparencyLayer();
    };

    layer (0.2f, m, 1.f, [] (Ctx& k) { drawFarLayer (k); });
    layer (0.6f, m, 1.f, [&] (Ctx& k) { drawVisual (k, m.visual); });
    // couches de mélange : leurs visuels se superposent
    String title = u (m.name);
    for (int n = 2; n <= 3; ++n)
    {
        const int li = layerIndex (proc, n);
        if (li < 0) continue;
        const auto& lm = modes()[(size_t) li];
        const float a = jlimit (0.15f, 0.85f, param (proc, n == 2 ? "blend2" : "blend3") * 0.9f);
        layer (0.75f, lm, a, [&] (Ctx& k) { drawVisual (k, lm.visual); });
        title << " + " << u (lm.name);
    }

    g.setGradientFill (ColourGradient (Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                       Colours::black.withAlpha (0.88f), r.getX(), r.getY(), true));
    g.fillRect (r);
    layer (1.4f, m, 1.f, [] (Ctx& k) { drawNearLayer (k); });

    g.setColour (Colours::black.withAlpha (0.16f));
    for (float y = 0; y < r.getHeight(); y += 3.f)
        g.fillRect (0.f, y, r.getWidth(), 1.f);

    if (od)   // OVERDOSE : flash rouge qui bat, vignette sang
    {
        const float beat = std::pow (0.5f + 0.5f * std::sin ((float) t * 9.f), 3.f);
        g.setGradientFill (ColourGradient (Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                           hui::danger.withAlpha (0.55f + 0.3f * beat), r.getX(), r.getY(), true));
        g.fillRect (r);
        g.setColour (hui::danger.withAlpha (0.08f * beat));
        g.fillRect (r);
    }
    if (sober)   // SOBRIÉTÉ : tout s'éteint, gris calme
    {
        g.setColour (Colour (0xff0b0d10).withAlpha (0.82f));
        g.fillRect (r);
    }

    // titre (substance ou mélange), en relief
    const bool merged = title.contains (" + ");
    auto nameArea = r.withSizeKeepingCentre (r.getWidth() - 40.f, r.getHeight() * 0.3f)
                     .translated (-parallax.x * 18.f + shake.x, -r.getHeight() * 0.05f - parallax.y * 12.f + shake.y);
    const String shownTitle = sober ? u (u8"SOBRIÉTÉ") : title;
    const float fh = jmin (r.getHeight() * (merged ? 0.11f : 0.17f), r.getWidth() / (float) jmax (6, shownTitle.length()) * 1.25f);
    g.setFont (hui::font (fh, true, 0.18f));
    const Colour c1 (m.c1), c2 (m.c2);
    if (! sober)
    {
        const float wob = std::sin ((float) t * 1.7f) * 2.f * dose;
        for (int i = 6; i >= 1; --i)
        {
            g.setColour (c1.withAlpha (0.07f));
            g.drawText (shownTitle, nameArea.translated ((float) i * 0.8f + wob, 0), Justification::centred);
            g.drawText (shownTitle, nameArea.translated (-(float) i * 0.8f - wob, 0), Justification::centred);
        }
        for (int i = 5; i >= 1; --i)
        {
            g.setColour (c2.darker (0.6f).withAlpha (0.55f));
            g.drawText (shownTitle, nameArea.translated ((float) i * 0.7f + parallax.x * (float) i, (float) i * 0.9f + parallax.y * (float) i), Justification::centred);
        }
        if (od)   // aberration chromatique
        {
            g.setColour (Colour (0xffff0040).withAlpha (0.7f));
            g.drawText (shownTitle, nameArea.translated (-4.f, 0.f), Justification::centred);
            g.setColour (Colour (0xff00e5ff).withAlpha (0.7f));
            g.drawText (shownTitle, nameArea.translated (4.f, 0.f), Justification::centred);
        }
    }
    g.setColour (sober ? hui::text.withAlpha (0.7f) : Colours::white);
    g.drawText (shownTitle, nameArea, Justification::centred);

    g.setColour (hui::text.withAlpha (0.88f));
    g.setFont (hui::font (13.5f, false, 0.02f).italicised());
    g.drawFittedText (sober ? u (u8"Le plugin est en pause : tu entends le son d'origine, sans aucune substance.") : u (m.desc),
                      r.reduced (40.f, 14.f).removeFromBottom (46.f).toNearestInt(), Justification::centred, 3, 0.9f);

    g.setFont (hui::font (11.f, true, 0.2f));
    g.setColour (hui::dim);
    g.drawText (String (idx + 1).paddedLeft ('0', 2) + " / " + String (numModes), r.reduced (14.f, 10.f), Justification::topLeft);
    g.setColour (od ? hui::danger : hui::dim);
    g.drawText (od ? String ("OVERDOSE  " + String (roundToInt (doseV * 100.f)) + " %") : "DOSE " + String (roundToInt (dose * 100.f)) + " %",
                r.reduced (14.f, 10.f), Justification::topRight);
    if (od && std::fmod (t * 2.0, 1.0) < 0.6)
    {
        g.setColour (hui::danger);
        g.setFont (hui::font (12.f, true, 0.5f));
        g.drawText (u (u8"⚠  OVERDOSE  ⚠"), r.withHeight (34.f), Justification::centred);
    }

    for (int i = 0; i < 10; ++i)
    {
        g.setColour (Colours::black.withAlpha (0.22f - (float) i * 0.02f));
        g.drawRect (r.reduced ((float) i), 1.f);
    }
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.07f), r.getX(), r.getY(),
                                       Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.35f, r.getY() + r.getHeight() * 0.45f, false));
    Path glass;
    glass.addTriangle (r.getX(), r.getY(), r.getX() + r.getWidth() * 0.7f, r.getY(), r.getX(), r.getY() + r.getHeight() * 0.8f);
    g.fillPath (glass);
}

//==============================================================================
void EffectsView::paint (Graphics& g)
{
    const auto& m = currentMode (proc);
    const float dose = param (proc, "dose") * (param (proc, "overdose") > 0.5f ? 1.6f : 1.f);
    const float grain = param (proc, "grain");
    const bool transpose = param (proc, "transpose") > 0.5f;
    int layers[2] = { layerIndex (proc, 2), layerIndex (proc, 3) };
    float w[2] = { param (proc, "blend2"), param (proc, "blend3") };
    const auto v = mergeModes (currentModeIndex (proc), layers, w, 2);
    auto clamp01 = [] (float x) { return jlimit (0.f, 1.f, x); };

    struct Row { const char* label; float value; };
    const Row rows[] =
    {
        { u8"HAUTEUR",      clamp01 ((transpose ? std::abs (v[PITCH]) / 5.f : 0.f) + v[SHIMMER] * 0.4f) },
        { u8"TEMPS",        clamp01 (v[WOW] / 18.f + v[STUTTER] + v[DLY] * 0.5f) },
        { u8"ESPACE",       clamp01 (v[VERB] + v[SHIMMER] * 0.5f + v[DLY] * 0.6f) },
        { u8"COULEURS",     clamp01 (v[PHASER] + v[CHORUS] * 0.6f + v[FLANGE] * 0.7f) },
        { u8"FILTRE",       clamp01 (v[LP] + v[HP] + v[FLFO]) },
        { u8"GRAIN",        clamp01 (v[DRIVE] + v[CRUSH] + v[RING]) },
        { u8"GRANULAR",     clamp01 (v[G_MIX] * grain * 2.f / jmax (0.05f, dose)) },
        { u8"PULSATION",    clamp01 (v[TREM] + v[PAN] * 0.7f) },
        { u8"DISSOCIATION", clamp01 (v[DROPOUT] * 2.f + std::abs (v[WIDTH] - 1.f) * 0.4f + v[CHAOS] * 0.5f + v[STUTTER] * 0.3f) },
    };
    const int num = (int) (sizeof (rows) / sizeof (rows[0]));
    auto r = getLocalBounds().toFloat();
    const float rowH = r.getHeight() / (float) num;
    for (int i = 0; i < num; ++i)
    {
        auto row = r.removeFromTop (rowH).reduced (0.f, rowH * 0.2f);
        auto label = row.removeFromLeft (104.f);
        g.setColour (hui::text.withAlpha (0.75f));
        g.setFont (hui::font (jmin (10.f, rowH * 0.75f), true, 0.14f));
        g.drawText (u (rows[i].label), label, Justification::centredLeft);
        auto bar = row.withTrimmedRight (4.f);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (bar.translated (0.f, -1.f), 3.f);
        g.setColour (hui::track);
        g.fillRoundedRectangle (bar.translated (0.f, 0.5f), 3.f);
        const float wiggle = 1.f + 0.04f * std::sin ((float) t * 3.f + (float) i * 1.3f);
        const float val = jlimit (0.f, 1.f, rows[i].value * dose * wiggle);
        if (val > 0.002f)
        {
            auto fillR = bar.withWidth (bar.getWidth() * val);
            g.setColour (Colour (m.c1).withAlpha (0.18f));
            g.fillRoundedRectangle (fillR.expanded (0.f, 2.f), 4.f);
            g.setGradientFill (ColourGradient (Colour (m.c2), fillR.getX(), 0, Colour (m.c1), fillR.getRight(), 0, false));
            g.fillRoundedRectangle (fillR, 3.f);
            g.setColour (Colours::white.withAlpha (0.25f));
            g.fillRect (fillR.withHeight (1.f).reduced (2.f, 0.f));
        }
    }
}

//==============================================================================
void ThermalView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.f);
    const float heat = jlimit (0.f, 1.f, param (proc, "heat") + (param (proc, "overdose") > 0.5f ? 0.35f : 0.f));
    const int type = (int) param (proc, "thermType");
    const float mod = proc.thermalMod.load();
    const Colour hot = Colour (0xffffb000).interpolatedWith (Colour (0xffff2a3d), heat);

    g.setColour (Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (r, 8.f);
    // braises
    for (int i = 0; i < 18; ++i)
    {
        const float x = r.getX() + r.getWidth() * std::fmod ((float) i * 0.137f + 0.05f * std::sin ((float) t + (float) i), 1.f);
        const float y = r.getBottom() - std::fmod ((float) t * (20.f + (float) i * 3.f) + (float) i * 17.f, r.getHeight());
        glowBlob (g, hot.withAlpha (0.25f * heat), x, y, 6.f + 10.f * heat);
    }
    g.setColour (Colours::white.withAlpha (0.06f));
    for (int i = 1; i < 4; ++i)
    {
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 4.f), r.getX(), r.getRight());
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 4.f), r.getY(), r.getBottom());
    }
    Path curve, lin;
    const float h = jlimit (0.f, 1.5f, heat * (1.f + 0.6f * mod));
    for (int i = 0; i <= 120; ++i)
    {
        const float x = -1.f + 2.f * (float) i / 120.f;
        const float y = heat > 0.001f ? (float) gdsp::Thermal::curve (type, x, h) : x;
        const Point<float> p (r.getX() + (x + 1.f) * 0.5f * r.getWidth(), r.getCentreY() - y * r.getHeight() * 0.42f);
        if (i == 0) curve.startNewSubPath (p); else curve.lineTo (p);
        const Point<float> pl (p.x, r.getCentreY() - x * r.getHeight() * 0.42f);
        if (i == 0) lin.startNewSubPath (pl); else lin.lineTo (pl);
    }
    g.setColour (Colours::white.withAlpha (0.12f));
    g.strokePath (lin, PathStrokeType (1.f));
    g.setColour (hot.withAlpha (0.25f));
    g.strokePath (curve, PathStrokeType (8.f, PathStrokeType::curved));
    g.setColour (hot);
    g.strokePath (curve, PathStrokeType (2.f, PathStrokeType::curved));
    g.setColour (hui::text.withAlpha (0.8f));
    g.setFont (hui::font (10.f, true, 0.2f));
    g.drawText (HitrProcessor::thermalTypeNames()[jlimit (0, 5, type)] + "  " + String (roundToInt (heat * 100.f)) + " %",
                r.reduced (8.f, 6.f), Justification::topLeft);
    // jauge de mouvement
    auto mv = r.reduced (8.f).removeFromBottom (5.f);
    g.setColour (hui::track);
    g.fillRoundedRectangle (mv, 2.f);
    g.setColour (hot);
    const float cx = mv.getCentreX() + mod * mv.getWidth() * 0.5f;
    g.fillEllipse (cx - 4.f, mv.getCentreY() - 4.f, 8.f, 8.f);
}

void ClipMeter::timerCallback()
{
    const float v = -proc.clipGrDb.load();
    disp = v > disp ? v : disp + (v - disp) * 0.15f;
    if (v >= hold) { hold = v; holdT = 40; }
    else if (--holdT < 0) hold += (v - hold) * 0.1f;
    repaint();
}

void ClipMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (hui::text.withAlpha (0.75f));
    g.setFont (hui::font (10.f, true, 0.18f));
    g.drawText ("GR  -" + String (disp, 1) + " dB", r.removeFromTop (14.f), Justification::centredLeft);
    auto bar = r.reduced (0.f, 3.f);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (bar, 4.f);
    const float frac = jlimit (0.f, 1.f, disp / 12.f);
    auto f = bar.withWidth (bar.getWidth() * frac);
    g.setGradientFill (ColourGradient (Colour (0xffffb000), f.getX(), 0, hui::danger, bar.getRight(), 0, false));
    g.fillRoundedRectangle (f, 4.f);
    const float hx = bar.getX() + bar.getWidth() * jlimit (0.f, 1.f, hold / 12.f);
    g.setColour (Colours::white);
    g.fillRect (hx - 1.f, bar.getY(), 2.f, bar.getHeight());
    g.setColour (Colours::white.withAlpha (0.15f));
    for (int i = 1; i < 6; ++i) g.drawVerticalLine ((int) (bar.getX() + bar.getWidth() * (float) i / 6.f), bar.getY(), bar.getBottom());
}

//==============================================================================
// PAGE DES PRESETS (reprise de ravenn)
PresetBrowser::PresetBrowser (HitrProcessor& p) : proc (p)
{
    search.setTextToShowWhenEmpty (u (u8"Rechercher un preset…"), hui::dim);
    search.setFont (hui::font (15.f));
    search.onTextChange = [this] { rebuildTiles(); };
    addAndMakeVisible (search);
    closeBtn.setButtonText ("FERMER");
    folderBtn.setButtonText ("DOSSIER");
    deleteBtn.setButtonText ("SUPPR.");
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    saveBtn.onClick = [this] { if (onSave) onSave(); };
    importBtn.onClick = [this] { if (onImport) onImport(); };
    exportBtn.onClick = [this] { if (onExport) onExport(); };
    folderBtn.onClick = [this] { proc.presets.openFolder(); };
    deleteBtn.onClick = [this] { if (onDelete) onDelete(); };
    saveBtn.setTooltip (u (u8"Sauvegarde le réglage actuel dans tes presets."));
    importBtn.setTooltip (u (u8"Importe des fichiers .preset (copiés dans Documents/hitr/Presets)."));
    exportBtn.setTooltip (u (u8"Exporte le réglage actuel dans un fichier .preset, où tu veux (pour le partager)."));
    folderBtn.setTooltip (u (u8"Ouvre le dossier des presets."));
    deleteBtn.setTooltip (u (u8"Met ton preset actuel à la corbeille (les presets d'usine ne s'effacent pas)."));
    for (auto* b : { &closeBtn, &saveBtn, &importBtn, &exportBtn, &folderBtn, &deleteBtn }) addAndMakeVisible (b);
    closeBtn.setToggleState (true, dontSendNotification);
    refresh();
}

void PresetBrowser::syncSelection()
{
    const auto cur = proc.presets.getCurrent();
    for (auto* t : tiles) t->setToggleState (t->getButtonText() == cur, dontSendNotification);
}

void PresetBrowser::refresh()
{
    catBtns.clear();
    StringArray cats { "TOUS" };
    for (auto& [c, names] : proc.presets.getFactoryByCategory()) cats.add (c);
    if (! proc.presets.getUserNames().isEmpty()) cats.add ("MES PRESETS");
    for (auto& c : cats)
    {
        auto* b = catBtns.add (new TextButton (c));
        b->getProperties().set ("tab", true);
        b->setColour (TextButton::buttonOnColourId, accent);
        b->setToggleState (c == category, dontSendNotification);
        b->onClick = [this, c] { category = c; for (auto* x : catBtns) x->setToggleState (x->getButtonText() == c, dontSendNotification); rebuildTiles(); };
        addAndMakeVisible (b);
    }
    for (auto* b : { &closeBtn, &saveBtn, &importBtn, &exportBtn, &folderBtn, &deleteBtn })
        b->setColour (TextButton::buttonOnColourId, accent);
    rebuildTiles();
}

void PresetBrowser::rebuildTiles()
{
    tiles.clear();
    const auto q = search.getText().trim().toLowerCase();
    auto addTile = [&] (const String& name, const String& cat)
    {
        if (category != "TOUS" && category != cat) return;
        if (q.isNotEmpty() && ! name.toLowerCase().contains (q)) return;
        auto* b = tiles.add (new TextButton (name));
        b->setColour (TextButton::buttonOnColourId, accent);
        b->setToggleState (name == proc.presets.getCurrent(), dontSendNotification);
        b->setTooltip (cat);
        b->onClick = [this, name]
        {
            proc.undo.beginNewTransaction ("Preset");
            proc.presets.load (name);
            proc.undo.beginNewTransaction();
            syncSelection();
        };
        addAndMakeVisible (b);
    };
    for (auto& [c, names] : proc.presets.getFactoryByCategory())
        for (auto& n : names) addTile (n, c);
    for (auto& n : proc.presets.getUserNames()) addTile (n, "MES PRESETS");
    resized();
}

void PresetBrowser::resized()
{
    const float s = (float) getWidth() / 1320.f;
    auto px = [s] (float v) { return roundToInt (v * s); };
    panel = getLocalBounds().reduced (px (60), px (24));
    auto a = panel.reduced (px (18)).withTrimmedTop (px (26));
    auto top = a.removeFromTop (px (36));
    closeBtn.setBounds (top.removeFromRight (px (100)));
    top.removeFromRight (px (8));
    folderBtn.setBounds (top.removeFromRight (px (90)));
    deleteBtn.setBounds (top.removeFromRight (px (80)));
    exportBtn.setBounds (top.removeFromRight (px (80)));
    importBtn.setBounds (top.removeFromRight (px (80)));
    saveBtn.setBounds (top.removeFromRight (px (70)));
    top.removeFromRight (px (12));
    search.setBounds (top);
    a.removeFromTop (px (12));
    auto left = a.removeFromLeft (px (200));
    for (auto* b : catBtns) { b->setBounds (left.removeFromTop (px (34))); left.removeFromTop (px (4)); }
    a.removeFromLeft (px (16));
    tileArea = a;
    const int cols = 3, th = px (38), gap = px (8);
    const int tw = (a.getWidth() - gap * (cols - 1)) / cols;
    for (int i = 0; i < tiles.size(); ++i)
        tiles[i]->setBounds (a.getX() + (i % cols) * (tw + gap), a.getY() + (i / cols) * (th + gap), tw, th);
}

void PresetBrowser::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.74f));
    drawGlassPanel (g, panel, "PRESETS", accent, (float) getWidth() / 1320.f);
    if (tiles.isEmpty())
    {
        g.setColour (hui::dim);
        g.setFont (hui::font (13.f));
        g.drawText (u (u8"Aucun preset ne correspond."), tileArea, Justification::centredTop);
    }
}

//==============================================================================
//  ÉDITEUR
//==============================================================================
static constexpr float baseW = 1320.f, baseH = 880.f;

HitrEditor::HitrEditor (HitrProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      dose (p.apvts, "dose", "DOSE", u (u8"Intensité : de sobre à la dose complète des substances.")),
      trip (p.apvts, "trip", "TRIP", u (u8"Vitesse des mouvements (en BPM : multiplie la division).")),
      mix (p.apvts, "mix", "MIX", u (u8"Mélange son traité / son d'origine (à puissance constante).")),
      output (p.apvts, "output", "OUTPUT", u (u8"Volume de sortie.")),
      width (p.apvts, "width", "WIDTH", u (u8"Largeur stéréo : 0 % mono, 100 % normal, 200 % très large.")),
      widthFreq (p.apvts, "widthFreq", "W. FREQ", u (u8"Sous cette fréquence, les basses ne sont jamais élargies (restent solides en mono).")),
      overdose (p.apvts, "overdose", "OVERDOSE", u (u8"Tout passe au-delà de 100 % : plus de chaos, de chaleur, de grains et de vitesse.")),
      sobriety (p.apvts, "sobriety", u (u8"SOBRIÉTÉ"), u (u8"Bypass : tu entends le son d'origine (fondu sans clic, latence compensée).")),
      noLatency (p.apvts, "noLatency", "NO LATENCY", u (u8"Zéro latence : coupe le suréchantillonnage de THERMAL / FATTENER / CLIPPER.")),
      autoGain (p.apvts, "autoGain", "AUTO GAIN", u (u8"Ramène la sortie au volume de l'entrée, pour comparer honnêtement avec SOBRIÉTÉ.")),
      grain (p.apvts, "grain", "GRAIN", u (u8"Quantité de granulizer. 50 % = le dosage du mode.")),
      gsize (p.apvts, "gsize", "SIZE", u (u8"Taille des grains (multiplie celle du mode).")),
      gdens (p.apvts, "gdens", "DENSITY", u (u8"Densité des grains (calée sur le tempo en BPM).")),
      delayMs (p.apvts, "delayms", "TIME", u (u8"Temps de l'écho en millisecondes. AUTO = réglage de la substance.")),
      freeze (p.apvts, "freeze", "FREEZE", u (u8"Gèle le tampon du granulizer : les grains tournent sur l'instant figé.")),
      transpose (p.apvts, "transpose", "GAMME LIBRE", u (u8"Autorise les substances à changer la hauteur (et donc la gamme). Coupé par défaut.")),
      effects (p),
      heat (p.apvts, "heat", "HEAT", u (u8"Chaleur : quantité de distorsion THERMAL (2 étages).")),
      thermMove (p.apvts, "thermMove", "MOVE", u (u8"Mouvement : un LFO calé sur le tempo et l'enveloppe du son font bouger le drive et le filtre.")),
      thermTone (p.apvts, "thermTone", "TONE", u (u8"Couleur de THERMAL : sombre à gauche, brillant à droite.")),
      thermMix (p.apvts, "thermMix", "MIX", u (u8"Mélange de THERMAL.")),
      thermType (p.apvts, "thermType", u (u8"Caractère : TUBE chaud, FUZZ carré, FOLD repliement, RECTIFY octave, BITS numérique, MELT qui coule."), 3),
      thermRate (p.apvts, "thermRate", u (u8"Vitesse du mouvement, calée sur le tempo."), 3),
      thermalView (p),
      ottK (p.apvts, "ott", "DEPTH", u (u8"Compression multibande haut + bas (OTT de ravenn).")),
      ottTime (p.apvts, "ottTime", "TIME", u (u8"Vitesse de l'OTT : à gauche nerveux, à droite lent.")),
      ottLow (p.apvts, "ottLow", "LOW", u (u8"OTT sur les graves.")),
      ottMid (p.apvts, "ottMid", "MID", u (u8"OTT sur les médiums.")),
      ottHigh (p.apvts, "ottHigh", "HIGH", u (u8"OTT sur les aigus.")),
      fatten (p.apvts, "fatten", "FATTEN", u (u8"Sausage Fattener : écrase et sature tout ensemble, plus gras, plus dense.")),
      fatColor (p.apvts, "fatColor", "COLOR", u (u8"Couleur du Fattener : sombre et rond à gauche, brillant et mordant à droite.")),
      tone (p.apvts, "tone", "TONE", u (u8"Bascule le spectre autour de 1 kHz : + plus brillant, - plus chaud.")),
      toneLow (p.apvts, "toneLow", "LOW", u (u8"Low shelf à 100 Hz.")),
      toneAir (p.apvts, "toneAir", "AIR", u (u8"High shelf à 12 kHz.")),
      clip (p.apvts, "clip", "CLIPPER", u (u8"Pousse le son dans le clipper de ravenn.")),
      clipKnee (p.apvts, "clipKnee", "KNEE", u (u8"Genou du clip SOFT : 0 presque dur, 100 très doux.")),
      clipXLo (p.apvts, "clipXLo", "LOW | MID", u (u8"Coupure entre les bandes LOW et MID du clipper.")),
      clipXHi (p.apvts, "clipXHi", "MID | HIGH", u (u8"Coupure entre les bandes MID et HIGH du clipper.")),
      clipShape (p.apvts, "clipShape", u (u8"HARD = net · SOFT = genou doux · TUBE = saturation à lampe")),
      clipBand (p.apvts, "clipBand", u (u8"FULL = tout · LOW / MID / HIGH = clippe une seule bande")),
      os (p.apvts, "os", u (u8"Suréchantillonnage de THERMAL / FATTENER / CLIPPER : moins d'aliasing, un peu de latence.")),
      clipMS (p.apvts, "clipMS", "M/S", u (u8"Clippe le centre et les côtés séparément.")),
      body (p.apvts, "body", "BODY", u (u8"La couleur de base de ravenn : bosse à 60 Hz et Side +1 dB.")),
      clipMeter (p),
      layer2 (p, 2), layer3 (p, 3), tripView (p), browser (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);
    ravenImg = ImageCache::getFromMemory (BinaryData::raven_png, BinaryData::raven_pngSize);

    for (auto* c : std::initializer_list<Component*> { &dose, &trip, &mix, &output, &width, &widthFreq, &overdose, &sobriety,
                                                       &noLatency, &autoGain, &tripView, &layer2, &layer3 })
        addAndMakeVisible (c);
    overdose.button.setColour (TextButton::buttonOnColourId, hui::danger);

    // pages
    for (auto* c : std::initializer_list<Component*> { &grain, &gsize, &gdens, &freeze, &transpose, &bpmBtn, &msBtn, &divisionBox, &delayMs, &effects })
        place (*c, 0);
    for (auto* c : std::initializer_list<Component*> { &heat, &thermMove, &thermTone, &thermMix, &thermType, &thermRate, &thermalView })
        place (*c, 1);
    for (auto* c : std::initializer_list<Component*> { &ottK, &ottTime, &ottLow, &ottMid, &ottHigh, &fatten, &fatColor, &tone, &toneLow, &toneAir })
        place (*c, 2);
    for (auto* c : std::initializer_list<Component*> { &clip, &clipKnee, &clipXLo, &clipXHi, &clipShape, &clipBand, &os, &clipMS, &body, &clipMeter })
        place (*c, 3);

    // BPM / MS
    bpmBtn.setTooltip (u (u8"Effets temporels calés sur le tempo du projet."));
    msBtn.setTooltip (u (u8"Effets temporels libres, écho réglé en millisecondes."));
    if (auto* prm = p.apvts.getParameter ("sync"))
        syncAtt = std::make_unique<ParameterAttachment> (*prm, [this] (float v)
        {
            bpmBtn.setToggleState (v > 0.5f, dontSendNotification);
            msBtn.setToggleState (v <= 0.5f, dontSendNotification);
            setPage (page);
        });
    bpmBtn.onClick = [this] { syncAtt->setValueAsCompleteGesture (1.0f); };
    msBtn.onClick  = [this] { syncAtt->setValueAsCompleteGesture (0.0f); };
    divisionBox.addItemList (HitrProcessor::divisionNames(), 1);
    divisionBox.setTooltip (u (u8"Division de l'écho. AUTO = la plus proche du réglage de la substance."));
    divisionAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "division", divisionBox);

    // header
    presetNameBtn.setTooltip (u (u8"Ouvre la page des presets"));
    presetNameBtn.onClick = [this] { showPresetBrowser (! browser.isVisible()); };
    prevBtn.onClick = [this] { proc.undo.beginNewTransaction ("Preset"); proc.presets.previous(); proc.undo.beginNewTransaction(); };
    nextBtn.onClick = [this] { proc.undo.beginNewTransaction ("Preset"); proc.presets.next(); proc.undo.beginNewTransaction(); };
    saveBtn.setTooltip (u (u8"Sauvegarde le réglage actuel en preset"));
    saveBtn.onClick = [this] { savePreset(); };
    for (int i = 0; i < 4; ++i)
    {
        auto* b = slotBtns.add (new TextButton (String::charToString ((juce_wchar) ('A' + i))));
        b->setTooltip (u (u8"Slots de comparaison A / B / C / D : chaque slot garde ses propres réglages."));
        b->onClick = [this, i] { proc.selectSlot (i); };
        addAndMakeVisible (b);
    }
    copyBtn.setButtonText (u (u8"→"));
    copyBtn.setTooltip (u (u8"Copie le slot actuel vers un autre slot (ou tous)."));
    copyBtn.onClick = [this]
    {
        const int cur = proc.getABSlot();
        PopupMenu m;
        m.addSectionHeader (u (u8"Copier ") + String::charToString ((juce_wchar) ('A' + cur)) + u (u8" vers…"));
        for (int i = 0; i < 4; ++i)
            if (i != cur) m.addItem (i + 1, String::charToString ((juce_wchar) ('A' + i)) + (proc.slotHasState (i) ? u (u8"  (remplace)") : String()));
        m.addSeparator();
        m.addItem (10, u (u8"Tous les autres slots"));
        Component::SafePointer<HitrEditor> safe (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&copyBtn), [safe, cur] (int r)
        {
            if (safe == nullptr || r == 0) return;
            if (r == 10) { for (int i = 0; i < 4; ++i) if (i != cur) safe->proc.copySlotTo (i); }
            else safe->proc.copySlotTo (r - 1);
        });
    };
    undoBtn.setTooltip (u (u8"Annuler (Ctrl+Z)"));
    redoBtn.setTooltip (u (u8"Rétablir (Ctrl+Y)"));
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };
    randomBtn.setButtonText ("RANDOM");
    randomBtn.setTooltip (u (u8"Tire au sort substances, mélange, dose, granulizer, THERMAL, FATTENER, OTT et TONE. Ne touche pas au temps (BPM/MS, divisions, écho, TRIP) ni à la sortie."));
    randomBtn.onClick = [this] { proc.randomize(); };
    for (auto* b : { &prevBtn, &nextBtn, &presetNameBtn, &saveBtn, &copyBtn, &undoBtn, &redoBtn, &randomBtn })
        addAndMakeVisible (b);

    const char* tabNames[4] = { "TRIP", "THERMAL", "DYNAMICS", "CLIPPER" };
    for (int i = 0; i < 4; ++i)
    {
        auto* t = tabs.add (new TextButton (tabNames[i]));
        t->getProperties().set ("tab", true);
        t->onClick = [this, i] { setPage (i); };
        addAndMakeVisible (t);
    }

    for (int i = 0; i < numModes; ++i)
    {
        auto* t = tiles.add (new ModeTile (i));
        t->onClick = [this, i] { setMode (i); };
        t->onPick = [this, i] (bool) { proc.toggleLayer (i); };
        addAndMakeVisible (t);
    }

    addChildComponent (browser);
    browser.onClose = [this] { showPresetBrowser (false); };
    browser.onSave = [this] { savePreset(); };
    browser.onImport = [this] { importPresets(); };
    browser.onExport = [this] { exportPreset(); };
    browser.onDelete = [this]
    {
        if (proc.presets.remove (proc.presets.getCurrent())) refreshPresets();
    };

    syncAtt->sendInitialUpdate();
    setResizable (true, true);
    setResizeLimits ((int) (baseW * 0.7f), (int) (baseH * 0.7f), (int) (baseW * 1.6f), (int) (baseH * 1.6f));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (baseW / baseH);
    setSize ((int) baseW, (int) baseH);
    setPage (0);
    refreshPresets();
    applyModeColours();
    startTimerHz (20);
}

HitrEditor::~HitrEditor() { setLookAndFeel (nullptr); }

void HitrEditor::place (Component& c, int pg)
{
    addChildComponent (c);
    pageOf.push_back ({ &c, pg });
}

void HitrEditor::setPage (int pg)
{
    page = pg;
    const bool sync = param (proc, "sync") > 0.5f;
    for (auto& [c, p] : pageOf)
    {
        bool vis = p == page;
        if (c == &divisionBox) vis = vis && sync;
        if (c == &delayMs) vis = vis && ! sync;
        c->setVisible (vis);
    }
    for (int i = 0; i < tabs.size(); ++i) tabs[i]->setToggleState (i == page, dontSendNotification);
    static const char* titles[4][4] = { { "GRANULIZER", "TEMPS", "EFFETS SUR LE SON", "" },
                                        { "THERMAL", "", "", "" },
                                        { "OTT", "SAUSAGE FATTENER", "TONE", "" },
                                        { "CLIPPER", "", "", "" } };
    for (int i = 0; i < 4; ++i) sectionNames[(size_t) i] = titles[pg][i];
    resized();
    repaint();
}

void HitrEditor::showPresetBrowser (bool show)
{
    if (show)
    {
        browser.refresh();
        browser.setBounds (getLocalBounds().withTrimmedTop (roundToInt (70.f * (float) getWidth() / baseW)));
        browser.toFront (false);
    }
    browser.setVisible (show);
}

void HitrEditor::setMode (int idx)
{
    proc.undo.beginNewTransaction ("Mode");
    proc.setParam ("mode", (float) idx);
    // si la substance était une couche, elle quitte le mélange
    for (int n = 2; n <= 3; ++n)
        if ((int) param (proc, n == 2 ? "mode2" : "mode3") - 1 == idx) proc.setParam (n == 2 ? "mode2" : "mode3", 0.f);
}

bool HitrEditor::keyPressed (const KeyPress& k)
{
    const auto mods = k.getModifiers();
    if (mods.isCommandDown() && k.getKeyCode() == 'Z' && ! mods.isShiftDown()) { proc.undo.undo(); return true; }
    if (mods.isCommandDown() && (k.getKeyCode() == 'Y' || (k.getKeyCode() == 'Z' && mods.isShiftDown()))) { proc.undo.redo(); return true; }
    return false;
}

void HitrEditor::refreshPresets()
{
    shownPreset = proc.presets.getCurrent();
    presetNameBtn.setButtonText (shownPreset + u (u8"  ▾"));
    browser.refresh();
}

void HitrEditor::savePreset()
{
    auto* w = new AlertWindow (u (u8"Sauvegarder le preset"), u (u8"Nom du preset :"), MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presets.isFactory (proc.presets.getCurrent()) ? String() : proc.presets.getCurrent());
    w->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Annuler", 0, KeyPress (KeyPress::escapeKey));
    Component::SafePointer<HitrEditor> safe (this);
    w->enterModalState (true, ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result != 1 || safe == nullptr) return;
        if (safe->proc.presets.save (w->getTextEditorContents ("name")))
            safe->refreshPresets();
    }), true);
}

void HitrEditor::importPresets()
{
    chooser = std::make_unique<FileChooser> (u (u8"Importer des presets h.i.t.r"),
                                             File::getSpecialLocation (File::userHomeDirectory), "*.preset");
    Component::SafePointer<HitrEditor> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectMultipleItems,
                          [safe] (const FileChooser& fc)
    {
        if (safe == nullptr) return;
        if (safe->proc.presets.importFiles (fc.getResults()) > 0)
            safe->refreshPresets();
    });
}

void HitrEditor::exportPreset()
{
    chooser = std::make_unique<FileChooser> (u (u8"Exporter le preset"),
                                             File::getSpecialLocation (File::userDocumentsDirectory)
                                                 .getChildFile (File::createLegalFileName (proc.presets.getCurrent()) + ".preset"),
                                             "*.preset");
    Component::SafePointer<HitrEditor> safe (this);
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const FileChooser& fc)
    {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (f != File()) safe->proc.presets.exportTo (f);
    });
}

void HitrEditor::timerCallback()
{
    pulse += 0.05;
    if (currentModeIndex (proc) != shownMode) applyModeColours();
    for (int i = 0; i < tiles.size(); ++i)
    {
        int role = i == currentModeIndex (proc) ? 1 : (i == layerIndex (proc, 2) ? 2 : (i == layerIndex (proc, 3) ? 3 : 0));
        if (tiles[i]->role != role) { tiles[i]->role = role; tiles[i]->repaint(); }
    }
    layer2.refresh(); layer3.refresh();

    if (proc.presets.getCurrent() != shownPreset)
    {
        shownPreset = proc.presets.getCurrent();
        presetNameBtn.setButtonText (shownPreset + u (u8"  ▾"));
        browser.syncSelection();
    }
    const int slot = proc.getABSlot();
    for (int i = 0; i < slotBtns.size(); ++i)
    {
        slotBtns[i]->setToggleState (i == slot, dontSendNotification);
        slotBtns[i]->setAlpha (proc.slotHasState (i) ? 1.f : 0.5f);
    }

    // une étape d'undo par geste (rien tant que la souris est enfoncée)
    if (! ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        proc.undo.beginNewTransaction();
    undoBtn.setEnabled (proc.undo.canUndo());
    redoBtn.setEnabled (proc.undo.canRedo());

    const bool noLat = param (proc, "noLatency") > 0.5f;
    os.setEnabled (! noLat);
    os.setAlpha (noLat ? 0.35f : 1.f);

    const bool sync = param (proc, "sync") > 0.5f;
    const String info = "AG " + String (proc.autoGainDb.load(), 1) + " dB\nLAT " + String ((double) proc.latencyShown.load() * 1000.0 / jmax (1.0, proc.getSampleRate()), 1) + " ms";
    const String timeTxt = (sync ? String (proc.hostBpm.load(), 1) + " BPM  ·  " : String ("LIBRE  ·  ")) + String (roundToInt (proc.delayMsShown.load())) + " ms";
    const bool od = param (proc, "overdose") > 0.5f, sober = param (proc, "sobriety") > 0.5f;
    if (info != shownInfo || timeTxt != shownTimeText || od != shownOverdose || sober != shownSober || od)
    {
        shownInfo = info; shownTimeText = timeTxt; shownOverdose = od; shownSober = sober;
        repaint();
    }
}

void HitrEditor::applyModeColours()
{
    shownMode = currentModeIndex (proc);
    const auto& m = modes()[(size_t) shownMode];
    const Colour c1 (m.c1), c2 (m.c2), c3 (m.c3);
    const Colour c2v = c2.getPerceivedBrightness() < 0.25f ? c3 : c2;
    lnf.accent = c1;
    browser.accent = c1;
    dose.setColour (c1);
    for (auto* k : { &trip, &grain, &gdens, &ottTime, &fatColor, &toneLow, &clipXLo, &thermTone }) k->setColour (c2v);
    for (auto* k : { &mix, &gsize, &ottLow, &ottMid, &ottHigh, &toneAir, &clipXHi, &thermMix }) k->setColour (c3);
    for (auto* k : { &output, &width, &widthFreq, &delayMs, &ottK, &tone, &clipKnee }) k->setColour (c1.withRotatedHue (0.06f));
    heat.setColour (Colour (0xffff6a00));
    thermMove.setColour (Colour (0xffffb000));
    fatten.setColour (Colour (0xffff4f7a));
    clip.setColour (hui::danger);
    for (auto* b : { &prevBtn, &nextBtn, &presetNameBtn, &saveBtn, &copyBtn, &undoBtn, &redoBtn, &randomBtn, &bpmBtn, &msBtn })
        b->setColour (TextButton::buttonOnColourId, c1);
    for (auto* b : { &freeze.button, &transpose.button, &noLatency.button, &autoGain.button, &clipMS.button, &body.button, &sobriety.button })
        b->setColour (TextButton::buttonOnColourId, c1);
    for (auto* s : { &thermType, &thermRate }) s->setColour (Colour (0xffff6a00));
    for (auto* s : { &clipShape, &clipBand, &os }) s->setColour (c1);
    for (auto* b : slotBtns) b->setColour (TextButton::buttonOnColourId, c1);
    for (auto* t : tabs) t->setColour (TextButton::buttonOnColourId, c1);
    browser.refresh();
    rebuildBackground();
    repaint();
}

void HitrEditor::rebuildBackground()
{
    if (getWidth() <= 0) return;
    const auto& m = modes()[(size_t) jmax (0, shownMode)];
    const Colour c1 (m.c1), c2 (m.c2), c3 (m.c3);
    // rendu aux pixels physiques de l'écran (Retina / 4K) : jamais flou
    background = Image (Image::ARGB, jmax (1, roundToInt ((float) getWidth() * bgScale)), jmax (1, roundToInt ((float) getHeight() * bgScale)), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (bgScale));
    auto b = getLocalBounds().toFloat();
    const float sx = b.getWidth() / baseW;
    g.fillAll (hui::bg);
    // grosses nappes de couleur floues : le verre des panneaux les laisse deviner
    Random rng (shownMode * 7 + 3);
    for (int i = 0; i < 9; ++i)
    {
        const Colour c = i % 3 == 0 ? c1 : (i % 3 == 1 ? c2 : c3);
        glowBlob (g, c.withAlpha (0.16f), rng.nextFloat() * b.getWidth(), rng.nextFloat() * b.getHeight(), (220.f + rng.nextFloat() * 260.f) * sx);
    }
    const float horizon = b.getHeight() * 0.42f, vx = b.getCentreX();
    for (int i = -24; i <= 24; ++i)
    {
        g.setColour (c1.withAlpha (0.05f));
        g.drawLine (vx, horizon, vx + (float) i * b.getWidth() * 0.12f, b.getBottom(), 1.f);
    }
    for (int i = 1; i < 16; ++i)
    {
        const float z = (float) i / 16.f;
        g.setColour (c1.withAlpha (0.02f + 0.04f * z));
        g.drawHorizontalLine ((int) (horizon + (b.getBottom() - horizon) * z * z), 0.f, b.getWidth());
    }
    for (int i = 0; i < 200; ++i)
    {
        g.setColour (Colours::white.withAlpha (rng.nextFloat() * 0.25f));
        const float s = rng.nextFloat() * 1.6f + 0.4f;
        g.fillEllipse (rng.nextFloat() * b.getWidth(), rng.nextFloat() * b.getHeight(), s, s);
    }
}

void HitrEditor::drawPanel (Graphics& g, Rectangle<int> r, const String& title, Colour c) const
{
    drawGlassPanel (g, r, title, c, (float) getWidth() / baseW);
}

void HitrEditor::drawSection (Graphics& g, Rectangle<int> r, const String& title, Colour c) const
{
    if (r.isEmpty() || title.isEmpty()) return;
    const float s = (float) getWidth() / baseW;
    auto rf = r.toFloat();
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (rf, 7.f * s);
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (rf, 7.f * s, 1.f);
    g.setColour (c);
    g.fillRoundedRectangle (rf.getX() + 10.f * s, rf.getY() + 9.f * s, 3.f * s, 11.f * s, 1.5f);
    g.setColour (hui::text.withAlpha (0.85f));
    g.setFont (hui::font (10.5f * s, true, 0.22f));
    g.drawText (title, rf.withTrimmedLeft (19.f * s).withHeight (28.f * s), Justification::centredLeft);
}

void HitrEditor::paint (Graphics& g)
{
    const auto& m = modes()[(size_t) jmax (0, shownMode)];
    const Colour c1 (m.c1), c2 (m.c2), c3 (m.c3);
    const Colour c2v = c2.getPerceivedBrightness() < 0.25f ? c3 : c2;
    const float sx = (float) getWidth() / baseW;
    const bool od = param (proc, "overdose") > 0.5f;

    const float px = jmax (1.f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (std::abs (px - bgScale) > 0.01f || ! background.isValid())
    {
        bgScale = px;
        rebuildBackground();
    }
    if (background.isValid())
        g.drawImageTransformed (background, AffineTransform::scale (1.f / bgScale));
    else g.fillAll (hui::bg);

    // header
    auto h = header.toFloat();
    g.setGradientFill (ColourGradient (Colour (0xff0d121b).withAlpha (0.96f), h.getX(), h.getY(), Colour (0xff040508).withAlpha (0.96f), h.getX(), h.getBottom(), false));
    g.fillRect (h);
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.6f), h.getX(), h.getBottom(), Colours::transparentBlack, h.getX(), h.getBottom() + 12.f, false));
    g.fillRect (h.withY (h.getBottom()).withHeight (12.f));
    const Colour line = od ? hui::danger : c1;
    g.setGradientFill (ColourGradient (line.withAlpha (0.f), h.getX(), 0, line.withAlpha (0.8f), h.getCentreX(), 0, true));
    g.fillRect (h.removeFromBottom (1.f));

    // LOGO : le corbeau de ravenn, redimensionné à la taille exacte des pixels de l'écran
    // (calé sur un pixel physique entier, aucun rééchantillonnage au dessin : net, jamais pixellisé)
    float tx = 18.f * sx;
    if (ravenImg.isValid())
    {
        const float rh = (float) header.getHeight() * 0.92f;
        const float rw = rh * (float) ravenImg.getWidth() / (float) ravenImg.getHeight();
        const int tw = jmax (1, roundToInt (rw * px)), th = jmax (1, roundToInt (rh * px));
        const Rectangle<float> box (std::round (8.f * sx * px) / px, std::round (((float) header.getCentreY() - rh * 0.5f) * px) / px,
                                    (float) tw / px, (float) th / px);
        if (ravenCrisp.getWidth() != tw || ravenCrisp.getHeight() != th)
        {
            Image src = ravenImg.convertedToFormat (Image::ARGB);
            while (src.getWidth() > tw * 2 && src.getHeight() > th * 2)   // réduction par moitiés successives
                src = src.rescaled (src.getWidth() / 2, src.getHeight() / 2, Graphics::highResamplingQuality);
            ravenCrisp = src.rescaled (tw, th, Graphics::highResamplingQuality).convertedToFormat (Image::ARGB);
            Image::BitmapData bd (ravenCrisp, Image::BitmapData::readWrite);
            for (int y = 0; y < th; ++y)
                for (int x = 0; x < tw; ++x)
                {
                    const float a = bd.getPixelColour (x, y).getFloatAlpha();
                    bd.setPixelColour (x, y, Colours::white.withAlpha (jlimit (0.f, 1.f, (a - 0.12f) * 2.4f)));   // bords francs
                }
        }
        glowBlob (g, (od ? hui::danger : c1).withAlpha (0.28f), box.getCentreX(), box.getCentreY(), box.getHeight() * 0.7f);
        g.drawImageTransformed (ravenCrisp, AffineTransform::scale (1.f / px).translated (box.getX(), box.getY()));
        tx = box.getRight() + 6.f * sx;
    }
    const Rectangle<float> titleR (tx, (float) header.getCentreY() - 24.f * sx, 150.f * sx, 32.f * sx);
    g.setFont (hui::font (30.f * sx, true, 0.04f));
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawText ("h.i.t.r", titleR.translated (0.f, 2.f), Justification::bottomLeft);
    g.setGradientFill (ColourGradient (Colours::white, titleR.getX(), titleR.getY(), c1.interpolatedWith (Colours::white, 0.35f), titleR.getRight(), titleR.getBottom(), false));
    g.drawText ("h.i.t.r", titleR, Justification::bottomLeft);
    // version, bien visible
    {
        const String ver = "v" + String (JucePlugin_VersionString);
        const auto vf = hui::font (9.5f * sx, true, 0.12f);
        GlyphArrangement ga; ga.addLineOfText (vf, ver, 0.f, 0.f);
        const float vw = ga.getBoundingBox (0, -1, true).getWidth() + 12.f * sx;
        const Rectangle<float> pill (tx + 2.f, (float) header.getCentreY() + 11.f * sx, vw, 15.f * sx);
        g.setColour (c1.withAlpha (0.18f));
        g.fillRoundedRectangle (pill, 7.f * sx);
        g.setColour (c1.withAlpha (0.8f));
        g.drawRoundedRectangle (pill, 7.f * sx, 1.f);
        g.setColour (Colours::white);
        g.setFont (vf);
        g.drawText (ver, pill, Justification::centred);
        g.setColour (hui::dim);
        g.setFont (hui::font (7.5f * sx, true, 0.3f));
        g.drawText ("PSYCHOACTIVE MELODY", Rectangle<float> (pill.getRight() + 6.f * sx, pill.getY(), 140.f * sx, pill.getHeight()), Justification::centredLeft);
    }

    drawPanel (g, tripPanel, "TRIP VIEW", od ? hui::danger : c1);
    drawPanel (g, modesPanel, "SUBSTANCES", c1);
    drawPanel (g, ctrlPanel, "DOSAGE", od ? hui::danger : c2v);
    drawPanel (g, pagePanel, {}, c3);
    for (int i = 0; i < 4; ++i)
        drawSection (g, sections[(size_t) i], sectionNames[(size_t) i], i == 0 ? c1 : (i == 1 ? c2v : c3));

    // aide du mélange
    g.setColour (hui::dim);
    g.setFont (hui::font (10.f * sx, true, 0.1f));
    g.drawText (u (u8"CLIC : SUBSTANCE  ·  CLIC DROIT / MAJ+CLIC : MÉLANGER (jusqu'à 3)"),
                modesPanel.toFloat().withTrimmedTop (8.f * sx).withHeight (20.f * sx).withTrimmedRight (16.f * sx), Justification::centredRight);
    // substance principale dans la bande de mélange
    {
        auto pr = mixStrip.toFloat().withWidth (mixStrip.getWidth() * 0.3f).reduced (2.f);
        g.setGradientFill (ColourGradient (c1.withAlpha (0.45f), pr.getX(), pr.getY(), c2.withAlpha (0.25f), pr.getRight(), pr.getBottom(), false));
        g.fillRoundedRectangle (pr, 7.f);
        g.setColour (c1);
        g.drawRoundedRectangle (pr, 7.f, 1.2f);
        g.setColour (c1.getPerceivedBrightness() > 0.75f ? Colour (0xff05080d) : Colours::white);
        g.setFont (hui::font (12.f * sx, true, 0.12f));
        g.drawFittedText (u (m.name), pr.reduced (10.f, 2.f).toNearestInt(), Justification::centred, 1, 0.6f);
    }

    // infos
    g.setColour (hui::dim);
    g.setFont (hui::font (10.f * sx, true, 0.12f));
    g.drawFittedText (shownInfo, infoArea, Justification::centred, 2, 0.8f);
    if (page == 0 && ! sections[1].isEmpty())
        g.drawText (shownTimeText, sections[1].reduced (roundToInt (12 * sx), roundToInt (8 * sx)), Justification::bottomRight);
    if (page == 3)
    {
        g.setColour (hui::text.withAlpha (0.7f));
        g.setFont (hui::font (9.5f * sx, true, 0.2f));
        g.drawText ("SHAPE", clipShape.getBounds().translated (0, roundToInt (-14 * sx)).withHeight (roundToInt (14 * sx)), Justification::centredLeft);
        g.drawText ("BAND", clipBand.getBounds().translated (0, roundToInt (-14 * sx)).withHeight (roundToInt (14 * sx)), Justification::centredLeft);
        g.drawText ("OVERSAMPLING", os.getBounds().translated (0, roundToInt (-14 * sx)).withHeight (roundToInt (14 * sx)), Justification::centredLeft);
    }
    if (page == 1)
    {
        g.setColour (hui::text.withAlpha (0.7f));
        g.setFont (hui::font (9.5f * sx, true, 0.2f));
        g.drawText (u (u8"CARACTÈRE"), thermType.getBounds().translated (0, roundToInt (-14 * sx)).withHeight (roundToInt (14 * sx)), Justification::centredLeft);
        g.drawText ("VITESSE DU MOUVEMENT", thermRate.getBounds().translated (0, roundToInt (-14 * sx)).withHeight (roundToInt (14 * sx)), Justification::centredLeft);
    }
}

void HitrEditor::paintOverChildren (Graphics& g)
{
    const bool od = param (proc, "overdose") > 0.5f;
    const bool sober = param (proc, "sobriety") > 0.5f;
    auto b = getLocalBounds().toFloat();
    if (od)   // bord rouge qui bat
    {
        const float beat = 0.5f + 0.5f * std::sin ((float) pulse * 9.f);
        for (int i = 0; i < 14; ++i)
        {
            g.setColour (hui::danger.withAlpha ((0.16f - (float) i * 0.011f) * (0.5f + beat)));
            g.drawRect (b.reduced ((float) i), 1.f);
        }
    }
    if (sober)   // voile gris sur les réglages (le plugin est en pause)
    {
        g.setColour (Colour (0xff050608).withAlpha (0.35f));
        g.fillRect (b.withTrimmedTop ((float) header.getBottom()));
    }
}

void HitrEditor::resized()
{
    const float s = (float) getWidth() / baseW;
    auto S = [s] (float v) { return roundToInt (v * s); };
    auto R = [&] (float x, float y, float w, float h) { return Rectangle<int> (S (x), S (y), S (w), S (h)); };

    header = R (0, 0, baseW, 70);
    prevBtn.setBounds (R (262, 17, 34, 36));
    presetNameBtn.setBounds (R (298, 17, 250, 36));
    nextBtn.setBounds (R (550, 17, 34, 36));
    saveBtn.setBounds (R (588, 17, 62, 36));
    for (int i = 0; i < slotBtns.size(); ++i) slotBtns[i]->setBounds (R (662 + 32.f * (float) i, 17, 32, 36));
    copyBtn.setBounds (R (792, 17, 36, 36));
    undoBtn.setBounds (R (840, 17, 62, 36));
    redoBtn.setBounds (R (904, 17, 62, 36));
    randomBtn.setBounds (R (978, 17, 88, 36));
    noLatency.setBounds (R (1074, 17, 108, 36));
    sobriety.setBounds (R (1188, 17, 120, 36));

    tripPanel  = R (12, 82, 760, 370);
    modesPanel = R (12, 464, 760, 404);
    ctrlPanel  = R (784, 82, 524, 300);
    tabArea    = R (784, 392, 524, 38);
    pagePanel  = R (784, 436, 524, 432);

    tripView.setBounds (tripPanel.reduced (S (10)).withTrimmedTop (S (26)));

    mixStrip = R (24, 494, 736, 44);
    layer2.setBounds (R (24 + 736 * 0.3f + 4, 494, 736 * 0.35f - 6, 44));
    layer3.setBounds (R (24 + 736 * 0.65f + 2, 494, 736 * 0.35f - 2, 44));
    {
        auto grid = R (24, 546, 736, 312);
        const int cols = 10, rows = 5;
        const float cw = (float) grid.getWidth() / (float) cols, ch = (float) grid.getHeight() / (float) rows;
        for (int i = 0; i < tiles.size(); ++i)
            tiles[i]->setBounds (roundToInt ((float) grid.getX() + (float) (i % cols) * cw), roundToInt ((float) grid.getY() + (float) (i / cols) * ch),
                                 roundToInt (cw), roundToInt (ch));
    }

    dose.setBounds (R (794, 106, 200, 216));
    overdose.setBounds (R (806, 326, 176, 46));
    trip.setBounds (R (1000, 104, 100, 120));
    mix.setBounds (R (1102, 104, 100, 120));
    width.setBounds (R (1204, 104, 100, 120));
    widthFreq.setBounds (R (1000, 230, 100, 120));
    output.setBounds (R (1102, 230, 100, 120));
    autoGain.setBounds (R (1208, 244, 94, 32));
    infoArea = R (1204, 282, 104, 44);

    for (int i = 0; i < tabs.size(); ++i) tabs[i]->setBounds (R (784 + 131.f * (float) i, 392, 131, 38));

    for (auto& sct : sections) sct = {};
    switch (page)
    {
        case 0:
            sections[0] = R (796, 448, 500, 146);
            sections[1] = R (796, 600, 500, 98);
            sections[2] = R (796, 704, 500, 154);
            grain.setBounds (R (806, 470, 110, 118));
            gsize.setBounds (R (920, 470, 110, 118));
            gdens.setBounds (R (1034, 470, 110, 118));
            freeze.setBounds (R (1162, 510, 122, 36));
            bpmBtn.setBounds (R (806, 626, 60, 32));
            msBtn.setBounds (R (868, 626, 60, 32));
            transpose.setBounds (R (806, 660, 122, 32));
            divisionBox.setBounds (R (944, 636, 116, 36));
            delayMs.setBounds (R (944, 622, 90, 74));
            effects.setBounds (R (808, 730, 476, 120));
            break;
        case 1:
            sections[0] = R (796, 448, 500, 410);
            thermalView.setBounds (R (808, 478, 236, 186));
            thermType.setBounds (R (1054, 492, 232, 74));
            thermRate.setBounds (R (1054, 590, 232, 74));
            heat.setBounds (R (808, 690, 116, 140));
            thermMove.setBounds (R (928, 690, 116, 140));
            thermTone.setBounds (R (1048, 690, 116, 140));
            thermMix.setBounds (R (1168, 690, 116, 140));
            break;
        case 2:
            sections[0] = R (796, 448, 500, 160);
            sections[1] = R (796, 616, 246, 242);
            sections[2] = R (1050, 616, 246, 242);
            ottK.setBounds (R (806, 476, 96, 120));
            ottTime.setBounds (R (904, 476, 96, 120));
            ottLow.setBounds (R (1002, 476, 96, 120));
            ottMid.setBounds (R (1100, 476, 96, 120));
            ottHigh.setBounds (R (1198, 476, 96, 120));
            fatten.setBounds (R (806, 660, 114, 140));
            fatColor.setBounds (R (922, 660, 114, 140));
            tone.setBounds (R (1056, 668, 78, 110));
            toneLow.setBounds (R (1136, 668, 78, 110));
            toneAir.setBounds (R (1216, 668, 78, 110));
            break;
        case 3:
            sections[0] = R (796, 448, 500, 410);
            clip.setBounds (R (806, 478, 180, 210));
            clipMeter.setBounds (R (998, 484, 286, 40));
            clipShape.setBounds (R (998, 552, 286, 34));
            clipBand.setBounds (R (998, 612, 286, 34));
            clipMS.setBounds (R (998, 656, 92, 32));
            body.setBounds (R (1096, 656, 92, 32));
            clipKnee.setBounds (R (806, 708, 100, 128));
            clipXLo.setBounds (R (910, 708, 100, 128));
            clipXHi.setBounds (R (1014, 708, 100, 128));
            os.setBounds (R (1128, 748, 156, 34));
            break;
        default: break;
    }

    if (browser.isVisible())
        browser.setBounds (getLocalBounds().withTrimmedTop (S (70)));
    rebuildBackground();
}
