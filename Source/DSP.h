#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <cmath>

// Briques DSP de h.i.t.r — aucune allocation pendant le traitement.
namespace hitr
{
constexpr float pi    = juce::MathConstants<float>::pi;
constexpr float twoPi = juce::MathConstants<float>::twoPi;

// ---------------------------------------------------------------------------
// Ligne à retard circulaire, lecture interpolée Hermite (4 points) :
// beaucoup moins de perte d'aigus et d'aliasing que l'interpolation linéaire.
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int minSize)
    {
        int size = 16;
        while (size < minSize + 8) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    int size() const { return mask + 1; }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }

    static float hermite (float xm1, float x0, float x1, float x2, float f)
    {
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    // position absolue (en échantillons, peut dépasser la taille : repliée)
    float readAt (double pos) const
    {
        const double fl = std::floor (pos);
        const int i = (int) (static_cast<long long> (fl) & mask);
        const float f = (float) (pos - fl);
        return hermite (buf[(size_t) ((i - 1) & mask)], buf[(size_t) i],
                        buf[(size_t) ((i + 1) & mask)], buf[(size_t) ((i + 2) & mask)], f);
    }

    // délai en échantillons (>= 1), 1 = avant-dernier échantillon écrit
    float read (float delay) const
    {
        delay = juce::jlimit (1.0f, (float) (mask - 4), delay);
        return readAt ((double) (w - 1) - (double) delay + (double) size());
    }
};

// ---------------------------------------------------------------------------
// Saturation tanh anti-aliasée (ADAA du 1er ordre) : pas besoin de suréchantillonner.
struct AdaaTanh
{
    float x1 = 0.0f;
    static float logCosh (float x)
    {
        const float ax = std::abs (x);
        return ax + std::log1p (std::exp (-2.0f * ax)) - 0.69314718f;
    }
    float process (float x)
    {
        const float d = x - x1;
        float y;
        if (std::abs (d) < 1.0e-4f) y = std::tanh (0.5f * (x + x1));
        else                         y = (logCosh (x) - logCosh (x1)) / d;
        x1 = x;
        return y;
    }
    void reset() { x1 = 0.0f; }
};

// ---------------------------------------------------------------------------
// Pitch shifter "tape" à deux têtes croisées (utilisé seulement si GAMME LIBRE
// est activé, et pour le shimmer à l'octave).
struct PitchShifter
{
    DelayLine dl;
    float phase = 0.0f, window = 2048.0f;

    void prepare (double sr)
    {
        window = (float) (sr * 0.06);
        dl.prepare ((int) window * 2 + 16);
        phase = 0.0f;
    }
    void clear() { dl.clear(); phase = 0.0f; }

    float process (float x, float ratio)
    {
        dl.push (x);
        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
        const float g1 = std::sin (pi * phase), g2 = std::sin (pi * p2);
        return dl.read (2.0f + phase * window) * g1 + dl.read (2.0f + p2 * window) * g2;
    }
};

// ---------------------------------------------------------------------------
struct AllpassPhaser
{
    float z[6] {};
    float fb = 0.0f;
    float process (float x, float coef)
    {
        float y = x + fb * 0.5f;
        for (auto& s : z)
        {
            const float o = -coef * y + s;
            s = y + coef * o;
            y = o;
        }
        fb = y;
        return y;
    }
    void clear() { for (auto& s : z) s = 0.0f; fb = 0.0f; }
};

// ---------------------------------------------------------------------------
// Bruit lent lissé pour le chaos.
struct Wander
{
    juce::Random rng;
    float value = 0.0f, target = 0.0f, counter = 0.0f;
    float next (float rateHz, float sr)
    {
        counter -= 1.0f;
        if (counter <= 0.0f)
        {
            target = rng.nextFloat() * 2.0f - 1.0f;
            counter = sr / juce::jmax (0.05f, rateHz);
        }
        value += (target - value) * (rateHz * 6.0f / sr);
        return value;
    }
};

