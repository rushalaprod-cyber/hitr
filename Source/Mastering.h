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
// Rattrapage de niveau : ramène un signal traité au niveau du signal d'origine.
// Descend vite (aucun pic quand l'effet s'active), remonte lentement (pas de pompage).
struct LevelMatch
{
    double dE = 0, wE = 0, g = 1.0, det = 0, down = 0, up = 0;
    double lo = 0.05, hi = 1.5, bias = 1.0;
    void prepare (double fs, double detMs = 12.0, double downMs = 1.5, double upMs = 180.0)
    {
        auto c = [fs] (double ms) { return 1.0 - std::exp (-1.0 / (ms * 0.001 * fs)); };
        det = c (detMs); down = c (downMs); up = c (upMs);
        reset();
    }
    void reset() noexcept { dE = wE = 0; g = 1.0; }
    inline double process (double dryPow, double wetPow) noexcept
    {
        dE += det * (dryPow - dE);
        wE += det * (wetPow - wE);
        if (dE > 1.0e-10 && wE > 1.0e-12)
        {
            const double t = std::clamp (std::sqrt (dE / wE) * bias, lo, hi);
            g += (t < g ? down : up) * (t - g);
        }
        return g;
    }
};

//==============================================================================
// SAUSAGE FATTENER (ravenn) : compression écrasante + saturation, COLOR sombre -> mordant.
// 1.2.2 : le volume est rattrapé (gras et dense, mais plus de saut de niveau).
struct Fattener
{
    std::array<Biquad, 2> pre, post, low;
    double env = 0, aC = 0, rC = 0, fs = 0, lastAmt = -1, lastCol = -1;
    LevelMatch match;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        aC = 1.0 - std::exp (-1.0 / (0.0008 * fs));
        rC = 1.0 - std::exp (-1.0 / (0.06 * fs));
        lastAmt = lastCol = -1;
        match.prepare (fs, 10.0, 1.0, 200.0);
        match.bias = 1.12;   // un petit +1 dB de "gras" ressenti, pas plus
    }
    void reset() noexcept { for (auto* a : { &pre, &post, &low }) for (auto& f : *a) f.reset(); env = 0; match.reset(); }
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
    // appelé en continu (même à 0) pour que le rattrapage soit toujours prêt
    inline void process (double* x, int nch, double amt) noexcept
    {
        const double drive = std::pow (10.0, 18.0 * amt / 20.0);
        double det = 0;
        for (int c = 0; c < nch; ++c) det = std::max (det, std::abs (x[c]) * drive);
        env += (det > env ? aC : rC) * (det - env);
        constexpr double thr = 0.25, ratio = 8.0;
        const double gr = env > thr ? std::pow (env / thr, -(1.0 - 1.0 / ratio)) : 1.0;
        const double wetAmt = std::min (1.0, amt * 1.5);
        double v[2] {}, pd = 0, pw = 0;
        for (int c = 0; c < nch; ++c)
        {
            double y = pre[(size_t) c].process (x[c]) * drive * gr;
            y = std::tanh (y * 1.2) / 1.2;
            v[c] = low[(size_t) c].process (post[(size_t) c].process (y)) * 2.2;
            pd += x[c] * x[c]; pw += v[c] * v[c];
        }
        const double mk = match.process (pd, pw);
        if (wetAmt <= 0.0) return;
        for (int c = 0; c < nch; ++c)
            x[c] += wetAmt * (v[c] * mk - x[c]);
    }
};

//==============================================================================
// THERMAL : distorsion à 2 étages en mouvement (esprit Output Thermal).
// 1.2.2 : saturation "à niveau constant" (comme les saturations de ravenn) : le signal
// est normalisé par son enveloppe, saturé, puis remis à son niveau. Résultat : aucun pic
// quand on active FUZZ / RECTIFY, le volume reste stable, et HEAT monte progressivement.
struct Thermal
{
    enum Type { Tube, Fuzz, Fold, Rectify, Bits, Melt, numTypes };

    // HEAT -> drive : très doux au début (courbe), fort au bout
    static double driveFor (double heat) noexcept { return 1.0 + 13.0 * std::pow (std::clamp (heat, 0.0, 1.5), 2.2); }

    static double stage1 (int type, double x, double d) noexcept
    {
        switch (type)
        {
            case Fuzz:    { const double y = std::clamp (x * d * 1.3, -1.0, 1.0); return 0.7 * y + 0.3 * std::tanh (2.0 * y); }
            case Fold:    return std::sin (x * (0.9 + 0.55 * (d - 1.0))) * 0.95;
            case Rectify: { const double t = std::tanh (x * d); const double r = 0.2 + 0.2 * std::min (1.0, (d - 1.0) / 8.0);
                            return (1.0 - r) * t + r * (std::abs (t) - 0.5); }   // octave au-dessus, centrée
            case Bits:    { const double t = std::tanh (x * d * 0.8); const double q = 2.0 + 40.0 / d; return std::round (t * q) / q; }
            case Melt:    return std::tanh (x * d * 0.8);
            default:      // lampe : asymétrie légère (harmoniques paires), normalisée par sa valeur max (même niveau à tout HEAT)
            {
                constexpr double b = 0.1;
                const double tb = std::tanh (b);
                return (std::tanh (d * x + b) - tb) / (std::tanh (d + b) - tb);
            }
        }
    }

    // courbe de transfert complète (pour l'affichage), sur une entrée normalisée
    static double curve (int type, double x, double heat) noexcept
    {
        const double d = driveFor (heat);
        const double s1 = stage1 (type, x * 0.9, d);
        const double k = 1.0 + heat * 1.5;
        return std::tanh (s1 * k) / std::tanh (k);
    }

