#include "Decor.h"
#include "Theme.h"

namespace jgl::decor
{

using juce::Colour;
using juce::Path;
using juce::Point;
using juce::Rectangle;

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;

    /** Maps normalised (0..1) coordinates into `r`. */
    struct Box
    {
        Rectangle<float> r;
        float x (float u) const { return r.getX() + u * r.getWidth(); }
        float y (float v) const { return r.getY() + v * r.getHeight(); }
        Point<float> p (float u, float v) const { return { x (u), y (v) }; }
        float w (float u) const { return u * r.getWidth(); }
        float h (float v) const { return v * r.getHeight(); }
    };

    juce::ColourGradient wood (Rectangle<float> r, bool horizontal = true)
    {
        juce::ColourGradient g (Colour (0xff4a2410), horizontal ? r.getX() : r.getCentreX(), horizontal ? r.getCentreY() : r.getY(),
                                Colour (0xff4a2410), horizontal ? r.getRight() : r.getCentreX(), horizontal ? r.getCentreY() : r.getBottom(), false);
        g.addColour (0.35, Colour (0xffa86434));
        g.addColour (0.55, Colour (0xffc27a40));
        g.addColour (0.8, Colour (0xff6e3818));
        return g;
    }

    juce::ColourGradient brass (Rectangle<float> r)
    {
        juce::ColourGradient g (Colour (0xff7a5a1e), r.getX(), r.getY(), Colour (0xff6b4d18), r.getRight(), r.getBottom(), false);
        g.addColour (0.35, Colour (0xffe9c873));
        g.addColour (0.6, Colour (0xffb38a35));
        return g;
    }

    void skin (juce::Graphics& g, Rectangle<float> e)
    {
        g.setGradientFill (juce::ColourGradient (Colour (0xfff3e4c6), e.getX(), e.getY(), Colour (0xffd2b88e), e.getRight(), e.getBottom(), false));
        g.fillEllipse (e);
        g.setColour (Colour (0xff5c3a1e).withAlpha (0.6f));
        g.drawEllipse (e, 0.8f);
    }

    void shadowUnder (juce::Graphics& g, Rectangle<float> r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.fillEllipse (r.getX() + r.getWidth() * 0.1f, r.getBottom() - r.getHeight() * 0.08f, r.getWidth() * 0.8f, r.getHeight() * 0.12f);
    }

    /** Thick line between two points as a filled path (for necks, tubes). */
    Path tube (Point<float> a, Point<float> b, float wA, float wB)
    {
        const auto d = b - a;
        const float len = std::max (0.001f, d.getDistanceFromOrigin());
        const Point<float> n (-d.y / len, d.x / len);
        Path p;
        p.startNewSubPath (a + n * (wA * 0.5f));
        p.lineTo (b + n * (wB * 0.5f));
        p.lineTo (b - n * (wB * 0.5f));
        p.lineTo (a - n * (wA * 0.5f));
        p.closeSubPath();
        return p;
    }
}

//==============================================================================
void drawPaper (juce::Graphics& g, Rectangle<int> area)
{
    static juce::Image cached;
    if (cached.getWidth() != area.getWidth() || cached.getHeight() != area.getHeight())
    {
        cached = juce::Image (juce::Image::RGB, juce::jmax (1, area.getWidth()), juce::jmax (1, area.getHeight()), true);
        juce::Graphics ig (cached);
        const auto r = cached.getBounds().toFloat();
        ig.fillAll (Colours::paper);

        juce::Random rnd (1234);
        for (int i = 0; i < area.getWidth() * area.getHeight() / 90; ++i)
        {
            const float x = rnd.nextFloat() * r.getWidth(), y = rnd.nextFloat() * r.getHeight();
            const bool darkFibre = rnd.nextBool();
            ig.setColour ((darkFibre ? Colour (0xff8a6a48) : juce::Colours::white).withAlpha (0.02f + rnd.nextFloat() * 0.04f));
            const float a = rnd.nextFloat() * pi;
            const float len = 1.0f + rnd.nextFloat() * 5.0f;
            ig.drawLine (x, y, x + std::cos (a) * len, y + std::sin (a) * len, 0.7f);
        }

        juce::ColourGradient vignette (Colour (0x00000000), r.getCentreX(), r.getCentreY(),
                                       Colour (0xff6b4a2a).withAlpha (0.16f), 0.0f, 0.0f, true);
        ig.setGradientFill (vignette);
        ig.fillRect (r);
    }
    g.drawImageAt (cached, area.getX(), area.getY());
}