// ---------------------------------------------------------------------------
// Reverb FDN 8 lignes (matrice de Householder), diffusion d'entrée,
// amortissement par ligne et légère modulation anti-métallique.
struct FdnReverb
{
    static constexpr int N = 8;
    DelayLine lines[N];
    float baseMs[N] = { 31.7f, 37.9f, 41.3f, 47.1f, 53.9f, 61.3f, 67.7f, 73.1f };
    float len[N] {}, gain[N] {}, lp[N] {}, modPh[N] {};
    float damp = 0.5f, fs = 48000.0f;

    struct Allpass
    {
        std::vector<float> b; int i = 0; float g = 0.7f;
        void prepare (int n) { b.assign ((size_t) juce::jmax (1, n), 0.0f); i = 0; }
        float process (float x)
        {
            const float d = b[(size_t) i];
            const float v = x - g * d;
            b[(size_t) i] = v;
            i = (i + 1) % (int) b.size();
            return d + g * v;
        }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); }
    };
    Allpass diff[2][4];
    DelayLine pre[2];

    void prepare (double sr)
    {
        fs = (float) sr;
        for (int i = 0; i < N; ++i) { lines[i].prepare ((int) (fs * 0.2f)); modPh[i] = (float) i / (float) N; }
        const float apMs[2][4] = { { 4.7f, 3.6f, 12.7f, 9.3f }, { 5.3f, 3.1f, 11.9f, 8.7f } };
        for (int c = 0; c < 2; ++c)
        {
            for (int k = 0; k < 4; ++k) diff[c][k].prepare ((int) (apMs[c][k] * fs / 1000.0f));
            pre[c].prepare ((int) (fs * 0.1f));
        }
        setParams (0.6f, 0.5f);
    }

    void clear()
    {
        for (auto& l : lines) l.clear();
        for (auto& c : diff) for (auto& a : c) a.clear();
        for (auto& p : pre) p.clear();
        for (auto& v : lp) v = 0.0f;
    }

    void setParams (float size, float damping)
    {
        size = juce::jlimit (0.0f, 1.0f, size);
        const float scale = 0.55f + size * 1.45f;
        const float rt60 = 0.5f + size * size * 9.0f;
        for (int i = 0; i < N; ++i)
        {
            len[i] = baseMs[i] * scale * fs / 1000.0f;
            gain[i] = std::pow (10.0f, -3.0f * len[i] / (rt60 * fs));
        }
        const float fc = 14000.0f * std::pow (0.13f, juce::jlimit (0.0f, 1.0f, damping));
        damp = std::exp (-twoPi * fc / fs);
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        pre[0].push (inL); pre[1].push (inR);
        float l = pre[0].read (0.012f * fs), r = pre[1].read (0.017f * fs);
        for (auto& a : diff[0]) l = a.process (l);
        for (auto& a : diff[1]) r = a.process (r);

        float y[N];
        for (int i = 0; i < N; ++i)
        {
            float d = len[i];
            if (i < 4)
            {
                modPh[i] += (0.31f + 0.07f * (float) i) / fs;
                modPh[i] -= std::floor (modPh[i]);
                d += 0.0007f * fs * std::sin (twoPi * modPh[i]);
            }
            float v = lines[i].read (d);
            lp[i] = v * (1.0f - damp) + lp[i] * damp;
            y[i] = lp[i] * gain[i];
        }
        float sum = 0.0f;
        for (float v : y) sum += v;
        sum *= 2.0f / (float) N;
        static const float sgn[N] = { 1, -1, 1, -1, -1, 1, -1, 1 };
        for (int i = 0; i < N; ++i)
            lines[i].push (y[i] - sum + ((i & 1) ? r : l) * 0.6f * sgn[i]);

        outL = (y[0] - y[2] + y[4] - y[6] + 0.5f * (y[1] - y[5])) * 0.55f;
        outR = (y[1] - y[3] + y[5] - y[7] + 0.5f * (y[2] - y[6])) * 0.55f;
    }
};

// ---------------------------------------------------------------------------
// Granulizer (esprit Portal / Autochroma) : jusqu'à 64 grains fenêtrés Hann,
// position aléatoire (spray), lecture inversée, harmonies d'octave/quinte,
// étalement stéréo, réinjection et gel du tampon (FREEZE).
struct Granulizer
{
    struct Settings
    {
        float sizeMs = 120.0f, intervalSamples = 4800.0f, sprayMs = 100.0f, reverse = 0.0f;
        float feedback = 0.0f, spread = 0.5f, transpose = 0.0f;
        int pitchSet = 0;
        bool freeze = false;
    };

