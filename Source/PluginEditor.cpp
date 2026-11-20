#include "PluginEditor.h"

using namespace juce;
using namespace hitr;

static int currentModeIndex (HitrProcessor& p)
{
    return jlimit (0, 24, (int) p.apvts.getRawParameterValue ("mode")->load());
}
static const Mode& currentMode (HitrProcessor& p) { return modes()[(size_t) currentModeIndex (p)]; }

static void glowBlob (Graphics& g, Colour col, float x, float y, float rad)
{
    g.setGradientFill (ColourGradient (col, x, y, col.withAlpha (0.f), x + rad, y, true));
    g.fillEllipse (x - rad, y - rad, rad * 2.f, rad * 2.f);
}

//==============================================================================
HitrLookAndFeel::HitrLookAndFeel()
{
    setColour (PopupMenu::backgroundColourId, Colour (0xff0a0d12));
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff3d7bff).withAlpha (0.35f));
    setColour (PopupMenu::textColourId, hui::text);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (ComboBox::textColourId, hui::text);
    setColour (TextButton::buttonColourId, hui::panel);
    setColour (TextButton::textColourOffId, hui::text.withAlpha (0.8f));
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff10141c));
    setColour (TooltipWindow::textColourId, hui::text);
    setColour (TooltipWindow::outlineColourId, hui::edge);
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

    // ombre portée douce sous le bouton (profondeur)
    glowBlob (g, Colours::black.withAlpha (0.75f), c.x, c.y + kr * 0.28f, kr * 1.35f);

    for (int i = 0; i <= 30; ++i)
    {
        const float a = startAngle + (float) i / 30.f * (endAngle - startAngle);
        const auto pt = c.getPointOnCircumference (radius - lw * 0.1f + lw * 0.55f, a);
        const float d = (i % 5 == 0) ? lw * 0.30f : lw * 0.16f;
        g.setColour (hui::dim.withAlpha (i % 5 == 0 ? 0.55f : 0.3f));
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
            g.setColour (fill.withAlpha (0.06f));
            g.strokePath (val, PathStrokeType (lw * (1.f + (float) i * 0.9f), PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setGradientFill (ColourGradient (fill, b.getX(), b.getBottom(), fill2, b.getRight(), b.getY(), false));
        g.strokePath (val, stroke);
    }

    // corps : dôme éclairé par le haut
    g.setGradientFill (ColourGradient (Colour (0xff2c3644), c.x - kr * 0.3f, c.y - kr * 0.8f,
                                       Colour (0xff040609), c.x + kr * 0.4f, c.y + kr, true));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f), c.x, c.y - kr,
                                       Colours::white.withAlpha (0.0f), c.x, c.y + kr * 0.2f, false));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f, 1.2f);
    // reflet spéculaire
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

    const auto txt = slider.getTextFromValue (slider.getValue());
    g.setColour (hui::text);
    g.setFont (hui::font (jmax (8.5f, kr * 0.30f), true));
    g.drawFittedText (txt, Rectangle<float> (kr * 1.5f, kr * 0.6f).withCentre (c).toNearestInt(),
                      Justification::centred, 1, 0.7f);
}

void HitrLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hover, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (3.f);
    const float rad = jmin (5.f, r.getHeight() * 0.22f);
    const auto acc = b.findColour (TextButton::buttonOnColourId);

    // ombre portée
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.f, down ? 1.f : 2.5f), rad);

    if (b.getToggleState())
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (acc.withAlpha (0.12f));
            g.fillRoundedRectangle (r.expanded ((float) i), rad + (float) i);
        }
        g.setGradientFill (ColourGradient (acc.brighter (0.35f), r.getX(), r.getY(),
                                           acc.darker (0.5f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (Colours::white.withAlpha (0.28f));
        g.drawLine (r.getX() + rad, r.getY() + 1.f, r.getRight() - rad, r.getY() + 1.f, 1.f);
    }
    else
    {
        g.setGradientFill (ColourGradient (Colour (0xff141a24).brighter (hover ? 0.15f : 0.f), r.getX(), r.getY(),
                                           Colour (0xff07090d), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawLine (r.getX() + rad, r.getY() + 1.f, r.getRight() - rad, r.getY() + 1.f, 1.f);
        g.setColour (acc.withAlpha (hover ? 0.8f : 0.35f));
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
    if (b.getToggleState() && b.findColour (TextButton::buttonOnColourId).getPerceivedBrightness() > 0.7f)
        col = Colour (0xff05080d);
    g.setColour (col);
    const float h = (float) b.getHeight();
    g.setFont (hui::font (jlimit (8.f, 15.f, h * 0.34f), true, 0.08f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 2), Justification::centred, 1, 0.6f);
}

void HitrLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (3.f);
    const float rad = jmin (5.f, r.getHeight() * 0.22f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.f, 2.5f), rad);
    g.setGradientFill (ColourGradient (Colour (0xff121822), r.getX(), r.getY(), Colour (0xff07090d), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (accent.withAlpha (box.isMouseOver (true) ? 0.85f : 0.5f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.f);

    Path arrow;
    const float ax = r.getRight() - r.getHeight() * 0.5f - 6.f, ay = r.getCentreY();
    const float as = jmax (3.f, r.getHeight() * 0.12f);
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
Font HitrLookAndFeel::getComboBoxFont (ComboBox& b) { return hui::font (jlimit (9.f, 18.f, (float) b.getHeight() * 0.42f), true, 0.06f); }
Font HitrLookAndFeel::getPopupMenuFont() { return hui::font (15.f); }

//==============================================================================
Knob::Knob (AudioProcessorValueTreeState& s, const String& id, const String& t) : title (t)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (220);
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
    g.setColour (hui::text.withAlpha (0.78f));
    g.setFont (hui::font (jlimit (8.f, 14.f, (float) lh * 0.72f), true, 0.16f));
    g.drawFittedText (title, r, Justification::centred, 1, 0.7f);
}

Pill::Pill (AudioProcessorValueTreeState& s, const String& id, const String& text)
{
    button.setButtonText (text);
    button.setClickingTogglesState (true);
    addAndMakeVisible (button);
    att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, id, button);
}

//==============================================================================
//  TRIP VIEW — visuels par mode
//==============================================================================
namespace
{
struct Ctx
{
    Graphics& g;
    Rectangle<float> r;
    float t, lv, dose, speed;
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

// plan lointain : champ d'étoiles qui dérive lentement
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

// plan proche : particules floues, grosses, qui passent devant
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
} // namespace

void TripView::mouseMove (const MouseEvent& e)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    mouseTarget = { (e.position.x - c.x) / jmax (1.f, (float) getWidth()), (e.position.y - c.y) / jmax (1.f, (float) getHeight()) };
}

void TripView::timerCallback()
{
    const float trip = proc.apvts.getRawParameterValue ("trip")->load();
    t += (1.0 / 40.0) * (double) trip;
    level += (jmin (1.f, proc.outputLevel.load() * 4.f) - level) * 0.25f;
    parallax += (mouseTarget - parallax) * 0.08f;
    repaint();
}

void TripView::paint (Graphics& g)
{
    const auto& m = currentMode (proc);
    const int idx = currentModeIndex (proc);
    const float dose = proc.apvts.getRawParameterValue ("dose")->load();
    auto r = getLocalBounds().toFloat();

    g.fillAll (Colour (0xff020305));
    const Colour cols[3] = { Colour (m.c1), Colour (m.c2), Colour (m.c3) };
    auto layer = [&] (float depth, auto&& fn)
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (getLocalBounds());
        const float ox = -parallax.x * depth * 40.f, oy = -parallax.y * depth * 26.f;
        const float zoom = 1.f + depth * 0.04f + level * 0.03f * depth;
        g.addTransform (AffineTransform::scale (zoom, zoom, r.getCentreX(), r.getCentreY()).translated (ox, oy));
        Ctx k { g, r, (float) t, level, dose, 1.0f, { cols[0], cols[1], cols[2] } };
        fn (k);
    };

    layer (0.2f, [] (Ctx& k) { drawFarLayer (k); });
    layer (0.6f, [&] (Ctx& k)
    {
        switch (m.visual)
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
        }
    });

    // vignette noire façon ravenn
    g.setGradientFill (ColourGradient (Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                       Colours::black.withAlpha (0.88f), r.getX(), r.getY(), true));
    g.fillRect (r);

    layer (1.4f, [] (Ctx& k) { drawNearLayer (k); });

    g.setColour (Colours::black.withAlpha (0.16f));
    for (float y = 0; y < r.getHeight(); y += 3.f)
        g.fillRect (0.f, y, r.getWidth(), 1.f);

    // nom du mode, en relief
    const auto name = String::fromUTF8 (m.name);
    auto nameArea = r.withSizeKeepingCentre (r.getWidth() - 40.f, r.getHeight() * 0.3f)
                     .translated (-parallax.x * 18.f, -r.getHeight() * 0.05f - parallax.y * 12.f);
    g.setFont (hui::font (jmin (r.getHeight() * 0.17f, r.getWidth() / (float) jmax (6, name.length()) * 1.25f), true, 0.18f));
    const float wob = std::sin ((float) t * 1.7f) * 2.f * dose;
    for (int i = 6; i >= 1; --i)
    {
        g.setColour (cols[0].withAlpha (0.07f));
        g.drawText (name, nameArea.translated ((float) i * 0.8f + wob, 0), Justification::centred);
        g.drawText (name, nameArea.translated (-(float) i * 0.8f - wob, 0), Justification::centred);
    }
    for (int i = 5; i >= 1; --i)   // extrusion 3D
    {
        g.setColour (cols[1].darker (0.6f).withAlpha (0.55f));
        g.drawText (name, nameArea.translated ((float) i * 0.7f + parallax.x * (float) i, (float) i * 0.9f + parallax.y * (float) i), Justification::centred);
    }
    g.setColour (Colours::white);
    g.drawText (name, nameArea, Justification::centred);

    g.setColour (hui::text.withAlpha (0.88f));
    g.setFont (hui::font (14.f, false, 0.02f).italicised());
    g.drawFittedText (String::fromUTF8 (m.desc), r.reduced (40.f, 14.f).removeFromBottom (48.f).toNearestInt(),
                      Justification::centred, 3, 0.9f);

    g.setFont (hui::font (11.f, true, 0.2f));
    g.setColour (hui::dim);
    g.drawText (String (idx + 1).paddedLeft ('0', 2) + " / 25", r.reduced (14.f, 10.f), Justification::topLeft);
    g.drawText ("DOSE " + String (roundToInt (dose * 100.f)) + " %", r.reduced (14.f, 10.f), Justification::topRight);

    // écran encastré : ombre intérieure + reflet de vitre
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
    const float dose = proc.apvts.getRawParameterValue ("dose")->load();
    const float grain = proc.apvts.getRawParameterValue ("grain")->load();
    const bool transpose = proc.apvts.getRawParameterValue ("transpose")->load() > 0.5f;
    const auto& v = m.values;
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
        { u8"GRANULAR",     clamp01 (v[G_MIX] * grain * 2.f / dose) },
        { u8"PULSATION",    clamp01 (v[TREM] + v[PAN] * 0.7f) },
        { u8"DISSOCIATION", clamp01 (v[DROPOUT] * 2.f + std::abs (v[WIDTH] - 1.f) * 0.4f + v[CHAOS] * 0.5f + v[STUTTER] * 0.3f) },
    };
    const int num = (int) (sizeof (rows) / sizeof (rows[0]));

    auto r = getLocalBounds().toFloat();
    const float rowH = r.getHeight() / (float) num;
    for (int i = 0; i < num; ++i)
    {
        auto row = r.removeFromTop (rowH).reduced (0.f, rowH * 0.2f);
        auto label = row.removeFromLeft (112.f);
        g.setColour (hui::text.withAlpha (0.75f));
        g.setFont (hui::font (10.5f, true, 0.14f));
        g.drawText (String::fromUTF8 (rows[i].label), label, Justification::centredLeft);

        auto bar = row.withTrimmedRight (6.f);
        // rainure creusée
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
            g.fillRoundedRectangle (fillR.expanded (0.f, 3.f), 4.f);
            g.setGradientFill (ColourGradient (Colour (m.c2), fillR.getX(), 0, Colour (m.c1), fillR.getRight(), 0, false));
            g.fillRoundedRectangle (fillR, 3.f);
            g.setColour (Colours::white.withAlpha (0.25f));
            g.fillRect (fillR.withHeight (1.f).reduced (2.f, 0.f));
        }
    }
}