void drawFloralCorner (juce::Graphics& g, Rectangle<float> area, bool flipX, bool flipY, Colour colour)
{
    juce::Graphics::ScopedSaveState s (g);
    auto t = juce::AffineTransform::scale (flipX ? -1.0f : 1.0f, flipY ? -1.0f : 1.0f)
                 .translated (flipX ? area.getRight() : area.getX(), flipY ? area.getBottom() : area.getY());
    const float W = area.getWidth(), H = area.getHeight();

    Path stem;
    stem.startNewSubPath (0.0f, H * 0.95f);
    stem.cubicTo (W * 0.15f, H * 0.6f, W * 0.1f, H * 0.3f, W * 0.45f, H * 0.12f);
    stem.cubicTo (W * 0.6f, H * 0.05f, W * 0.8f, H * 0.12f, W * 0.95f, H * 0.02f);
    Path branch;
    branch.startNewSubPath (W * 0.12f, H * 0.55f);
    branch.cubicTo (W * 0.3f, H * 0.5f, W * 0.4f, H * 0.6f, W * 0.55f, H * 0.5f);

    g.setColour (colour);
    g.strokePath (stem, juce::PathStrokeType (1.1f), t);
    g.strokePath (branch, juce::PathStrokeType (0.9f), t);

    auto leaf = [&] (float x, float y, float len, float angle)
    {
        Path l;
        l.startNewSubPath (0.0f, 0.0f);
        l.quadraticTo (len * 0.5f, -len * 0.28f, len, 0.0f);
        l.quadraticTo (len * 0.5f, len * 0.28f, 0.0f, 0.0f);
        g.setColour (colour.withMultipliedAlpha (0.55f));
        g.fillPath (l, juce::AffineTransform::rotation (angle).translated (x, y).followedBy (t));
        g.setColour (colour);
        g.strokePath (l, juce::PathStrokeType (0.7f), juce::AffineTransform::rotation (angle).translated (x, y).followedBy (t));
    };
    leaf (W * 0.08f, H * 0.7f, W * 0.16f, -0.5f);
    leaf (W * 0.12f, H * 0.42f, W * 0.15f, -1.4f);
    leaf (W * 0.3f, H * 0.2f, W * 0.14f, -0.2f);
    leaf (W * 0.6f, H * 0.08f, W * 0.13f, 0.6f);
    leaf (W * 0.35f, H * 0.53f, W * 0.12f, 0.4f);
    leaf (W * 0.5f, H * 0.5f, W * 0.12f, -0.9f);

    auto flower = [&] (float x, float y, float r)
    {
        for (int i = 0; i < 5; ++i)
        {
            const float a = (float) i / 5.0f * 2.0f * pi;
            Path petal;
            petal.addEllipse (-r * 0.35f, -r, r * 0.7f, r);
            g.setColour (colour.withMultipliedAlpha (0.45f));
            g.fillPath (petal, juce::AffineTransform::rotation (a).translated (x, y).followedBy (t));
        }
        g.setColour (colour);
        Path c;
        c.addEllipse (x - r * 0.25f, y - r * 0.25f, r * 0.5f, r * 0.5f);
        g.fillPath (c, t);
    };
    flower (W * 0.45f, H * 0.12f, W * 0.05f);
    flower (W * 0.56f, H * 0.5f, W * 0.04f);
    flower (W * 0.92f, H * 0.03f, W * 0.035f);
}

Path lotus (Rectangle<float> a)
{
    Box b { a };
    Path p;
    // Centre petal
    p.startNewSubPath (b.p (0.5f, 0.92f));
    p.cubicTo (b.p (0.36f, 0.62f), b.p (0.4f, 0.3f), b.p (0.5f, 0.05f));
    p.cubicTo (b.p (0.6f, 0.3f), b.p (0.64f, 0.62f), b.p (0.5f, 0.92f));
    // Inner side petals
    for (int s : { -1, 1 })
    {
        const float m = (float) s;
        p.startNewSubPath (b.p (0.5f, 0.92f));
        p.cubicTo (b.p (0.5f + m * 0.05f, 0.6f), b.p (0.5f + m * 0.18f, 0.32f), b.p (0.5f + m * 0.3f, 0.2f));
        p.cubicTo (b.p (0.5f + m * 0.32f, 0.45f), b.p (0.5f + m * 0.22f, 0.75f), b.p (0.5f, 0.92f));
        // Outer petals
        p.startNewSubPath (b.p (0.5f, 0.92f));
        p.cubicTo (b.p (0.5f + m * 0.2f, 0.72f), b.p (0.5f + m * 0.36f, 0.55f), b.p (0.5f + m * 0.5f, 0.5f));
        p.cubicTo (b.p (0.5f + m * 0.42f, 0.75f), b.p (0.5f + m * 0.25f, 0.9f), b.p (0.5f, 0.92f));
    }
    // Base water line
    p.startNewSubPath (b.p (0.18f, 0.97f));
    p.quadraticTo (b.p (0.5f, 1.02f), b.p (0.82f, 0.97f));
    return p;
}

void drawFlowerOrnament (juce::Graphics& g, Point<float> c, float size, Colour colour)
{
    g.setColour (colour);
    for (int i = 0; i < 4; ++i)
    {
        Path petal;
        petal.startNewSubPath (0.0f, 0.0f);
        petal.cubicTo (-size * 0.22f, -size * 0.25f, -size * 0.12f, -size * 0.5f, 0.0f, -size * 0.55f);
        petal.cubicTo (size * 0.12f, -size * 0.5f, size * 0.22f, -size * 0.25f, 0.0f, 0.0f);
        const auto t = juce::AffineTransform::rotation ((float) i * pi * 0.5f).translated (c);
        g.strokePath (petal, juce::PathStrokeType (1.1f), t);
        Path inner;
        inner.addEllipse (-size * 0.05f, -size * 0.38f, size * 0.1f, size * 0.22f);
        g.fillPath (inner, t);
    }
    g.fillEllipse (c.x - size * 0.07f, c.y - size * 0.07f, size * 0.14f, size * 0.14f);
}

void drawDiamond (juce::Graphics& g, Point<float> c, float size, Colour colour, bool filled)
{
    Path d;
    d.addQuadrilateral (c.x, c.y - size, c.x + size, c.y, c.x, c.y + size, c.x - size, c.y);
    g.setColour (colour);
    if (filled) g.fillPath (d);
    else        g.strokePath (d, juce::PathStrokeType (1.1f));
}