    struct Grain
    {
        bool active = false;
        double pos = 0.0, inc = 1.0;
        int len = 0, age = 0;
        float gl = 0.0f, gr = 0.0f;
    };

    DelayLine l, r;
    std::array<Grain, 64> grains;
    std::array<float, 1025> window {};
    juce::Random rng;
    float fs = 48000.0f, counter = 0.0f, fbL = 0.0f, fbR = 0.0f;

    void prepare (double sr)
    {
        fs = (float) sr;
        l.prepare ((int) (fs * 8.0f));
        r.prepare ((int) (fs * 8.0f));
        for (size_t i = 0; i < window.size(); ++i)
            window[i] = 0.5f - 0.5f * std::cos (twoPi * (float) i / 1024.0f);
        clear();
    }
    void clear()
    {
        l.clear(); r.clear();
        for (auto& g : grains) g.active = false;
        counter = 0.0f; fbL = fbR = 0.0f;
    }

    static float pickSemis (int set, juce::Random& rnd)
    {
        static const float s0[] = { 0 };
        static const float s1[] = { 0, 0, 12 };
        static const float s2[] = { 0, 0, -12 };
        static const float s3[] = { -12, 0, 0, 12 };
        static const float s4[] = { 0, 7, 12, 19 };
        static const float s5[] = { -24, -12, 0, 12, 24 };
        switch (set)
        {
            case 1: return s1[rnd.nextInt (3)];
            case 2: return s2[rnd.nextInt (3)];
            case 3: return s3[rnd.nextInt (4)];
            case 4: return s4[rnd.nextInt (4)];
            case 5: return s5[rnd.nextInt (5)];
            default: return s0[0];
        }
    }

    void spawn (const Settings& s)
    {
        for (auto& g : grains)
        {
            if (g.active) continue;
            const int len = juce::jmax (64, (int) (s.sizeMs * fs / 1000.0f));
            const float ratio = std::pow (2.0f, (pickSemis (s.pitchSet, rng) + s.transpose) / 12.0f);
            const bool rev = rng.nextFloat() < s.reverse;
            const float spray = rng.nextFloat() * s.sprayMs * fs / 1000.0f;
            // la tête de lecture ne doit jamais rattraper la tête d'écriture
            // (tampon gelé : la tête d'écriture ne bouge plus, il faut toute la longueur du grain)
            const double ahead = (double) ratio - (s.freeze ? 0.0 : 1.0);
            const double margin = 64.0 + (rev ? 0.0 : juce::jmax (0.0, (double) len * ahead));
            const double back = margin + (double) spray + (rev ? (double) len * ratio : 0.0);
            g.pos = (double) l.w - 1.0 - juce::jmin (back, (double) l.size() - (double) len * 4.0 - 64.0);
            g.inc = rev ? -(double) ratio : (double) ratio;
            g.len = len;
            g.age = 0;
            const float pan = juce::jlimit (-1.0f, 1.0f, (rng.nextFloat() * 2.0f - 1.0f) * s.spread);
            g.gl = std::cos ((pan + 1.0f) * pi * 0.25f);
            g.gr = std::sin ((pan + 1.0f) * pi * 0.25f);
            g.active = true;
            return;
        }
    }