    std::array<Biquad, 2> preTilt, postTilt;
    std::array<double, 2> lp {}, slew {}, held {};
    std::array<DCBlock, 2> dc;
    LevelMatch match;
    int holdCount = 0;
    double e2 = 0, envA = 0, envR = 0, fs = 48000, lastTone = -1;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        envA = 1.0 - std::exp (-1.0 / (0.001 * fs));
        envR = 1.0 - std::exp (-1.0 / (0.08 * fs));
        for (auto& d : dc) d.set (fs, 8.0);
        match.prepare (fs, 15.0, 2.0, 200.0);
        match.lo = 0.4; match.hi = 1.6;
        pkRel = std::exp (-1.0 / (0.05 * fs));
        limRel = 1.0 - std::exp (-1.0 / (0.06 * fs));
        lastTone = -1;
        reset();
    }
    void reset() noexcept
    {
        for (auto* a : { &preTilt, &postTilt }) for (auto& f : *a) f.reset();
        lp.fill (0); slew.fill (0); held.fill (0); e2 = 0; holdCount = 0;
        for (auto& d : dc) d.reset();
        match.reset();
        dryPk = 0; limG = 1.0;
    }
    void setTone (double tone) noexcept
    {
        if (std::abs (tone - lastTone) < 1e-4) return;
        lastTone = tone;
        const double tilt = (tone - 0.5) * 12.0;
        for (int c = 0; c < 2; ++c)
        {
            preTilt[(size_t) c].highShelf (fs, 1200, 0.5, tilt);
            postTilt[(size_t) c].highShelf (fs, 1200, 0.5, -tilt * 0.6);
        }
    }

    // lfo dans [-1, 1] fourni par l'appelant (calé sur le tempo). Appelé en continu (même à HEAT 0).
    inline void process (double* x, int nch, double heat, int type, double move, double lfo, double mix) noexcept
    {
        double p = 0;
        for (int c = 0; c < nch; ++c) p = std::max (p, x[c] * x[c]);
        e2 += (p > e2 ? envA : envR) * (p - e2);
        const double env = std::sqrt (e2);
        constexpr double floorLvl = 1.0e-3;   // -60 dBFS : en dessous, on s'efface (pas de bruit remonté)
        const double ef = std::max (env, floorLvl);
        const double gate = std::min (1.0, env / floorLvl);

        const double mod = std::clamp (move * (0.65 * lfo + 0.5 * std::min (1.0, env * 4.0) - 0.2), -1.0, 1.0);
        const double h = std::clamp (heat * (1.0 + 0.5 * mod), 0.0, 1.5);
        const double d = driveFor (h);
        const double k = 1.0 + h * 1.5;
        const double cutoff = std::clamp (4500.0 * std::pow (2.0, 2.0 * mod + 1.5 * (lastTone - 0.5)), 500.0, fs * 0.45);
        const double a = std::exp (-2.0 * pi * cutoff / fs);
        const bool doHold = type == Bits && ++holdCount >= (int) (1.0 + h * 8.0);
        if (doHold) holdCount = 0;
        const double fMix = 0.25 + 0.6 * move;

        double wet[2] {}, pd = 0, pw = 0;
        for (int c = 0; c < nch; ++c)
        {
            const double v0 = preTilt[(size_t) c].process (x[c]);
            double v = stage1 (type, v0 / ef * 0.9, d);   // entrée normalisée par l'enveloppe de crête (~ +-0.9)
            if (type == Bits)
            {
                if (doHold) held[(size_t) c] = v;
                v = held[(size_t) c];
            }
            if (type == Melt)   // fonte : la pente est limitée, le son coule
            {
                const double maxStep = 0.03 + 0.6 * (1.0 - std::min (1.0, h));
                slew[(size_t) c] += std::clamp (v - slew[(size_t) c], -maxStep, maxStep);
                v = slew[(size_t) c];
            }
            v = std::tanh (v * k) / std::tanh (k);                      // 2e étage
            lp[(size_t) c] = v * (1.0 - a) + lp[(size_t) c] * a;          // filtre qui bouge avec MOVE
            v = lp[(size_t) c] * fMix + v * (1.0 - fMix);
            v = postTilt[(size_t) c].process (v) * ef * 1.05;           // retour au niveau d'origine (crête ~ identique)
            v = v * gate + v0 * (1.0 - gate);
            wet[c] = dc[(size_t) c].process (v);                        // RECTIFY : pas de décalage DC
            pd += x[c] * x[c]; pw += wet[c] * wet[c];
        }
        const double mk = match.process (pd, pw);

        // GARDE-CRÊTE : le son chauffé ne dépasse jamais la crête du son d'origine de plus de ~0,8 dB
        // (attaque instantanée, relâchement 60 ms) — vaut pour les 6 caractères
        double dryPeak = 0, wetPeak = 0;
        for (int c = 0; c < nch; ++c) { dryPeak = std::max (dryPeak, std::abs (x[c])); wetPeak = std::max (wetPeak, std::abs (wet[c] * mk)); }
        dryPk = std::max (dryPeak, dryPk * pkRel);
        const double ceiling = dryPk * 1.12 + 1.0e-9;
        const double need = wetPeak * limG > ceiling ? ceiling / wetPeak : 1.0;
        if (need < limG) limG = need;
        else limG += limRel * (1.0 - limG);

        const double amt = mix * std::min (1.0, heat * 3.0);
        if (amt <= 0.0) return;
        for (int c = 0; c < nch; ++c)
            x[c] += amt * (wet[c] * mk * limG - x[c]);
    }

    double dryPk = 0, limG = 1.0, pkRel = 0.999, limRel = 0.001;
};
} // namespace gdsp