Path archPanel (Rectangle<float> a, float archH)
{
    const float x0 = a.getX(), x1 = a.getRight(), y0 = a.getY(), y1 = a.getBottom();
    const float cx = a.getCentreX(), w = a.getWidth();
    const float base = y0 + archH;
    const float r = 14.0f;

    Path p;
    p.startNewSubPath (x0, y1 - r);
    p.lineTo (x0, base + r);
    p.quadraticTo (x0, base, x0 + r, base);
    p.lineTo (cx - w * 0.30f, base);
    // Left scallops rising to the peak
    p.cubicTo (cx - w * 0.25f, base, cx - w * 0.24f, base - archH * 0.45f, cx - w * 0.19f, base - archH * 0.5f);
    p.cubicTo (cx - w * 0.15f, base - archH * 0.55f, cx - w * 0.13f, base - archH * 0.8f, cx - w * 0.08f, base - archH * 0.85f);
    p.cubicTo (cx - w * 0.04f, base - archH * 0.9f, cx - w * 0.02f, y0, cx, y0);
    p.cubicTo (cx + w * 0.02f, y0, cx + w * 0.04f, base - archH * 0.9f, cx + w * 0.08f, base - archH * 0.85f);
    p.cubicTo (cx + w * 0.13f, base - archH * 0.8f, cx + w * 0.15f, base - archH * 0.55f, cx + w * 0.19f, base - archH * 0.5f);
    p.cubicTo (cx + w * 0.24f, base - archH * 0.45f, cx + w * 0.25f, base, cx + w * 0.30f, base);
    p.lineTo (x1 - r, base);
    p.quadraticTo (x1, base, x1, base + r);
    p.lineTo (x1, y1 - r);
    p.quadraticTo (x1, y1, x1 - r, y1);
    p.lineTo (x0 + r, y1);
    p.quadraticTo (x0, y1, x0, y1 - r);
    p.closeSubPath();
    return p;
}