    void process (float inL, float inR, float& outL, float& outR, const Settings& s)
    {
        if (! s.freeze)
        {
            l.push (inL + std::tanh (fbL * s.feedback));
            r.push (inR + std::tanh (fbR * s.feedback));
        }

        counter -= 1.0f;
        if (counter <= 0.0f)
        {
            spawn (s);
            counter = juce::jmax (8.0f, s.intervalSamples * (0.8f + 0.4f * rng.nextFloat()));
        }

        float sl = 0.0f, sr = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            const float ph = (float) g.age / (float) g.len * 1024.0f;
            const int i0 = (int) ph;
            const float w = window[(size_t) i0] + (window[(size_t) juce::jmin (1024, i0 + 1)] - window[(size_t) i0]) * (ph - (float) i0);
            sl += l.readAt (g.pos) * w * g.gl;
            sr += r.readAt (g.pos) * w * g.gr;
            g.pos += g.inc;
            if (++g.age >= g.len) g.active = false;
        }
        // normalisation selon le recouvrement moyen des grains
        const float overlap = juce::jmax (1.0f, (s.sizeMs * fs / 1000.0f) / juce::jmax (1.0f, s.intervalSamples));
        const float norm = 1.6f / std::sqrt (overlap);
        outL = sl * norm;
        outR = sr * norm;
        fbL = outL; fbR = outR;
    }
};

// ---------------------------------------------------------------------------
// Bégaiement : recapture un grain récent et le répète (calé sur la grille en sync).
struct Stutter
{
    DelayLine l, r;
    juce::Random rng;
    float sr = 44100.0f;
    int checkCounter = 0;
    bool active = false;
    int grainLen = 0, pos = 0, repeats = 0;
    float xfade = 0.0f;

    void prepare (double s)
    {
        sr = (float) s;
        l.prepare ((int) (sr * 10.0f));
        r.prepare ((int) (sr * 10.0f));
        active = false; xfade = 0.0f;
    }

    // beatSamples > 0 : grille musicale (double-croches), sinon temps libre
    void process (float& L, float& R, float prob, float chaos, float beatSamples)
    {
        l.push (L); r.push (R);

        if (! active && --checkCounter <= 0)
        {
            const bool synced = beatSamples > 0.0f;
            checkCounter = synced ? (int) (beatSamples * 0.25f)
                                  : (int) (sr * (0.12f + 0.25f * rng.nextFloat()));
            if (prob > 0.001f && rng.nextFloat() < prob * 0.35f)
            {
                active = true;
                if (synced)
                {
                    static const float div[] = { 0.125f, 0.25f, 0.25f, 0.5f };
                    grainLen = (int) (beatSamples * div[rng.nextInt (4)]);
                    repeats = 2 << rng.nextInt (2 + (int) (prob * 2.0f));
                }
                else
                {
                    grainLen = (int) (sr * (0.05f + 0.16f * rng.nextFloat() + 0.2f * chaos * rng.nextFloat()));
                    repeats = 2 + rng.nextInt (3 + (int) (prob * 6.0f));
                }
                grainLen = juce::jmax (256, grainLen);
                repeats = juce::jmin (repeats, (int) (sr * 4.0f) / grainLen);
                pos = 0;
            }
        }

        xfade += ((active ? 1.0f : 0.0f) - xfade) * 0.004f;

        if (xfade > 0.0005f && grainLen > 0)
        {
            const int inGrain = pos % grainLen;
            const float readDelay = (float) grainLen + (float) pos - (float) inGrain;
            const float env = juce::jmin (1.0f, (float) inGrain / 96.0f, (float) (grainLen - inGrain) / 96.0f);
            L = L * (1.0f - xfade) + l.read (readDelay) * env * xfade;
            R = R * (1.0f - xfade) + r.read (readDelay) * env * xfade;
        }

        if (active)
        {
            if (++pos >= grainLen * repeats) active = false;
        }
        else pos = 0;
    }
};

// ---------------------------------------------------------------------------
// Trous noirs : coupures douces aléatoires.
struct Dropout
{
    juce::Random rng;
    float sr = 44100.0f, gain = 1.0f;
    int counter = 0, holdLeft = 0;
    void prepare (double s) { sr = (float) s; gain = 1.0f; counter = 0; holdLeft = 0; }
    float next (float prob)
    {
        if (holdLeft > 0) --holdLeft;
        else if (--counter <= 0)
        {
            counter = (int) (sr * (0.3f + 0.7f * rng.nextFloat()));
            if (rng.nextFloat() < prob * 0.5f)
                holdLeft = (int) (sr * (0.08f + 0.5f * rng.nextFloat() * prob));
        }
        const float target = holdLeft > 0 ? 0.03f : 1.0f;
        gain += (target - gain) * 0.0015f;
        return gain;
    }
};
} // namespace hitr