//==============================================================================
//  ÉDITEUR
//==============================================================================
static constexpr float baseW = 1180.f, baseH = 790.f;

HitrEditor::HitrEditor (HitrProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      dose (p.apvts, "dose", "DOSE"), trip (p.apvts, "trip", "TRIP"),
      mix (p.apvts, "mix", "MIX"), output (p.apvts, "output", "OUTPUT"), width (p.apvts, "width", "WIDTH"),
      grain (p.apvts, "grain", "GRAIN"), gsize (p.apvts, "gsize", "SIZE"), gdens (p.apvts, "gdens", "DENSITY"),
      delayMs (p.apvts, "delayms", "TIME"),
      freeze (p.apvts, "freeze", "FREEZE"), transpose (p.apvts, "transpose", "GAMME LIBRE"),
      tripView (p), effects (p)
{
    setLookAndFeel (&lnf);

    for (auto* k : { &dose, &trip, &mix, &output, &width, &grain, &gsize, &gdens, &delayMs })
        addAndMakeVisible (*k);
    dose.slider.setTooltip (String::fromUTF8 (u8"Intensité : de sobre à la dose complète du mode."));
    trip.slider.setTooltip (String::fromUTF8 (u8"Vitesse des mouvements (en BPM : multiplie la division)."));
    width.slider.setTooltip (String::fromUTF8 (u8"Largeur stéréo. Les basses sous 140 Hz restent en mono."));
    grain.slider.setTooltip (String::fromUTF8 (u8"Quantité de granulizer. 50 % = le dosage du mode."));
    gsize.slider.setTooltip (String::fromUTF8 (u8"Taille des grains (multiplie celle du mode)."));
    gdens.slider.setTooltip (String::fromUTF8 (u8"Densité des grains (calée sur le tempo en BPM)."));
    delayMs.slider.setTooltip (String::fromUTF8 (u8"Temps de l'écho en millisecondes. AUTO = réglage du mode."));

    addAndMakeVisible (freeze);
    addAndMakeVisible (transpose);
    freeze.button.setTooltip (String::fromUTF8 (u8"Gèle le tampon du granulizer : les grains tournent sur l'instant figé."));
    transpose.button.setTooltip (String::fromUTF8 (u8"Autorise le mode à changer la hauteur (et donc la gamme). Coupé par défaut."));

    // BPM / MS
    for (auto* b : { &bpmBtn, &msBtn })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (*b);
    }
    bpmBtn.setTooltip (String::fromUTF8 (u8"Effets temporels calés sur le tempo du projet."));
    msBtn.setTooltip (String::fromUTF8 (u8"Effets temporels libres, écho réglé en millisecondes."));
    if (auto* prm = p.apvts.getParameter ("sync"))
        syncAtt = std::make_unique<ParameterAttachment> (*prm, [this] (float v)
        {
            bpmBtn.setToggleState (v > 0.5f, dontSendNotification);
            msBtn.setToggleState (v <= 0.5f, dontSendNotification);
            resized();
            repaint();
        });
    bpmBtn.onClick = [this] { syncAtt->setValueAsCompleteGesture (1.0f); };
    msBtn.onClick  = [this] { syncAtt->setValueAsCompleteGesture (0.0f); };
    syncAtt->sendInitialUpdate();

    divisionBox.addItemList (HitrProcessor::divisionNames(), 1);
    divisionBox.setTooltip (String::fromUTF8 (u8"Division de l'écho. AUTO = la plus proche du réglage du mode."));
    addAndMakeVisible (divisionBox);
    divisionAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "division", divisionBox);

    modeBox.addItemList (modeNames(), 1);
    addAndMakeVisible (modeBox);
    modeAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "mode", modeBox);

    prevBtn.onClick = [this] { setMode ((currentModeIndex (proc) + 24) % 25); };
    nextBtn.onClick = [this] { setMode ((currentModeIndex (proc) + 1) % 25); };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    for (int i = 0; i < 25; ++i)
    {
        const auto& m = modes()[(size_t) i];
        auto* b = modeBtns.add (new TextButton (String::fromUTF8 (m.shortName)));
        b->setColour (TextButton::buttonOnColourId, Colour (m.c1));
        b->setTooltip (String::fromUTF8 (m.name));
        b->onClick = [this, i] { setMode (i); };
        addAndMakeVisible (b);
    }

    addAndMakeVisible (tripView);
    addAndMakeVisible (effects);

    setResizable (true, true);
    setResizeLimits ((int) (baseW * 0.75f), (int) (baseH * 0.75f), (int) (baseW * 1.6f), (int) (baseH * 1.6f));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (baseW / baseH);
    setSize ((int) baseW, (int) baseH);
    applyModeColours();
    startTimerHz (20);
}