//==============================================================================
void drawTempleSunset (juce::Graphics& g, Rectangle<float> a)
{
    juce::Graphics::ScopedSaveState s (g);
    Box b { a };
    const float horizon = b.y (0.72f);

    // Sky
    juce::ColourGradient sky (Colours::panel.withAlpha (0.0f), a.getX(), a.getY(), Colour (0xffeea56a), a.getX(), horizon, false);
    sky.addColour (0.45, Colour (0xfff6dcbc).withAlpha (0.8f));
    sky.addColour (0.8, Colour (0xfff2bd8a));
    g.setGradientFill (sky);
    g.fillRect (a.withBottom (horizon));

    // Sun and glow
    const auto sun = b.p (0.58f, 0.70f);
    for (int i = 6; i >= 1; --i)
    {
        const float r = b.w (0.05f) * (1.0f + (float) i * 0.6f);
        g.setColour (Colour (0xfff8b36a).withAlpha (0.07f));
        g.fillEllipse (sun.x - r, sun.y - r, r * 2.0f, r * 2.0f);
    }
    g.setColour (Colour (0xfff9c07a));
    g.fillEllipse (sun.x - b.w (0.06f), sun.y - b.w (0.06f), b.w (0.12f), b.w (0.12f));

    // Distant haze hills
    Path hills;
    hills.startNewSubPath (a.getX(), horizon);
    hills.cubicTo (b.x (0.15f), b.y (0.66f), b.x (0.3f), b.y (0.69f), b.x (0.45f), b.y (0.685f));
    hills.cubicTo (b.x (0.6f), b.y (0.68f), b.x (0.75f), b.y (0.67f), a.getRight(), b.y (0.68f));
    hills.lineTo (a.getRight(), horizon);
    hills.closeSubPath();
    g.setColour (Colour (0xffb98a6c).withAlpha (0.45f));
    g.fillPath (hills);

    // Water
    juce::ColourGradient water (Colour (0xffe1a57a), a.getX(), horizon, Colour (0xff7d5d4a), a.getX(), a.getBottom(), false);
    g.setGradientFill (water);
    g.fillRect (a.withTop (horizon));
    juce::Random rnd (7);
    for (int i = 0; i < 40; ++i)
    {
        const float yy = horizon + 3.0f + rnd.nextFloat() * (a.getBottom() - horizon - 4.0f);
        const float spread = (yy - horizon) * 0.6f + b.w (0.03f);
        const float xx = sun.x + (rnd.nextFloat() - 0.5f) * spread * 2.0f;
        g.setColour (Colour (0xfffad1a0).withAlpha (0.25f + rnd.nextFloat() * 0.3f));
        g.drawLine (xx, yy, xx + 4.0f + rnd.nextFloat() * 14.0f, yy, 1.0f);
    }

    const Colour silhouette (0xff4b3326);

    // Temple gopuram (stepped tower) on the right, with a soft reflection
    {
        Path t;
        const float left = b.x (0.68f), right = b.x (0.95f), H = b.h (0.36f);
        float l = left, r = right, y = horizon;
        // Base platform
        t.addRectangle (l - b.w (0.03f), y - H * 0.08f, (r - l) + b.w (0.06f), H * 0.08f);
        y -= H * 0.08f;
        const int tiers = 6;
        const float tierH = H * 0.11f;
        for (int i = 0; i < tiers; ++i)
        {
            t.addRectangle (l, y - tierH, r - l, tierH);
            t.addRectangle (l - 2.0f, y - tierH * 0.18f, (r - l) + 4.0f, tierH * 0.18f);   // cornice ledge
            const float inset = (r - l) * 0.075f;
            l += inset; r -= inset; y -= tierH;
        }
        // Barrel-vault crown and finials
        t.addRoundedRectangle (l - 3.0f, y - H * 0.12f, (r - l) + 6.0f, H * 0.12f, H * 0.05f);
        y -= H * 0.12f;
        for (int k = 0; k < 5; ++k)
        {
            const float fx = l + (r - l) * (0.1f + 0.2f * (float) k);
            t.addEllipse (fx - 1.6f, y - 6.0f, 3.2f, 5.0f);
        }
        // Entrance
        Path door;
        door.addRoundedRectangle ((left + right) * 0.5f - b.w (0.025f), horizon - H * 0.2f, b.w (0.05f), H * 0.12f, 2.0f);

        g.setColour (silhouette.withAlpha (0.9f));
        g.fillPath (t);
        g.setColour (Colour (0xffe7a76a).withAlpha (0.5f));
        g.fillPath (door);

        g.setColour (silhouette.withAlpha (0.22f));
        g.fillPath (t, juce::AffineTransform::scale (1.0f, -0.55f, 0.0f, horizon));

        // Tier highlights from the low sun
        g.setColour (Colour (0xffe0a26c).withAlpha (0.35f));
        float yy = horizon - H * 0.08f, ll = left, rr = right;
        for (int i = 0; i < tiers; ++i)
        {
            g.drawLine (ll + 2.0f, yy - 2.0f, rr - 2.0f, yy - 2.0f, 0.8f);
            const float inset = (rr - ll) * 0.075f;
            ll += inset; rr -= inset; yy -= tierH;
        }
    }

    // Palms on the left bank
    auto palm = [&] (float baseX, float height, float lean)
    {
        const float bx = b.x (baseX), by = horizon + 2.0f;
        const float tx = bx + b.w (lean), ty = by - b.h (height);
        Path trunk;
        trunk.startNewSubPath (bx, by);
        trunk.quadraticTo (bx + b.w (lean) * 0.2f, (by + ty) * 0.5f, tx, ty);
        g.setColour (silhouette);
        g.strokePath (trunk, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        for (int k = 0; k < 7; ++k)
        {
            const float ang = -pi * 0.95f + (float) k * pi * 0.3f;
            Path frond;
            frond.startNewSubPath (tx, ty);
            const float len = b.w (0.16f);
            frond.quadraticTo (tx + std::cos (ang) * len * 0.6f, ty + std::sin (ang) * len * 0.6f - 6.0f,
                               tx + std::cos (ang) * len, ty + std::sin (ang) * len + 8.0f);
            g.strokePath (frond, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    };
    palm (0.06f, 0.30f, 0.05f);
    palm (0.17f, 0.22f, -0.03f);
    palm (0.27f, 0.16f, 0.04f);

    // Shoreline
    Path shore;
    shore.startNewSubPath (a.getX(), horizon + b.h (0.03f));
    shore.quadraticTo (b.x (0.25f), horizon + b.h (0.01f), b.x (0.45f), horizon + 1.0f);
    shore.lineTo (a.getX(), horizon);
    shore.closeSubPath();
    g.setColour (silhouette.withAlpha (0.85f));
    g.fillPath (shore);

    // Watercolour blooms at the top that melt into the panel
    juce::Random bloom (11);
    for (int i = 0; i < 14; ++i)
    {
        const float r = b.w (0.06f + bloom.nextFloat() * 0.12f);
        const float x = b.x (bloom.nextFloat() * 0.5f), y = b.y (0.02f + bloom.nextFloat() * 0.3f);
        g.setColour (Colour (0xffd78a4e).withAlpha (0.05f + bloom.nextFloat() * 0.06f));
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 1.6f);
    }
}

//==============================================================================
void drawInstrument (juce::Graphics& g, InstrumentId id, Rectangle<float> a)
{
    Box b { a };
    const Colour outline (0xff3b2414);

    switch (id)
    {
        case InstrumentId::Tabla:
        {
            // Bayan (metal, left)
            auto bayan = Rectangle<float> (b.x (0.02f), b.y (0.36f), b.w (0.52f), b.h (0.6f));
            shadowUnder (g, bayan);
            Path bowl;
            bowl.addRoundedRectangle (bayan.getX(), bayan.getY() + bayan.getHeight() * 0.12f, bayan.getWidth(), bayan.getHeight() * 0.84f, bayan.getWidth() * 0.32f);
            g.setGradientFill (brass (bayan));
            g.fillPath (bowl);
            g.setColour (outline.withAlpha (0.5f)); g.strokePath (bowl, juce::PathStrokeType (0.8f));
            auto bTop = bayan.withHeight (bayan.getHeight() * 0.26f);
            skin (g, bTop);
            g.setColour (Colour (0xff1c1612)); g.fillEllipse (bTop.reduced (bTop.getWidth() * 0.3f, bTop.getHeight() * 0.28f).translated (-bTop.getWidth() * 0.08f, 0.0f));
            // Dayan (wood, right)
            auto dayan = Rectangle<float> (b.x (0.48f), b.y (0.12f), b.w (0.4f), b.h (0.84f));
            shadowUnder (g, dayan);
            Path body;
            body.startNewSubPath (dayan.getX() + dayan.getWidth() * 0.06f, dayan.getY() + 4.0f);
            body.lineTo (dayan.getRight() - dayan.getWidth() * 0.06f, dayan.getY() + 4.0f);
            body.quadraticTo (dayan.getRight() + 2.0f, dayan.getCentreY(), dayan.getRight() - dayan.getWidth() * 0.02f, dayan.getBottom() - 3.0f);
            body.lineTo (dayan.getX() + dayan.getWidth() * 0.02f, dayan.getBottom() - 3.0f);
            body.quadraticTo (dayan.getX() - 2.0f, dayan.getCentreY(), dayan.getX() + dayan.getWidth() * 0.06f, dayan.getY() + 4.0f);
            g.setGradientFill (wood (dayan));
            g.fillPath (body);
            g.setColour (Colour (0xffe8d5b0).withAlpha (0.55f));
            for (int i = 0; i < 6; ++i)
            {
                const float x = dayan.getX() + dayan.getWidth() * (0.12f + 0.15f * (float) i);
                g.drawLine (x, dayan.getY() + 6.0f, x + 1.0f, dayan.getBottom() - 4.0f, 0.7f);
            }
            auto dTop = dayan.withHeight (dayan.getHeight() * 0.18f);
            skin (g, dTop);
            g.setColour (Colour (0xff1c1612)); g.fillEllipse (dTop.reduced (dTop.getWidth() * 0.3f, dTop.getHeight() * 0.3f));
            break;
        }

        case InstrumentId::Dhol:
        case InstrumentId::Dholak:
        {
            const bool dhol = id == InstrumentId::Dhol;
            auto body = Rectangle<float> (b.x (0.08f), b.y (dhol ? 0.22f : 0.3f), b.w (0.84f), b.h (dhol ? 0.62f : 0.5f));
            shadowUnder (g, body.withHeight (body.getHeight() * 1.1f));
            Path barrel;
            barrel.startNewSubPath (body.getX() + 4.0f, body.getY() + 3.0f);
            barrel.quadraticTo (body.getCentreX(), body.getY() - body.getHeight() * 0.12f, body.getRight() - 4.0f, body.getY() + 3.0f);
            barrel.lineTo (body.getRight() - 4.0f, body.getBottom() - 3.0f);
            barrel.quadraticTo (body.getCentreX(), body.getBottom() + body.getHeight() * 0.12f, body.getX() + 4.0f, body.getBottom() - 3.0f);
            barrel.closeSubPath();
            g.setGradientFill (wood (body, false));
            g.fillPath (barrel);
            // Rope lacing
            g.setColour (Colour (0xffeedcb8).withAlpha (0.8f));
            const int zig = dhol ? 7 : 6;
            for (int i = 0; i < zig; ++i)
            {
                const float x0 = body.getX() + 6.0f + (float) i / (float) zig * (body.getWidth() - 12.0f);
                const float x1 = x0 + (body.getWidth() - 12.0f) / (float) zig;
                g.drawLine (x0, body.getY() + 3.0f, x1, body.getBottom() - 3.0f, 0.9f);
                g.drawLine (x1, body.getY() + 3.0f, x0, body.getBottom() - 3.0f, 0.9f);
            }
            if (! dhol)
            {
                g.setColour (Colour (0xffcfd2d4));
                for (int i = 0; i < zig; ++i)
                {
                    const float x = body.getX() + 6.0f + ((float) i + 0.5f) / (float) zig * (body.getWidth() - 12.0f);
                    g.drawEllipse (x - 2.0f, body.getCentreY() - 2.0f, 4.0f, 4.0f, 0.8f);
                }
            }
            // Heads
            for (float hx : { body.getX(), body.getRight() })
                skin (g, Rectangle<float> (body.getHeight() * 0.32f, body.getHeight()).withCentre ({ hx, body.getCentreY() }));
            break;
        }

        case InstrumentId::Ghungroo:
        {
            Path cord;
            cord.startNewSubPath (b.p (0.05f, 0.25f));
            cord.quadraticTo (b.p (0.5f, 0.05f), b.p (0.95f, 0.3f));
            g.setColour (Colour (0xff8b2a1c));
            g.strokePath (cord, juce::PathStrokeType (1.5f));
            const float pts[][2] = { { .18f, .42f }, { .38f, .34f }, { .58f, .36f }, { .78f, .45f }, { .28f, .62f },
                                     { .48f, .58f }, { .68f, .64f }, { .38f, .8f }, { .58f, .82f } };
            const float r = b.h (0.16f);
            for (auto& p : pts)
            {
                auto e = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (b.p (p[0], p[1]));
                g.setColour (juce::Colours::black.withAlpha (0.15f));
                g.fillEllipse (e.translated (1.0f, 1.5f));
                g.setGradientFill (juce::ColourGradient (Colour (0xfff2d78c), e.getX() + r * 0.6f, e.getY() + r * 0.5f,
                                                         Colour (0xff7d5a1a), e.getRight(), e.getBottom(), true));
                g.fillEllipse (e);
                g.setColour (Colour (0xff3a2a10));
                g.drawLine (e.getX() + r * 0.5f, e.getCentreY() + r * 0.3f, e.getRight() - r * 0.5f, e.getCentreY() + r * 0.3f, 1.0f);
            }
            break;
        }

        case InstrumentId::Manjira:
        {
            g.setColour (Colour (0xff8b2a1c));
            Path str;
            str.startNewSubPath (b.p (0.3f, 0.3f));
            str.quadraticTo (b.p (0.5f, 0.05f), b.p (0.72f, 0.28f));
            g.strokePath (str, juce::PathStrokeType (1.2f));
            for (int i = 0; i < 2; ++i)
            {
                auto e = Rectangle<float> (b.w (0.55f), b.h (0.42f)).withCentre (b.p (i == 0 ? 0.32f : 0.68f, i == 0 ? 0.55f : 0.5f));
                g.setColour (juce::Colours::black.withAlpha (0.12f));
                g.fillEllipse (e.translated (1.0f, 2.0f));
                g.setGradientFill (brass (e));
                g.fillEllipse (e);
                auto dome = e.reduced (e.getWidth() * 0.33f, e.getHeight() * 0.3f).translated (0.0f, -e.getHeight() * 0.08f);
                g.setGradientFill (juce::ColourGradient (Colour (0xfff8e3a0), dome.getX(), dome.getY(), Colour (0xff8c6a24), dome.getRight(), dome.getBottom(), false));
                g.fillEllipse (dome);
                g.setColour (outline.withAlpha (0.4f));
                g.drawEllipse (e, 0.7f);
            }
            break;
        }

        case InstrumentId::Taali:
        {
            const Colour skinTone (0xffd59a6e), skinDark (0xffa86b45);
            for (int side = 0; side < 2; ++side)
            {
                const float m = side == 0 ? -1.0f : 1.0f;
                Path hand;
                hand.addRoundedRectangle (-b.w (0.13f), -b.h (0.42f), b.w (0.26f), b.h (0.75f), b.w (0.11f));
                hand.addRoundedRectangle (-b.w (0.19f) * (side == 0 ? 1.0f : -0.2f), -b.h (0.08f), b.w (0.09f), b.h (0.3f), b.w (0.04f));
                const auto t = juce::AffineTransform::rotation (m * 0.32f).translated (b.p (0.5f + m * 0.1f, 0.55f));
                g.setGradientFill (juce::ColourGradient (skinTone, b.x (0.3f), b.y (0.2f), skinDark, b.x (0.7f), b.y (0.9f), false));
                g.fillPath (hand, t);
                g.setColour (skinDark.darker (0.3f));
                g.strokePath (hand, juce::PathStrokeType (0.7f), t);
                for (int f = 1; f < 4; ++f)
                {
                    Path line;
                    const float fx = -b.w (0.13f) + b.w (0.065f) * (float) f;
                    line.startNewSubPath (fx, -b.h (0.4f));
                    line.lineTo (fx, -b.h (0.12f));
                    g.strokePath (line, juce::PathStrokeType (0.6f), t);
                }
            }
            // Motion arcs
            g.setColour (Colours::copper.withAlpha (0.6f));
            g.drawLine (b.x (0.5f), b.y (0.02f), b.x (0.5f), b.y (0.12f), 1.0f);
            g.drawLine (b.x (0.38f), b.y (0.06f), b.x (0.42f), b.y (0.14f), 1.0f);
            g.drawLine (b.x (0.62f), b.y (0.06f), b.x (0.58f), b.y (0.14f), 1.0f);
            break;
        }

        case InstrumentId::Tumbi:
        case InstrumentId::Sitar:
        case InstrumentId::Tanpura:
        {
            const bool sitar = id == InstrumentId::Sitar, tanpura = id == InstrumentId::Tanpura;
            const float gourdR = b.h (id == InstrumentId::Tumbi ? 0.24f : (tanpura ? 0.34f : 0.3f));
            const auto gc = b.p (id == InstrumentId::Tumbi ? 0.2f : 0.2f, id == InstrumentId::Tumbi ? 0.72f : 0.68f);
            const auto top = b.p (0.97f, 0.06f);
            // Neck
            g.setGradientFill (wood (a));
            g.fillPath (tube (gc, top, b.h (sitar || tanpura ? 0.13f : 0.07f), b.h (sitar || tanpura ? 0.1f : 0.06f)));
            if (sitar)
            {
                g.setColour (Colour (0xffe8d8b8));
                for (int i = 1; i < 12; ++i)
                {
                    const auto p = gc + (top - gc) * (0.25f + 0.055f * (float) i);
                    g.drawLine (p.x - 3.0f, p.y - 3.0f, p.x + 3.0f, p.y + 3.0f, 0.8f);
                }
                auto upper = Rectangle<float> (b.h (0.16f), b.h (0.16f)).withCentre (gc + (top - gc) * 0.78f + Point<float> (2.0f, 4.0f));
                g.setGradientFill (wood (upper));
                g.fillEllipse (upper);
            }
            // Strings
            g.setColour (Colour (0xfff3e7cf).withAlpha (0.9f));
            g.drawLine (gc.x, gc.y, top.x, top.y, 0.6f);
            // Gourd
            auto gourd = Rectangle<float> (gourdR * 2.0f, gourdR * 2.0f).withCentre (gc);
            shadowUnder (g, gourd);
            g.setGradientFill (juce::ColourGradient (Colour (0xffd08848), gourd.getX() + gourdR * 0.6f, gourd.getY() + gourdR * 0.5f,
                                                     Colour (0xff5a2c12), gourd.getRight(), gourd.getBottom(), true));
            g.fillEllipse (gourd);
            g.setColour (outline.withAlpha (0.5f));
            g.drawEllipse (gourd, 0.8f);
            if (id == InstrumentId::Tumbi)
                skin (g, gourd.reduced (gourdR * 0.35f));
            else
            {
                g.setColour (Colour (0xffeedfc0).withAlpha (0.8f));
                g.drawEllipse (gourd.reduced (gourdR * 0.3f), 0.8f);
            }
            // Pegs
            g.setColour (Colour (0xff3b2414));
            for (int i = 0; i < (id == InstrumentId::Tumbi ? 1 : 3); ++i)
            {
                const auto p = top + (gc - top) * (0.04f + 0.06f * (float) i);
                g.drawLine (p.x - 4.0f, p.y - 1.0f, p.x + 4.0f, p.y + 1.0f, 2.0f);
            }
            break;
        }

        case InstrumentId::Bansuri:
        {
            const auto p0 = b.p (0.04f, 0.86f), p1 = b.p (0.96f, 0.14f);
            g.setColour (juce::Colours::black.withAlpha (0.1f));
            g.fillPath (tube (p0 + Point<float> (1.0f, 3.0f), p1 + Point<float> (1.0f, 3.0f), b.h (0.13f), b.h (0.13f)));
            g.setGradientFill (juce::ColourGradient (Colour (0xffe2c28a), p0.x, p0.y - 4.0f, Colour (0xffa47a40), p0.x, p0.y + 4.0f, false));
            g.fillPath (tube (p0, p1, b.h (0.13f), b.h (0.13f)));
            g.setColour (Colour (0xff6b2a1a));
            for (float t : { 0.06f, 0.1f, 0.92f })
            {
                const auto p = p0 + (p1 - p0) * t;
                g.fillPath (tube (p - (p1 - p0) * 0.012f, p + (p1 - p0) * 0.012f, b.h (0.15f), b.h (0.15f)));
            }
            g.setColour (Colour (0xff2a1a10));
            for (int i = 0; i < 6; ++i)
            {
                const auto p = p0 + (p1 - p0) * (0.3f + 0.08f * (float) i);
                g.fillEllipse (p.x - 1.6f, p.y - 1.6f, 3.2f, 3.2f);
            }
            const auto emb = p0 + (p1 - p0) * 0.82f;
            g.fillEllipse (emb.x - 2.0f, emb.y - 2.0f, 4.0f, 4.0f);
            break;
        }

        case InstrumentId::Nadaswaram:
        {
            const auto p0 = b.p (0.06f, 0.12f), p1 = b.p (0.8f, 0.78f);
            g.setColour (juce::Colours::black.withAlpha (0.1f));
            g.fillPath (tube (p0 + Point<float> (1.0f, 3.0f), p1 + Point<float> (1.0f, 3.0f), b.h (0.07f), b.h (0.2f)));
            g.setGradientFill (wood (a, false));
            g.fillPath (tube (p0, p1, b.h (0.07f), b.h (0.2f)));
            // Flared bell
            auto bell = Rectangle<float> (b.h (0.42f), b.h (0.24f)).withCentre (p1 + Point<float> (b.w (0.06f), b.h (0.06f)));
            g.setGradientFill (brass (bell));
            g.fillPath (tube (p1, bell.getCentre(), b.h (0.2f), b.h (0.4f)));
            // Brass bands
            g.setColour (Colour (0xffe1bd62));
            for (float t : { 0.12f, 0.45f, 0.72f })
            {
                const auto p = p0 + (p1 - p0) * t;
                g.fillPath (tube (p - (p1 - p0) * 0.01f, p + (p1 - p0) * 0.01f, b.h (0.08f + 0.12f * t), b.h (0.08f + 0.12f * t)));
            }
            // Holes and reed
            g.setColour (Colour (0xff1f140c));
            for (int i = 0; i < 6; ++i)
            {
                const auto p = p0 + (p1 - p0) * (0.22f + 0.07f * (float) i);
                g.fillEllipse (p.x - 1.5f, p.y - 1.5f, 3.0f, 3.0f);
            }
            g.setColour (Colour (0xffc9a86a));
            g.drawLine (p0.x, p0.y, p0.x - b.w (0.04f), p0.y - b.h (0.08f), 2.0f);
            break;
        }

        default:
            break;
    }
}

//==============================================================================
void drawIcon (juce::Graphics& g, Icon icon, Rectangle<float> a, Colour colour)
{
    Box b { a };
    g.setColour (colour);
    const juce::PathStrokeType stroke (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    Path p;

    switch (icon)
    {
        case Icon::Wave:
            p.startNewSubPath (b.p (0.0f, 0.5f));
            p.lineTo (b.p (0.25f, 0.5f)); p.lineTo (b.p (0.38f, 0.15f)); p.lineTo (b.p (0.55f, 0.85f));
            p.lineTo (b.p (0.68f, 0.35f)); p.lineTo (b.p (0.78f, 0.5f)); p.lineTo (b.p (1.0f, 0.5f));
            g.strokePath (p, stroke);
            break;
        case Icon::Bars:
            for (int i = 0; i < 5; ++i)
            {
                const float hgt = std::array<float, 5> { 0.4f, 0.8f, 0.55f, 1.0f, 0.5f }[(size_t) i];
                const float x = b.x (0.1f + 0.2f * (float) i);
                g.drawLine (x, b.y (0.5f - hgt * 0.45f), x, b.y (0.5f + hgt * 0.45f), 1.6f);
            }
            break;
        case Icon::Copy:
            g.drawRoundedRectangle (Rectangle<float> (b.x (0.3f), b.y (0.3f), b.w (0.62f), b.h (0.62f)), 2.0f, 1.3f);
            g.drawRoundedRectangle (Rectangle<float> (b.x (0.08f), b.y (0.08f), b.w (0.62f), b.h (0.62f)), 2.0f, 1.3f);
            break;
        case Icon::Upload:
            p.startNewSubPath (b.p (0.5f, 0.7f)); p.lineTo (b.p (0.5f, 0.1f));
            p.startNewSubPath (b.p (0.28f, 0.32f)); p.lineTo (b.p (0.5f, 0.1f)); p.lineTo (b.p (0.72f, 0.32f));
            p.startNewSubPath (b.p (0.1f, 0.65f)); p.lineTo (b.p (0.1f, 0.92f)); p.lineTo (b.p (0.9f, 0.92f)); p.lineTo (b.p (0.9f, 0.65f));
            g.strokePath (p, stroke);
            break;
        case Icon::Folder:
            p.startNewSubPath (b.p (0.05f, 0.85f)); p.lineTo (b.p (0.05f, 0.15f)); p.lineTo (b.p (0.38f, 0.15f));
            p.lineTo (b.p (0.48f, 0.3f)); p.lineTo (b.p (0.95f, 0.3f)); p.lineTo (b.p (0.95f, 0.85f)); p.closeSubPath();
            g.strokePath (p, stroke);
            break;
        case Icon::Search:
            g.drawEllipse (Rectangle<float> (b.x (0.08f), b.y (0.08f), b.w (0.58f), b.h (0.58f)), 1.4f);
            g.drawLine (b.x (0.6f), b.y (0.6f), b.x (0.92f), b.y (0.92f), 1.6f);
            break;
        case Icon::Heart:
        case Icon::HeartFilled:
            p.startNewSubPath (b.p (0.5f, 0.9f));
            p.cubicTo (b.p (0.1f, 0.6f), b.p (-0.05f, 0.25f), b.p (0.25f, 0.12f));
            p.cubicTo (b.p (0.38f, 0.07f), b.p (0.48f, 0.15f), b.p (0.5f, 0.27f));
            p.cubicTo (b.p (0.52f, 0.15f), b.p (0.62f, 0.07f), b.p (0.75f, 0.12f));
            p.cubicTo (b.p (1.05f, 0.25f), b.p (0.9f, 0.6f), b.p (0.5f, 0.9f));
            p.closeSubPath();
            if (icon == Icon::HeartFilled) g.fillPath (p);
            else                           g.strokePath (p, stroke);
            break;
        case Icon::LockOpen:
        case Icon::LockClosed:
        {
            auto body = Rectangle<float> (b.x (0.18f), b.y (0.45f), b.w (0.64f), b.h (0.48f));
            p.startNewSubPath (b.p (0.32f, 0.45f));
            p.lineTo (b.p (0.32f, 0.3f));
            p.cubicTo (b.p (0.32f, 0.0f), b.p (0.68f, 0.0f), b.p (0.68f, 0.3f));
            if (icon == Icon::LockClosed) p.lineTo (b.p (0.68f, 0.45f));
            g.strokePath (p, stroke);
            if (icon == Icon::LockClosed) g.fillRoundedRectangle (body, 2.0f);
            else                          g.drawRoundedRectangle (body, 2.0f, 1.3f);
            break;
        }
        case Icon::Regenerate:
        {
            const auto c = a.getCentre();
            const float r = a.getWidth() * 0.36f;
            p.addCentredArc (c.x, c.y, r, r, 0.0f, 0.5f, 2.0f * pi - 0.3f, true);
            g.strokePath (p, stroke);
            const float an = 2.0f * pi - 0.3f;
            const Point<float> e (c.x + r * std::sin (an), c.y - r * std::cos (an));
            Path head;
            head.addTriangle (e.x - 3.5f, e.y - 3.0f, e.x + 3.5f, e.y - 0.5f, e.x - 0.5f, e.y + 3.5f);
            g.fillPath (head);
            break;
        }
        case Icon::Play:
            p.addTriangle (b.p (0.3f, 0.18f), b.p (0.3f, 0.82f), b.p (0.84f, 0.5f));
            g.fillPath (p);
            break;
        case Icon::Pause:
            g.fillRoundedRectangle (Rectangle<float> (b.x (0.25f), b.y (0.2f), b.w (0.17f), b.h (0.6f)), 1.5f);
            g.fillRoundedRectangle (Rectangle<float> (b.x (0.58f), b.y (0.2f), b.w (0.17f), b.h (0.6f)), 1.5f);
            break;
        case Icon::Dots:
            for (int i = 0; i < 3; ++i)
                g.fillEllipse (b.x (0.18f + 0.32f * (float) i) - 2.0f, b.y (0.5f) - 2.0f, 4.0f, 4.0f);
            break;
        case Icon::Grip:
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 2; ++col)
                    g.fillEllipse (b.x (0.3f + 0.4f * (float) col) - 2.0f, b.y (0.2f + 0.3f * (float) row) - 2.0f, 4.0f, 4.0f);
            break;
        case Icon::ChevronLeft:
        case Icon::ChevronRight:
        case Icon::ChevronDown:
        case Icon::ChevronUp:
        {
            const bool h = icon == Icon::ChevronLeft || icon == Icon::ChevronRight;
            const float s = (icon == Icon::ChevronLeft || icon == Icon::ChevronUp) ? -1.0f : 1.0f;
            if (h) { p.startNewSubPath (b.p (0.5f - 0.15f * s, 0.2f)); p.lineTo (b.p (0.5f + 0.15f * s, 0.5f)); p.lineTo (b.p (0.5f - 0.15f * s, 0.8f)); }
            else   { p.startNewSubPath (b.p (0.2f, 0.5f - 0.15f * s)); p.lineTo (b.p (0.5f, 0.5f + 0.15f * s)); p.lineTo (b.p (0.8f, 0.5f - 0.15f * s)); }
            g.strokePath (p, stroke);
            break;
        }
    }
}

} // namespace jgl::decor
