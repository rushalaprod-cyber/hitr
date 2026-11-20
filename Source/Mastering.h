#pragma once
// Section "master" de h.i.t.r : filtres, TONE, OTT, Sausage Fattener, THERMAL et
// le clipper de ravenn (mêmes formes HARD / SOFT / TUBE, genou, bande, M/S, BODY).
// Filtres, OTT, Fattener et formes de clip repris tels quels de ravenn.
#include <algorithm>
#include <array>
#include <cmath>

namespace gdsp
{
constexpr double pi = 3.14159265358979323846;

//==============================================================================
// Biquad (formules RBJ), forme directe transposée II, en double.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double s1 = 0, s2 = 0;

    void reset() noexcept { s1 = s2 = 0; }
    inline double process (double x) noexcept
    {
        const double y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
    {
        const double inv = 1.0 / na0;
        b0 = nb0 * inv; b1 = nb1 * inv; b2 = nb2 * inv;
        a1 = na1 * inv; a2 = na2 * inv;
    }
    static double clampF (double fs, double f) noexcept { return std::clamp (f, 1.0, fs * 0.49); }

    void lowpass (double fs, double f, double q) noexcept
    {
        const double w = 2 * pi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double fs, double f, double q) noexcept
    {
        const double w = 2 * pi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void peak (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2 * pi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2 * pi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        const double sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2 * pi * clampF (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        const double sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

// Linkwitz-Riley 24 dB/oct
struct LR4
{
    Biquad a, b;
    void setLowpass (double fs, double f) noexcept  { a.lowpass (fs, f, 0.7071067811865476);  b.lowpass (fs, f, 0.7071067811865476); }
    void setHighpass (double fs, double f) noexcept { a.highpass (fs, f, 0.7071067811865476); b.highpass (fs, f, 0.7071067811865476); }
    inline double process (double x) noexcept { return b.process (a.process (x)); }
    void reset() noexcept { a.reset(); b.reset(); }
};

// Filtre de bande : passe-haut (si lowHz > 0) puis passe-bas (si highHz > 0).
struct BandFilter
{
    LR4 hp, lp;
    bool useHP = false, useLP = false;
    double lastLo = -1, lastHi = -1, lastFs = -1;
    void set (double fs, double lowHz, double highHz) noexcept
    {
        if (lowHz == lastLo && highHz == lastHi && fs == lastFs) return;
        lastLo = lowHz; lastHi = highHz; lastFs = fs;
        useHP = lowHz > 0; useLP = highHz > 0;
        if (useHP) hp.setHighpass (fs, lowHz);
        if (useLP) lp.setLowpass (fs, highHz);
    }
    inline double process (double x) noexcept
    {
        if (useHP) x = hp.process (x);
        if (useLP) x = lp.process (x);
        return x;
    }
    void reset() noexcept { hp.reset(); lp.reset(); }
};

// Rampe linéaire pour lisser les paramètres (pas de "zipper noise").
struct Ramp
{
    double cur = 0, tgt = 0, stp = 0;
    bool first = true;
    void set (double t, int n) noexcept
    {
        if (first) { cur = tgt = t; stp = 0; first = false; return; }
        tgt = t;
        stp = n > 0 ? (tgt - cur) / n : 0.0;
        if (stp == 0.0) cur = tgt;
    }
    inline double next() noexcept { cur += stp; return cur; }
    void finish() noexcept { cur = tgt; stp = 0; }
};

struct DCBlock
{
    double x1 = 0, y1 = 0, R = 0.9995;
    void set (double fs, double f) noexcept { R = 1.0 - 2.0 * pi * f / fs; }
    inline double process (double x) noexcept { const double y = x - x1 + R * y1; x1 = x; y1 = y; return y; }
    void reset() noexcept { x1 = y1 = 0; }
};

// TONE : tilt (pivot 1 kHz) + low shelf 100 Hz + air 12 kHz (ColorEQ de ravenn).
struct ColorEQ
{
    Biquad tLo, tHi, low, air;
    double lt = 1e9, ll = 1e9, la = 1e9;
    void set (double fs, double tiltDb, double lowDb, double airDb) noexcept
    {
        if (tiltDb == lt && lowDb == ll && airDb == la) return;
        lt = tiltDb; ll = lowDb; la = airDb;
        tLo.lowShelf (fs, 1000.0, 0.5, -tiltDb * 0.5);
        tHi.highShelf (fs, 1000.0, 0.5, tiltDb * 0.5);
        low.lowShelf (fs, 100.0, 0.707, lowDb);
        air.highShelf (fs, 12000.0, 0.707, airDb);
    }
    inline double process (double x) noexcept { return air.process (low.process (tHi.process (tLo.process (x)))); }
    void reset() noexcept { tLo.reset(); tHi.reset(); low.reset(); air.reset(); lt = ll = la = 1e9; }
};

//==============================================================================
// Formes de clip (ravenn)
namespace shape
{
    inline double hard (double x) noexcept { return std::clamp (x, -1.0, 1.0); }
    inline double soft (double x, double k) noexcept
    {
        const double a = std::abs (x);
        if (a <= k) return x;
        const double s = k + (1.0 - k) * std::tanh ((a - k) / (1.0 - k));
        return x < 0 ? -s : s;
    }
    inline double analog (double x) noexcept { return std::tanh (x + 0.12 * x * x / (1.0 + x * x)); }
    inline double asym (double x, double drive, double bias) noexcept
    {
        const double tb = std::tanh (bias);
        const double slope = 1.0 - tb * tb;
        return (std::tanh (drive * x + bias) - tb) / (drive * slope);
    }
}

//==============================================================================
// OTT (ravenn) : compression multibande haut + bas, 3 bandes 88 Hz / 2,5 kHz.
struct OTT
{
    static constexpr int nb = 3;
    std::array<std::array<LR4, 2>, 2> split;
    std::array<double, nb> env {}, gDb {};
    double att = 0, rel = 0, gS = 0, xLo = 0, xHi = 0, lastTime = -1;

    void prepare (double fs, double timeAmt) noexcept
    {
        xLo = xHi = 0;
        for (int c = 0; c < 2; ++c) { split[0][(size_t) c].setLowpass (fs, 88.0); split[1][(size_t) c].setLowpass (fs, 2500.0); }
        lastTime = -1;
        setTime (fs, timeAmt);
    }
    void setTime (double fs, double timeAmt) noexcept
    {
        if (timeAmt == lastTime) return;
        lastTime = timeAmt;
        const double tf = std::pow (10.0, (timeAmt - 0.5) * 1.4);
        auto c = [fs] (double ms) { return 1.0 - std::exp (-1.0 / (ms * 0.001 * fs)); };
        att = c (5.0 * tf); rel = c (90.0 * tf); gS = c (2.0);
    }
    void reset() noexcept
    {
        for (auto& s : split) for (auto& f : s) f.reset();
        env.fill (0); gDb.fill (0);
    }
    void process (double* x, int nch, double depth, const double* bandAmt) noexcept
    {
        double band[nb][2] {};
        for (int c = 0; c < nch; ++c)
        {
            const double lo = split[0][(size_t) c].process (x[c]);
            const double rest = x[c] - lo;
            const double mid = split[1][(size_t) c].process (rest);
            band[0][c] = lo; band[1][c] = mid; band[2][c] = rest - mid;
        }
        for (int b = 0; b < nb; ++b)
        {
            double p = 0;
            for (int c = 0; c < nch; ++c) p = std::max (p, band[b][c] * band[b][c]);
            env[(size_t) b] += (p > env[(size_t) b] ? att : rel) * (p - env[(size_t) b]);
            const double L = 10.0 * std::log10 (env[(size_t) b] + 1.0e-12);
            constexpr double thrDown = -18.0, thrUp = -36.0;
            double t = 0;
            if (L > thrDown) t = -(L - thrDown) * (1.0 - 1.0 / 4.0);
            else if (L < thrUp)
            {
                const double fade = std::clamp ((L + 75.0) / 10.0, 0.0, 1.0);
                t = std::min (18.0, (thrUp - L) * (1.0 - 1.0 / 3.0)) * fade;
            }
            gDb[(size_t) b] += gS * (t - gDb[(size_t) b]);
        }
        for (int c = 0; c < nch; ++c)
        {
            double y = 0;
            for (int b = 0; b < nb; ++b)
            {
                const double g = std::pow (10.0, (gDb[(size_t) b] + 6.0) / 20.0);
                y += band[b][c] * (1.0 + depth * bandAmt[b] * (g - 1.0));
            }
            x[c] = y;
        }
    }
};

//==============================================================================
// SAUSAGE FATTENER (ravenn) : compression écrasante + saturation, COLOR sombre -> mordant.
struct Fattener
{
    std::array<Biquad, 2> pre, post, low;
    double env = 0, aC = 0, rC = 0, fs = 0, lastAmt = -1, lastCol = -1;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        aC = 1.0 - std::exp (-1.0 / (0.0008 * fs));
        rC = 1.0 - std::exp (-1.0 / (0.06 * fs));
        lastAmt = lastCol = -1;
    }
    void reset() noexcept { for (auto* a : { &pre, &post, &low }) for (auto& f : *a) f.reset(); env = 0; }
    void set (double amt, double col) noexcept
    {
        if (std::abs (amt - lastAmt) < 1e-4 && std::abs (col - lastCol) < 1e-4) return;
        lastAmt = amt; lastCol = col;
        const double tilt = (col - 0.5) * 12.0;
        for (int c = 0; c < 2; ++c)
        {
            pre[(size_t) c].highShelf (fs, 1500, 0.5, tilt);
            post[(size_t) c].highShelf (fs, 1500, 0.5, -tilt * 0.8);
            low[(size_t) c].lowShelf (fs, 90, 0.7, 2.5 * amt);
        }
    }
    inline void process (double* x, int nch, double amt) noexcept
    {
        const double drive = std::pow (10.0, 18.0 * amt / 20.0);
        double det = 0;
        for (int c = 0; c < nch; ++c) det = std::max (det, std::abs (x[c]) * drive);
        env += (det > env ? aC : rC) * (det - env);
        constexpr double thr = 0.25, ratio = 8.0;
        const double gr = env > thr ? std::pow (env / thr, -(1.0 - 1.0 / ratio)) : 1.0;
        const double wetAmt = std::min (1.0, amt * 1.5);
        for (int c = 0; c < nch; ++c)
        {
            double v = pre[(size_t) c].process (x[c]) * drive * gr;
            v = std::tanh (v * 1.2) / 1.2;
            v = low[(size_t) c].process (post[(size_t) c].process (v)) * 2.2;
            x[c] += wetAmt * (v - x[c]);
        }
    }
};

//==============================================================================
// THERMAL : distorsion à 2 étages en mouvement (esprit Output Thermal).
// HEAT = quantité, 6 caractères, MOVE = modulation du drive et du filtre par un LFO
// calé sur le tempo + l'enveloppe du signal, TONE = sombre -> brillant.
struct Thermal
{
    enum Type { Tube, Fuzz, Fold, Rectify, Bits, Melt, numTypes };

    static double stage1 (int type, double x, double d) noexcept
    {
        switch (type)
        {
            case Fuzz:    { const double y = std::clamp (x * d * 1.6, -1.0, 1.0); return 0.75 * y + 0.25 * std::tanh (3.0 * y); }
            case Fold:    return std::sin (x * d * 1.25) * 0.9;
            case Rectify: { const double t = std::tanh (x * d); return 0.55 * t + 0.45 * (std::abs (t) - 0.35); }
            case Bits:    { const double t = std::tanh (x * d * 0.8); const double q = 2.0 + 30.0 / d; return std::round (t * q) / q; }
            case Melt:    return std::tanh (x * d * 0.7) * 0.95;
            default:      return shape::asym (x, d, 0.25) * std::min (1.0, 0.8 + 0.2 * d) / (1.0 + 0.04 * d);
        }
    }

    // courbe de transfert complète (pour l'affichage)
    static double curve (int type, double x, double heat) noexcept
    {
        const double d = 1.0 + heat * 14.0;
        const double s1 = stage1 (type, x, d);
        return std::tanh (s1 * (1.0 + heat * 2.0)) / std::tanh (1.0 + heat * 2.0);
    }

    std::array<Biquad, 2> preTilt, postTilt;
    std::array<double, 2> lp {}, slew {}, held {};
    int holdCount = 0;
    double env = 0, envA = 0, envR = 0, fs = 48000, lastTone = -1;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        envA = 1.0 - std::exp (-1.0 / (0.002 * fs));
        envR = 1.0 - std::exp (-1.0 / (0.12 * fs));
        lastTone = -1;
        setRates();
        reset();
    }
    void reset() noexcept
    {
        for (auto* a : { &preTilt, &postTilt }) for (auto& f : *a) f.reset();
        lp.fill (0); slew.fill (0); held.fill (0); env = 0; holdCount = 0;
        dryE = wetE = 0; makeup = 1.0;
    }
    void setTone (double tone) noexcept
    {
        if (std::abs (tone - lastTone) < 1e-4) return;
        lastTone = tone;
        const double tilt = (tone - 0.5) * 14.0;
        for (int c = 0; c < 2; ++c)
        {
            preTilt[(size_t) c].highShelf (fs, 1200, 0.5, tilt);
            postTilt[(size_t) c].highShelf (fs, 1200, 0.5, -tilt * 0.6);
        }
    }

    // lfo dans [-1, 1] fourni par l'appelant (calé sur le tempo)
    inline void process (double* x, int nch, double heat, int type, double move, double lfo, double mix) noexcept
    {
        double det = 0;
        for (int c = 0; c < nch; ++c) det = std::max (det, std::abs (x[c]));
        env += (det > env ? envA : envR) * (det - env);
        const double mod = std::clamp (move * (0.65 * lfo + 0.8 * std::min (1.0, env * 3.0) - 0.3), -1.0, 1.0);
        const double h = std::clamp (heat * (1.0 + 0.6 * mod), 0.0, 1.5);
        const double d = 1.0 + h * 14.0;
        const double cutoff = std::clamp (3500.0 * std::pow (2.0, 2.2 * mod + 1.5 * (lastTone - 0.5)), 400.0, fs * 0.45);
        const double a = std::exp (-2.0 * pi * cutoff / fs);
        const double bitsHold = 1.0 + h * 10.0;
        const bool doHold = type == Bits && ++holdCount >= (int) bitsHold;
        if (doHold) holdCount = 0;

        double wet[2] {}, dryIn[2] {};
        for (int c = 0; c < nch; ++c)
        {
            const double dry = x[c];
            dryIn[c] = dry;
            double v = preTilt[(size_t) c].process (dry);
            v = stage1 (type, v, d);
            if (type == Bits)
            {
                if (doHold) held[(size_t) c] = v;
                v = held[(size_t) c];
            }
            if (type == Melt)   // fonte : la pente est limitée, le son coule
            {
                const double maxStep = 0.02 + 0.5 * (1.0 - std::min (1.0, h));
                slew[(size_t) c] += std::clamp (v - slew[(size_t) c], -maxStep, maxStep);
                v = slew[(size_t) c];
            }
            // 2e étage : saturation douce, puis filtre qui bouge avec MOVE
            v = std::tanh (v * (1.0 + h * 2.0)) / std::tanh (1.0 + h * 2.0);
            lp[(size_t) c] = v * (1.0 - a) + lp[(size_t) c] * a;
            const double fMix = 0.35 + 0.65 * move;
            v = lp[(size_t) c] * fMix + v * (1.0 - fMix);
            wet[c] = postTilt[(size_t) c].process (v);
        }

        // niveau : le signal chauffé est ramené au niveau du signal d'origine (HEAT ne fait pas sauter le volume)
        double pd = 0, pw = 0;
        for (int c = 0; c < nch; ++c) { pd += dryIn[c] * dryIn[c]; pw += wet[c] * wet[c]; }
        dryE += lvl * (pd - dryE);
        wetE += lvl * (pw - wetE);
        if (dryE > 1.0e-9 && wetE > 1.0e-12)
            makeup += mk * (std::clamp (std::sqrt (dryE / wetE), 0.05, 4.0) - makeup);

        const double amt = mix * std::min (1.0, heat * 4.0);
        for (int c = 0; c < nch; ++c)
            x[c] = dryIn[c] + amt * (wet[c] * makeup - dryIn[c]);
    }

    double dryE = 0, wetE = 0, makeup = 1.0, lvl = 0.0005, mk = 0.0002;
    void setRates() noexcept
    {
        lvl = 1.0 - std::exp (-1.0 / (0.08 * fs));
        mk  = 1.0 - std::exp (-1.0 / (0.05 * fs));
    }
};
} // namespace gdsp