HitrEditor::~HitrEditor() { setLookAndFeel (nullptr); }

void HitrEditor::setMode (int idx)
{
    if (auto* prm = proc.apvts.getParameter ("mode"))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) idx));
        prm->endChangeGesture();
    }
}

void HitrEditor::timerCallback()
{
    if (currentModeIndex (proc) != shownMode)
        applyModeColours();

    const bool sync = proc.apvts.getRawParameterValue ("sync")->load() > 0.5f;
    const String txt = (sync ? String (proc.hostBpm.load(), 1) + " BPM  ·  " : String ("LIBRE  ·  "))
                       + String (roundToInt (proc.delayMsShown.load())) + " ms";
    if (txt != shownTimeText)
    {
        shownTimeText = txt;
        repaint (timeInfo);
    }
}

void HitrEditor::applyModeColours()
{
    shownMode = currentModeIndex (proc);
    const auto& m = modes()[(size_t) shownMode];
    const Colour c1 (m.c1), c2 (m.c2), c3 (m.c3);
    const Colour c2v = c2.getPerceivedBrightness() < 0.25f ? c3 : c2;
    lnf.accent = c1;
    dose.setColour (c1);
    trip.setColour (c2v);
    mix.setColour (c3);
    output.setColour (c1.withRotatedHue (0.08f));
    width.setColour (c2v.withRotatedHue (-0.06f));
    grain.setColour (c3);
    gsize.setColour (c1);
    gdens.setColour (c2v);
    delayMs.setColour (c1);
    for (auto* b : { &prevBtn, &nextBtn, &bpmBtn, &msBtn, &freeze.button, &transpose.button })
        b->setColour (TextButton::buttonOnColourId, c1);
    for (int i = 0; i < modeBtns.size(); ++i)
        modeBtns[i]->setToggleState (i == shownMode, dontSendNotification);
    rebuildBackground();
    repaint();
}

void HitrEditor::rebuildBackground()
{
    if (getWidth() <= 0) return;
    const auto& m = modes()[(size_t) jmax (0, shownMode)];
    const Colour c1 (m.c1), c2 (m.c2);
    background = Image (Image::ARGB, getWidth(), getHeight(), true);
    Graphics g (background);
    auto b = getLocalBounds().toFloat();
    const float sx = b.getWidth() / baseW;

    g.fillAll (hui::bg);
    g.setGradientFill (ColourGradient (c1.withAlpha (0.18f), b.getX(), b.getY(), Colours::transparentBlack, b.getX() + 560.f * sx, b.getY(), true));
    g.fillRect (b);
    g.setGradientFill (ColourGradient (c2.withAlpha (0.16f), b.getRight(), b.getBottom(), Colours::transparentBlack, b.getRight() - 650.f * sx, b.getBottom(), true));
    g.fillRect (b);

    // sol en perspective (profondeur)
    const float horizon = b.getHeight() * 0.42f, vx = b.getCentreX();
    for (int i = -24; i <= 24; ++i)
    {
        g.setColour (c1.withAlpha (0.045f));
        g.drawLine (vx, horizon, vx + (float) i * b.getWidth() * 0.12f, b.getBottom(), 1.f);
    }
    for (int i = 1; i < 16; ++i)
    {
        const float z = (float) i / 16.f;
        const float y = horizon + (b.getBottom() - horizon) * z * z;
        g.setColour (c1.withAlpha (0.02f + 0.04f * z));
        g.drawHorizontalLine ((int) y, 0.f, b.getWidth());
    }
    Random rng (99);
    for (int i = 0; i < 160; ++i)
    {
        g.setColour (Colours::white.withAlpha (rng.nextFloat() * 0.25f));
        const float s = rng.nextFloat() * 1.6f + 0.4f;
        g.fillEllipse (rng.nextFloat() * b.getWidth(), rng.nextFloat() * horizon, s, s);
    }
}

void HitrEditor::drawPanel (Graphics& g, Rectangle<int> area, const String& title, Colour c) const
{
    auto r = area.toFloat();
    // ombre portée
    DropShadow (Colours::black.withAlpha (0.8f), 22, { 0, 10 }).drawForRectangle (g, area);
    // corps biseauté
    g.setGradientFill (ColourGradient (Colour (0xff0e131b).withAlpha (0.97f), r.getX(), r.getY(),
                                       Colour (0xff040508).withAlpha (0.97f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 8.f);
    // lueur d'accent en haut du panneau
    g.setGradientFill (ColourGradient (c.withAlpha (0.10f), r.getX(), r.getY(), c.withAlpha (0.0f), r.getX(), r.getY() + 46.f, false));
    g.fillRoundedRectangle (r.withHeight (46.f), 8.f);
    g.setGradientFill (ColourGradient (c.withAlpha (0.6f), r.getX(), r.getY(), hui::edge, r.getRight(), r.getBottom(), false));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawLine (r.getX() + 10.f, r.getY() + 1.f, r.getRight() - 10.f, r.getY() + 1.f, 1.f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawLine (r.getX() + 10.f, r.getBottom() - 1.f, r.getRight() - 10.f, r.getBottom() - 1.f, 1.f);

    const float dotX = r.getX() + 16.f, dotY = r.getY() + 18.f;
    glowBlob (g, c.withAlpha (0.5f), dotX, dotY, 9.f);
    g.setColour (c);
    g.fillEllipse (dotX - 3.f, dotY - 3.f, 6.f, 6.f);
    g.setColour (hui::text);
    g.setFont (hui::font (11.5f, true, 0.2f));
    g.drawText (title, Rectangle<float> (dotX + 10.f, dotY - 9.f, r.getWidth() - 40.f, 18.f), Justification::centredLeft);
}

void HitrEditor::paint (Graphics& g)
{
    const auto& m = modes()[(size_t) jmax (0, shownMode)];
    const Colour c1 (m.c1), c2 (m.c2), c3 (m.c3);
    const Colour c2v = c2.getPerceivedBrightness() < 0.25f ? c3 : c2;
    const float sx = (float) getWidth() / baseW;

    if (background.isValid()) g.drawImageAt (background, 0, 0);
    else g.fillAll (hui::bg);

    // header
    auto h = header.toFloat();
    g.setGradientFill (ColourGradient (Colour (0xff0c1018), h.getX(), h.getY(), Colour (0xff040508), h.getX(), h.getBottom(), false));
    g.fillRect (h);
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.6f), h.getX(), h.getBottom(), Colours::transparentBlack, h.getX(), h.getBottom() + 10.f, false));
    g.fillRect (h.withY (h.getBottom()).withHeight (10.f));
    g.setGradientFill (ColourGradient (c1.withAlpha (0.f), h.getX(), 0, c1.withAlpha (0.75f), h.getCentreX(), 0, true));
    g.fillRect (h.removeFromBottom (1.f));

    // logo : spirale
    const float lx = 22.f * sx, ly = (float) header.getCentreY() - 16.f * sx, ls = 32.f * sx;
    {
        Path spiral;
        for (int s = 0; s < 60; ++s)
        {
            const float th = (float) s * 0.28f;
            const float rad = 1.f + (float) s * 0.26f * sx;
            const float x = lx + ls * 0.5f + rad * std::cos (th), y = ly + ls * 0.5f + rad * std::sin (th);
            if (s == 0) spiral.startNewSubPath (x, y); else spiral.lineTo (x, y);
        }
        glowBlob (g, c1.withAlpha (0.35f), lx + ls * 0.5f, ly + ls * 0.5f, ls * 0.9f);
        g.setGradientFill (ColourGradient (c1, lx, ly, c3, lx + ls, ly + ls, false));
        g.strokePath (spiral, PathStrokeType (2.2f * sx, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // nom en minuscules
    const float tx = lx + ls + 14.f * sx;
    const Rectangle<float> titleR (tx, ly - 6.f * sx, 200.f * sx, 32.f * sx);
    g.setFont (hui::font (30.f * sx, true, 0.04f));
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawText ("h.i.t.r", titleR.translated (0.f, 2.f), Justification::centredLeft);
    g.setColour (Colours::white);
    g.drawText ("h.i.t.r", titleR, Justification::centredLeft);
    g.setColour (hui::dim);
    g.setFont (hui::font (8.5f * sx, true, 0.35f));
    g.drawText ("PSYCHOACTIVE MELODY", Rectangle<float> (tx + 2.f, ly + 25.f * sx, 220.f * sx, 12.f * sx), Justification::centredLeft);

    drawPanel (g, tripPanel, "TRIP VIEW", c1);
    drawPanel (g, ctrlPanel, "DOSAGE", c2v);
    drawPanel (g, fxPanel, "EFFETS SUR LE SON", c3);
    drawPanel (g, grainPanel, "GRANULIZER", c3);
    drawPanel (g, timePanel, "TEMPS", c1);
    drawPanel (g, modesPanel, "SUBSTANCES", c1);

    g.setColour (hui::dim);
    g.setFont (hui::font (10.5f * sx, true, 0.12f));
    g.drawText (shownTimeText, timeInfo, Justification::centredRight);
}

void HitrEditor::resized()
{
    const float s = (float) getWidth() / baseW;
    auto S = [s] (float v) { return roundToInt (v * s); };
    auto R = [&] (float x, float y, float w, float h) { return Rectangle<int> (S (x), S (y), S (w), S (h)); };

    header     = R (0, 0, baseW, 64);
    tripPanel  = R (12, 76, 700, 360);
    ctrlPanel  = R (724, 76, 444, 360);
    fxPanel    = R (12, 448, 340, 330);
    grainPanel = R (364, 448, 348, 200);
    timePanel  = R (364, 660, 348, 118);
    modesPanel = R (724, 448, 444, 330);

    prevBtn.setBounds (R (450, 14, 36, 36));
    modeBox.setBounds (R (490, 14, 280, 36));
    nextBtn.setBounds (R (774, 14, 36, 36));

    tripView.setBounds (tripPanel.reduced (S (10)).withTrimmedTop (S (26)));

    // DOSAGE : gros DOSE à gauche, 2x2 à droite
    dose.setBounds (R (736, 112, 210, 230));
    trip.setBounds (R (950, 106, 104, 118));
    mix.setBounds (R (1056, 106, 104, 118));
    width.setBounds (R (950, 234, 104, 118));
    output.setBounds (R (1056, 234, 104, 118));

    effects.setBounds (fxPanel.reduced (S (16)).withTrimmedTop (S (24)));

    grain.setBounds (R (372, 478, 110, 124));
    gsize.setBounds (R (484, 478, 110, 124));
    gdens.setBounds (R (596, 478, 110, 124));
    freeze.setBounds (R (468, 606, 140, 32));

    const bool sync = proc.apvts.getRawParameterValue ("sync")->load() > 0.5f;
    bpmBtn.setBounds (R (376, 692, 62, 30));
    msBtn.setBounds (R (440, 692, 62, 30));
    transpose.setBounds (R (376, 732, 126, 32));
    divisionBox.setBounds (R (516, 704, 104, 34));
    divisionBox.setVisible (sync);
    delayMs.setBounds (R (624, 682, 82, 90));
    delayMs.setVisible (! sync);
    timeInfo = sync ? R (510, 744, 196, 24) : R (504, 700, 116, 40);

    auto grid = modesPanel.reduced (S (12)).withTrimmedTop (S (26));
    const int cw = grid.getWidth() / 5, ch = grid.getHeight() / 5;
    for (int i = 0; i < modeBtns.size(); ++i)
        modeBtns[i]->setBounds (grid.getX() + (i % 5) * cw, grid.getY() + (i / 5) * ch, cw, ch);

    rebuildBackground();
}
