#pragma once
#include <JuceHeader.h>
#include <array>
#include <initializer_list>
#include <utility>

// ============================================================================
//  H.I.T.R — table des 25 modes
//  Chaque mode = un jeu de valeurs pour le moteur DSP + une identité visuelle.
//  Le knob DOSE interpole entre "sobre" (neutre) et la valeur du mode.
// ============================================================================

namespace hitr
{
enum P
{
    PITCH,        // demi-tons (-12..+12)
    WOW,          // profondeur wow/flutter en ms (0..25)
    WOW_RATE,     // Hz
    CHORUS,       // 0..1
    LP,           // 0..1  (0 = ouvert, 1 = ~200 Hz)
    HP,           // 0..1  (0 = 20 Hz, 1 = ~1 kHz)
    FLFO,         // 0..1  modulation du passe-bas (respiration)
    FLFO_RATE,    // Hz
    DRIVE,        // 0..1
    CRUSH,        // 0..1
    RING_HZ,      // Hz
    RING,         // 0..1 mix
    TREM,         // 0..1
    TREM_RATE,    // Hz
    PAN,          // 0..1 autopan
    PAN_RATE,     // Hz
    WIDTH,        // 0..2 (1 = normal)
    DLY_MS,       // ms
    DLY_FB,       // 0..0.9
    DLY,          // 0..1 mix
    VERB_SIZE,    // 0..1
    VERB_DAMP,    // 0..1
    VERB,         // 0..1 mix
    SHIMMER,      // 0..1 (octave haute réinjectée dans la reverb)
    PHASER,       // 0..1
    PHASER_RATE,  // Hz
    STUTTER,      // 0..1 probabilité de boucles/répétitions
    DROPOUT,      // 0..1 probabilité de "trous noirs"
    COMP,         // 0..1 écrasement dynamique
    CHAOS,        // 0..1 rend les modulations aléatoires
    FLANGE,       // 0..1
    FLANGE_RATE,  // Hz
    AIR,          // dB shelf aigu
    BASS,         // dB shelf grave
    G_MIX,        // 0..1 quantité de granulizer propre au mode
    G_SIZE,       // ms taille des grains
    G_DENS,       // grains / seconde
    G_SPRAY,      // ms dispersion de position
    G_PITCH,      // jeu d'harmonies (0 unisson, 1 +oct, 2 -oct, 3 ±oct, 4 quinte+oct, 5 octaves larges)
    G_REV,        // 0..1 probabilité de grain inversé
    G_FB,         // 0..0.8 réinjection
    G_SPREAD,     // 0..1 étalement stéréo
    NUM_P
};

using Values = std::array<float, NUM_P>;
constexpr int numModes = 50;

// Paramètres qui ne sont PAS dosés (vitesses, tailles...) : ils prennent
// directement la valeur du mode.
inline bool isDoseScaled (int i)
{
    switch (i)
    {
        case WOW_RATE: case FLFO_RATE: case RING_HZ: case TREM_RATE: case PAN_RATE:
        case DLY_MS: case DLY_FB: case VERB_SIZE: case VERB_DAMP: case PHASER_RATE:
        case FLANGE_RATE: case G_SIZE: case G_DENS: case G_SPRAY: case G_PITCH: case G_SPREAD:
            return false;
        default:
            return true;
    }
}

inline Values neutralValues()
{
    Values v {};
    v.fill (0.0f);
    v[WOW_RATE] = 0.5f;  v[FLFO_RATE] = 0.2f; v[RING_HZ] = 30.0f; v[TREM_RATE] = 4.0f;
    v[PAN_RATE] = 0.2f;  v[WIDTH] = 1.0f;     v[DLY_MS] = 350.0f; v[DLY_FB] = 0.35f;
    v[VERB_SIZE] = 0.6f; v[VERB_DAMP] = 0.5f; v[PHASER_RATE] = 0.3f; v[FLANGE_RATE] = 0.2f;
    v[G_SIZE] = 120.0f;  v[G_DENS] = 10.0f;   v[G_SPRAY] = 100.0f; v[G_SPREAD] = 0.5f;
    return v;
}

enum class Visual
{
    Smoke, Tunnel, Kaleido, Waves, Geometry, Pulse, Strobe, Glitch, Melt, Rays, Vortex, Aurora, Lattice, Mandala, Rain
};

struct Mode
{
    const char* name;      // UTF-8
    const char* shortName; // pour la grille
    const char* desc;      // UTF-8, ressenti
    juce::uint32 c1, c2, c3;
    Visual visual;
    Values values;
};

inline Mode makeMode (const char* name, const char* shortName, const char* desc,
                      juce::uint32 c1, juce::uint32 c2, juce::uint32 c3, Visual vis,
                      std::initializer_list<std::pair<int, float>> settings)
{
    Mode m { name, shortName, desc, c1, c2, c3, vis, neutralValues() };
    for (auto& s : settings)
        m.values[(size_t) s.first] = s.second;
    return m;
}

// Granulizer de chaque mode : mix, taille ms, densité/s, spray ms, harmonies, inversion, réinjection, étalement
struct GrainFlavour { float mix, size, dens, spray, pitch, rev, fb, spread; };
inline const std::array<GrainFlavour, numModes>& grainFlavours()
{
    static const std::array<GrainFlavour, numModes> g =
    {{
        { 0.25f, 180, 12,  120, 0, 0.0f,  0.20f, 0.6f },  // cannabis : halo doux
        { 0.50f, 260,  8,  600, 2, 0.3f,  0.40f, 0.8f },  // kétamine : fragments figés, octave basse
        { 0.45f, 120, 20,  200, 3, 0.2f,  0.35f, 1.0f },  // LSD : nuée d'octaves
        { 0.35f, 220, 10,  300, 0, 0.15f, 0.30f, 0.7f },  // psilocybine
        { 0.60f,  80, 30,  400, 4, 0.25f, 0.50f, 1.0f },  // DMT : cristaux quinte + octave
        { 0.30f, 140, 16,   80, 1, 0.0f,  0.20f, 0.9f },  // MDMA : scintillement aigu
        { 0.15f,  40, 30,   20, 0, 0.0f,  0.05f, 0.4f },  // cocaïne : micro-grains secs
        { 0.25f,  30, 40,   30, 1, 0.0f,  0.15f, 0.5f },  // speed
        { 0.45f,  25, 45,  250, 5, 0.5f,  0.30f, 0.8f },  // méthamphétamine : éclats
        { 0.30f, 300,  6,  200, 2, 0.0f,  0.30f, 0.5f },  // héroïne : nappe grave
        { 0.25f, 200,  8,  350, 0, 0.1f,  0.20f, 0.7f },  // alcool
        { 0.35f, 260,  9,  250, 2, 0.1f,  0.30f, 0.8f },  // GHB
        { 0.35f, 320,  7,  150, 2, 0.0f,  0.30f, 0.6f },  // lean
        { 0.20f, 200,  5,  400, 0, 0.0f,  0.10f, 0.3f },  // xanax
        { 0.40f, 180, 14,  300, 4, 0.1f,  0.35f, 0.9f },  // mescaline
        { 0.45f, 200, 14,  500, 3, 0.2f,  0.40f, 1.0f },  // ayahuasca
        { 0.60f,  60, 35,  900, 5, 0.6f,  0.45f, 1.0f },  // salvia : chaos
        { 0.45f, 100, 22,  200, 1, 0.15f, 0.35f, 1.0f },  // 2C-B
        { 0.40f,  70, 25,  150, 2, 0.2f,  0.50f, 0.9f },  // protoxyde
        { 0.30f,  20, 50,   40, 1, 0.1f,  0.20f, 0.6f },  // crack
        { 0.10f,  50, 20,   30, 0, 0.0f,  0.05f, 0.3f },  // caféine
        { 0.30f, 150, 15,  100, 2, 0.0f,  0.30f, 0.8f },  // poppers
        { 0.55f, 300,  8, 1200, 2, 0.5f,  0.45f, 0.9f },  // datura : souvenirs déformés
        { 0.30f,  90, 20,   60, 1, 0.0f,  0.25f, 0.9f },  // 3-MMC
        { 0.50f,  40, 30,  300, 2, 0.3f,  0.30f, 0.4f },  // DXM : robot pixelisé
        { 0.30f, 280,  7,  180, 2, 0.0f,  0.30f, 0.6f },  // morphine
        { 0.35f, 320,  6,  300, 2, 0.1f,  0.35f, 0.7f },  // opium
        { 0.25f, 400,  4,  500, 2, 0.0f,  0.30f, 0.3f },  // fentanyl
        { 0.15f, 200,  8,  100, 0, 0.0f,  0.15f, 0.4f },  // tramadol
        { 0.15f, 160, 10,   80, 0, 0.0f,  0.15f, 0.5f },  // kratom
        { 0.35f, 110, 18,   70, 1, 0.0f,  0.25f, 1.0f },  // ecstasy
        { 0.30f,  60, 28,   50, 1, 0.05f, 0.20f, 0.8f },  // 4-MMC
        { 0.40f, 120, 15,  350, 2, 0.3f,  0.35f, 0.5f },  // PCP
        { 0.45f, 240, 10,  700, 3, 0.2f,  0.40f, 0.9f },  // ibogaïne
        { 0.70f, 300, 12,  800, 4, 0.2f,  0.60f, 1.0f },  // 5-MeO-DMT
        { 0.30f, 180, 12,  200, 3, 0.1f,  0.30f, 0.8f },  // LSA
        { 0.45f,  90, 24,  250, 4, 0.15f, 0.35f, 1.0f },  // DOM
        { 0.40f, 260,  9,  400, 2, 0.25f, 0.35f, 0.7f },  // amanite
        { 0.30f, 320,  6,  500, 2, 0.1f,  0.30f, 0.6f },  // muscade
        { 0.30f, 150, 14,  250, 1, 0.1f,  0.30f, 0.8f },  // absinthe
        { 0.10f,  40, 25,   20, 0, 0.0f,  0.05f, 0.3f },  // adderall
        { 0.05f,  40, 20,   20, 0, 0.0f,  0.05f, 0.3f },  // modafinil
        { 0.10f, 200,  6,  100, 0, 0.0f,  0.10f, 0.4f },  // CBD
        { 0.10f, 120, 10,   80, 0, 0.0f,  0.10f, 0.4f },  // nicotine
        { 0.20f, 220,  8,  150, 0, 0.0f,  0.20f, 0.6f },  // kava
        { 0.15f,  60, 20,   40, 1, 0.0f,  0.15f, 0.5f },  // khat
        { 0.40f, 240,  8,  600, 3, 0.2f,  0.35f, 0.9f },  // zolpidem
        { 0.45f,  50, 30,  400, 5, 0.4f,  0.40f, 0.8f },  // spice
        { 0.35f,  70, 25,  150, 2, 0.2f,  0.45f, 0.8f },  // solvants
        { 0.50f, 280,  8, 1000, 2, 0.4f,  0.45f, 0.9f },  // DPH
    }};
    return g;
}

inline const std::array<Mode, numModes>& modes()
{
    static const std::array<Mode, numModes> table = []
    {
    std::array<Mode, numModes> t =
    {{
        makeMode (u8"CANNABIS", u8"WEED",
                  u8"Tout ralentit un peu, les basses t'enveloppent, chaque note résonne plus longtemps dans ta tête.",
                  0xff3bff8a, 0xff0e5f35, 0xffc6ff3d, Visual::Smoke,
                  { {PITCH,-0.4f},{WOW,1.5f},{WOW_RATE,0.25f},{CHORUS,0.35f},{LP,0.25f},{BASS,4.f},{AIR,-2.f},
                    {VERB,0.3f},{VERB_SIZE,0.7f},{DLY,0.15f},{DLY_MS,420.f},{DLY_FB,0.4f},{WIDTH,1.3f},
                    {FLFO,0.15f},{FLFO_RATE,0.1f},{COMP,0.3f} }),

        makeMode (u8"KÉTAMINE", u8"KET",
                  u8"Le son part derrière une vitre. Le temps se fige, des bouts de mélodie bouclent, tu regardes le morceau de l'extérieur.",
                  0xff7fe7ff, 0xff5a3dff, 0xffe8f7ff, Visual::Tunnel,
                  { {PITCH,-2.5f},{WOW,6.f},{WOW_RATE,0.12f},{LP,0.6f},{HP,0.25f},{VERB,0.65f},{VERB_SIZE,0.95f},
                    {VERB_DAMP,0.3f},{WIDTH,0.35f},{PAN,0.6f},{PAN_RATE,0.07f},{STUTTER,0.35f},{DROPOUT,0.15f},
                    {CHORUS,0.3f},{COMP,0.2f},{FLFO,0.2f},{FLFO_RATE,0.05f},{BASS,-3.f} }),

        makeMode (u8"LSD", u8"LSD",
                  u8"Les notes ont des couleurs. Elles tournent autour de toi, se dédoublent à l'octave et respirent en phase.",
                  0xffff2bd6, 0xff2bfff1, 0xffffe600, Visual::Kaleido,
                  { {PHASER,0.6f},{PHASER_RATE,0.25f},{SHIMMER,0.35f},{CHORUS,0.5f},{PAN,0.5f},{PAN_RATE,0.3f},
                    {VERB,0.4f},{AIR,4.f},{FLFO,0.25f},{FLFO_RATE,0.4f},{LP,0.1f},{WIDTH,1.7f},{WOW,2.f},
                    {WOW_RATE,0.6f},{CHAOS,0.2f} }),

        makeMode (u8"PSILOCYBINE", u8"CHAMPI",
                  u8"La mélodie respire comme un être vivant, organique, chaude. Le filtre inspire, expire.",
                  0xffff9b3d, 0xff6bcb3a, 0xff8b4cff, Visual::Waves,
                  { {FLFO,0.45f},{FLFO_RATE,0.12f},{LP,0.3f},{WOW,3.f},{WOW_RATE,0.2f},{CHORUS,0.4f},{VERB,0.45f},
                    {VERB_SIZE,0.8f},{PHASER,0.25f},{PHASER_RATE,0.1f},{BASS,2.f},{WIDTH,1.4f},{TREM,0.15f},
                    {TREM_RATE,0.12f} }),

        makeMode (u8"DMT", u8"DMT",
                  u8"Une onde porteuse bourdonne, tout se fracture en géométrie infinie, les harmoniques montent vers l'hyperespace.",
                  0xffffd84a, 0xffff2e88, 0xff3bf0ff, Visual::Geometry,
                  { {SHIMMER,0.7f},{RING,0.25f},{RING_HZ,55.f},{TREM,0.35f},{TREM_RATE,11.f},{VERB,0.6f},
                    {VERB_SIZE,0.98f},{PHASER,0.4f},{PHASER_RATE,0.8f},{FLANGE,0.3f},{FLANGE_RATE,0.6f},
                    {WIDTH,2.f},{AIR,6.f},{CHAOS,0.3f},{CHORUS,0.4f} }),

        makeMode (u8"MDMA", u8"MD",
                  u8"Chaque note est une caresse. Le son pulse avec ton cœur, s'ouvre en grand, tout est brillant et chaud.",
                  0xffff4fd8, 0xffff9fe5, 0xff7a5cff, Visual::Pulse,
                  { {TREM,0.35f},{TREM_RATE,2.f},{AIR,5.f},{BASS,3.f},{WIDTH,1.8f},{VERB,0.35f},{VERB_SIZE,0.7f},
                    {DRIVE,0.25f},{SHIMMER,0.15f},{CHORUS,0.3f},{COMP,0.4f} }),

        makeMode (u8"COCAÏNE", u8"C",
                  u8"Tout est net, sec, compressé, en avant. Les attaques claquent, aucun espace, un poil trop rapide.",
                  0xffffffff, 0xffbfe9ff, 0xffff3355, Visual::Strobe,
                  { {COMP,0.8f},{AIR,6.f},{HP,0.15f},{DRIVE,0.3f},{PITCH,0.35f},{WIDTH,1.2f},{VERB,0.05f},
                    {DLY,0.1f},{DLY_MS,120.f},{DLY_FB,0.2f},{BASS,-2.f} }),

        makeMode (u8"SPEED", u8"SPEED",
                  u8"Ça accélère, ça grésille, ça tremble vite. Les aigus piquent et les échos te poursuivent.",
                  0xfff7ff00, 0xffff7a00, 0xffffffff, Visual::Strobe,
                  { {PITCH,1.f},{TREM,0.3f},{TREM_RATE,8.f},{HP,0.25f},{DRIVE,0.4f},{COMP,0.6f},{DLY,0.2f},
                    {DLY_MS,90.f},{DLY_FB,0.45f},{AIR,4.f},{CHAOS,0.15f} }),

        makeMode (u8"MÉTHAMPHÉTAMINE", u8"METH",
                  u8"Grain abrasif, paranoïa numérique : le son bégaye, se fissure, sature, rien ne tient en place.",
                  0xff00ffc8, 0xff0066ff, 0xffe0ff4f, Visual::Glitch,
                  { {DRIVE,0.75f},{CRUSH,0.45f},{STUTTER,0.4f},{COMP,0.8f},{HP,0.2f},{AIR,3.f},{RING,0.15f},
                    {RING_HZ,120.f},{CHAOS,0.4f},{PAN,0.3f},{PAN_RATE,3.f},{TREM,0.2f},{TREM_RATE,13.f} }),

        makeMode (u8"HÉROÏNE", u8"H",
                  u8"Du coton chaud partout. Le son descend, s'arrondit, s'étouffe doucement dans une couverture brune.",
                  0xffd9a066, 0xff6b3a1e, 0xffffd9a8, Visual::Melt,
                  { {PITCH,-1.f},{LP,0.55f},{WOW,2.f},{WOW_RATE,0.08f},{VERB,0.55f},{VERB_SIZE,0.85f},{VERB_DAMP,0.8f},
                    {BASS,4.f},{AIR,-6.f},{WIDTH,0.7f},{COMP,0.5f},{DRIVE,0.15f},{CHORUS,0.2f} }),

        makeMode (u8"ALCOOL", u8"ALCOOL",
                  u8"La mélodie tangue, se décale, n'est plus vraiment juste. Les échos arrivent un peu en retard.",
                  0xffffb627, 0xff8c3b00, 0xfffff0b3, Visual::Waves,
                  { {WOW,9.f},{WOW_RATE,0.35f},{CHAOS,0.5f},{PAN,0.45f},{PAN_RATE,0.18f},{LP,0.3f},{DLY,0.25f},
                    {DLY_MS,300.f},{DLY_FB,0.3f},{CHORUS,0.25f},{VERB,0.2f},{PITCH,-0.2f},{DRIVE,0.2f} }),

        makeMode (u8"GHB", u8"GHB",
                  u8"Une vague tiède et floue. Le son flotte, ondule lentement, les contours disparaissent.",
                  0xff2ee6c5, 0xff0a4f6e, 0xffb8fff2, Visual::Smoke,
                  { {LP,0.45f},{TREM,0.25f},{TREM_RATE,0.3f},{WOW,4.f},{WOW_RATE,0.15f},{VERB,0.45f},{VERB_SIZE,0.8f},
                    {CHORUS,0.35f},{BASS,3.f},{WIDTH,1.5f},{PITCH,-0.6f},{COMP,0.3f} }),

        makeMode (u8"LEAN", u8"LEAN",
                  u8"Codéine et sirop violet : tout est ralenti, grave, sirupeux, chopped & screwed.",
                  0xffa23cff, 0xff4b0082, 0xffff6bf0, Visual::Melt,
                  { {PITCH,-4.f},{WOW,2.5f},{WOW_RATE,0.1f},{LP,0.45f},{CHORUS,0.35f},{BASS,5.f},{VERB,0.3f},
                    {STUTTER,0.15f},{AIR,-3.f},{DLY,0.15f},{DLY_MS,500.f},{DLY_FB,0.35f} }),

        makeMode (u8"XANAX", u8"XANAX",
                  u8"Plat, anesthésié, sans émotion. Des trous de mémoire : des morceaux de mélodie disparaissent.",
                  0xffa8c5e6, 0xffe9eef5, 0xff5b6b80, Visual::Smoke,
                  { {COMP,0.9f},{LP,0.4f},{DROPOUT,0.25f},{WIDTH,0.5f},{VERB,0.15f},{AIR,-4.f},{PITCH,-0.3f} }),

        makeMode (u8"MESCALINE", u8"PEYOTE",
                  u8"Désert brûlant au coucher du soleil. Le son vibre de chaleur, phase lentement, s'étire à l'horizon.",
                  0xffff5a1f, 0xffffc23d, 0xffb0186b, Visual::Rays,
                  { {PHASER,0.5f},{PHASER_RATE,0.12f},{DRIVE,0.25f},{SHIMMER,0.25f},{PAN,0.4f},{PAN_RATE,0.1f},
                    {VERB,0.45f},{VERB_SIZE,0.9f},{VERB_DAMP,0.6f},{WOW,2.f},{WOW_RATE,0.3f},{BASS,2.f},
                    {AIR,2.f},{CHORUS,0.3f} }),

        makeMode (u8"AYAHUASCA", u8"AYA",
                  u8"Jungle profonde, chants d'icaros : un battement de chakapa, des basses viscérales, la forêt qui tourne.",
                  0xff1eff8e, 0xff5b1aa8, 0xffffb000, Visual::Vortex,
                  { {BASS,6.f},{TREM,0.3f},{TREM_RATE,5.f},{VERB,0.55f},{VERB_SIZE,0.9f},{FLFO,0.35f},{FLFO_RATE,0.08f},
                    {LP,0.25f},{PHASER,0.3f},{PHASER_RATE,0.15f},{WOW,3.f},{WOW_RATE,0.2f},{CHORUS,0.4f},{WIDTH,1.6f} }),

        makeMode (u8"SALVIA", u8"SALVIA",
                  u8"Réalité arrachée. Le son saute d'un côté à l'autre, se répète, se replie sur lui-même, plus rien n'a de sens.",
                  0xffff00a0, 0xff00ff5a, 0xffffffff, Visual::Vortex,
                  { {CHAOS,1.f},{PAN,0.9f},{PAN_RATE,2.5f},{STUTTER,0.6f},{PITCH,-3.f},{RING,0.3f},{RING_HZ,18.f},
                    {FLANGE,0.5f},{FLANGE_RATE,1.5f},{DROPOUT,0.2f},{WOW,8.f},{WOW_RATE,1.2f},{VERB,0.35f},
                    {CRUSH,0.15f},{WIDTH,2.f} }),

        makeMode (u8"2C-B", u8"2C-B",
                  u8"Couleurs fluo et euphorie : le son scintille, chatoie, s'élargit dans tous les sens.",
                  0xff39ff14, 0xffff00f7, 0xff00e5ff, Visual::Kaleido,
                  { {PHASER,0.45f},{PHASER_RATE,0.5f},{CHORUS,0.55f},{AIR,5.f},{SHIMMER,0.3f},{WIDTH,1.9f},
                    {TREM,0.15f},{TREM_RATE,6.f},{VERB,0.35f},{FLFO,0.2f},{FLFO_RATE,0.5f},{LP,0.1f},{DRIVE,0.15f} }),

        makeMode (u8"PROTOXYDE", u8"PROTO",
                  u8"Wah-wah-wah : le son devient métallique, rebondit en boucle, ta tête se remplit d'écho.",
                  0xffc8d6e5, 0xff8395a7, 0xffff9ff3, Visual::Tunnel,
                  { {FLANGE,0.8f},{FLANGE_RATE,3.5f},{DLY,0.45f},{DLY_MS,280.f},{DLY_FB,0.6f},{TREM,0.45f},
                    {TREM_RATE,3.5f},{PITCH,-1.2f},{LP,0.35f},{VERB,0.45f},{VERB_SIZE,0.85f},{WIDTH,1.5f},{COMP,0.3f} }),

        makeMode (u8"CRACK", u8"CRACK",
                  u8"Rush violent et court : tout explose, sature, monte dans les aigus, puis le son se casse.",
                  0xffff1e1e, 0xffffffff, 0xffffb3b3, Visual::Strobe,
                  { {COMP,1.f},{DRIVE,0.9f},{AIR,8.f},{PITCH,2.f},{HP,0.3f},{STUTTER,0.2f},{TREM,0.25f},
                    {TREM_RATE,16.f},{WIDTH,1.4f},{CRUSH,0.1f},{RING,0.1f},{RING_HZ,300.f} }),

        makeMode (u8"CAFÉINE", u8"CAFÉ",
                  u8"Juste un peu plus éveillé : un son plus vif, plus serré, plus brillant.",
                  0xff8b5a2b, 0xfff5deb3, 0xffffaa33, Visual::Pulse,
                  { {AIR,3.f},{COMP,0.4f},{HP,0.08f},{PITCH,0.15f},{DRIVE,0.1f},{WIDTH,1.15f},{TREM,0.05f},{TREM_RATE,7.f} }),

        makeMode (u8"POPPERS", u8"POPPERS",
                  u8"Bouffée de chaleur : le sang tape, le son gonfle, se balaie d'un coup puis retombe.",
                  0xffffd000, 0xffff0055, 0xffffffff, Visual::Pulse,
                  { {FLANGE,0.5f},{FLANGE_RATE,0.4f},{BASS,5.f},{DRIVE,0.35f},{COMP,0.5f},{LP,0.2f},{FLFO,0.4f},
                    {FLFO_RATE,0.4f},{VERB,0.2f},{PITCH,-0.5f},{WOW,3.f},{WOW_RATE,0.5f} }),

        makeMode (u8"DATURA", u8"DATURA",
                  u8"Délire sombre : des voix lointaines qui n'existent pas, des trous, des morceaux qui reviennent déformés.",
                  0xff8b0000, 0xff1a0000, 0xff556b2f, Visual::Smoke,
                  { {PITCH,-5.f},{LP,0.65f},{DROPOUT,0.3f},{STUTTER,0.3f},{VERB,0.7f},{VERB_SIZE,1.f},{VERB_DAMP,0.9f},
                    {RING,0.2f},{RING_HZ,7.f},{CHAOS,0.6f},{PAN,0.5f},{PAN_RATE,0.05f},{CRUSH,0.25f},{WIDTH,0.8f},{AIR,-6.f} }),

        makeMode (u8"3-MMC", u8"3-MMC",
                  u8"Euphorie électrique : ça pulse, ça brille, ça s'ouvre, envie de monter le volume.",
                  0xff00f5ff, 0xffff2079, 0xffffffff, Visual::Rays,
                  { {TREM,0.25f},{TREM_RATE,2.2f},{AIR,5.f},{WIDTH,1.7f},{DRIVE,0.3f},{COMP,0.55f},{CHORUS,0.3f},
                    {PITCH,0.5f},{SHIMMER,0.1f},{DLY,0.15f},{DLY_MS,180.f},{DLY_FB,0.3f},{VERB,0.2f} }),

        makeMode (u8"DXM", u8"DXM",
                  u8"Plateau robotique : le son devient synthétique, métallique, pixelisé, tu es un robot qui écoute.",
                  0xffff4500, 0xff2f4f4f, 0xffc0c0c0, Visual::Geometry,
                  { {RING,0.4f},{RING_HZ,90.f},{CRUSH,0.35f},{PITCH,-1.5f},{LP,0.3f},{VERB,0.4f},{VERB_SIZE,0.9f},
                    {WOW,3.f},{WOW_RATE,0.1f},{STUTTER,0.2f},{WIDTH,0.4f},{COMP,0.4f},{FLANGE,0.3f},{FLANGE_RATE,0.1f} }),

        // ---------------- 1.2 : 25 substances de plus ----------------
        makeMode (u8"MORPHINE", u8"MORPH",
                  u8"Une chaleur propre qui descend dans les épaules : le son s'arrondit, flotte, plus rien ne pique.",
                  0xffe8c39e, 0xff7a4e2d, 0xfffff1e0, Visual::Melt,
                  { {LP,0.45f},{VERB,0.5f},{VERB_SIZE,0.8f},{VERB_DAMP,0.7f},{BASS,3.f},{AIR,-4.f},{WOW,1.5f},{WOW_RATE,0.07f},
                    {COMP,0.4f},{CHORUS,0.2f},{WIDTH,0.9f},{PITCH,-0.5f} }),

        makeMode (u8"OPIUM", u8"OPIUM",
                  u8"Fumée lourde et rêve éveillé : les notes s'étirent dans une brume ambrée, les échos se perdent au loin.",
                  0xffc0392b, 0xfff39c12, 0xff4a0e0e, Visual::Smoke,
                  { {LP,0.4f},{VERB,0.6f},{VERB_SIZE,0.9f},{VERB_DAMP,0.6f},{PHASER,0.2f},{PHASER_RATE,0.05f},{CHORUS,0.3f},
                    {TREM,0.15f},{TREM_RATE,0.2f},{BASS,3.f},{AIR,-3.f},{DLY,0.2f},{DLY_MS,600.f},{DLY_FB,0.4f},{WIDTH,1.3f} }),

        makeMode (u8"FENTANYL", u8"FENTA",
                  u8"Écrasant. Le son s'enfonce sous l'eau, s'éteint par moments, il ne reste qu'un souffle grave.",
                  0xff6c7a89, 0xff1b1f24, 0xffa9b7c6, Visual::Smoke,
                  { {LP,0.7f},{DROPOUT,0.2f},{VERB,0.7f},{VERB_SIZE,1.f},{VERB_DAMP,0.9f},{COMP,0.7f},{BASS,2.f},{AIR,-9.f},
                    {WIDTH,0.4f},{PITCH,-1.5f},{WOW,1.f},{WOW_RATE,0.05f},{TREM,0.2f},{TREM_RATE,0.1f} }),

        makeMode (u8"TRAMADOL", u8"TRAMA",
                  u8"Un coussin léger : tout est un peu plus doux et plus plat, avec une pointe d'énergie.",
                  0xff9fb4c7, 0xffe2e8f0, 0xff4a5568, Visual::Waves,
                  { {LP,0.25f},{COMP,0.5f},{VERB,0.25f},{BASS,2.f},{AIR,-1.f},{CHORUS,0.15f},{TREM,0.05f},{TREM_RATE,3.f} }),

        makeMode (u8"KRATOM", u8"KRATOM",
                  u8"Feuilles vertes, calme alerte : le son est posé, rond, légèrement plus large.",
                  0xff7fb069, 0xff3d5a3d, 0xffe6aa68, Visual::Waves,
                  { {AIR,1.f},{BASS,2.f},{CHORUS,0.2f},{VERB,0.2f},{VERB_SIZE,0.5f},{COMP,0.3f},{DRIVE,0.1f},{WIDTH,1.1f},{LP,0.1f} }),

        makeMode (u8"ECSTASY", u8"XTC",
                  u8"La pilule de club : les basses tapent dans le ventre, les aigus brillent, tout pulse et s'ouvre en grand.",
                  0xff00e5ff, 0xffff4fd8, 0xfffff35c, Visual::Pulse,
                  { {TREM,0.3f},{TREM_RATE,2.2f},{AIR,6.f},{WIDTH,1.9f},{DRIVE,0.3f},{COMP,0.5f},{SHIMMER,0.2f},{CHORUS,0.35f},
                    {VERB,0.3f},{HP,0.05f},{PITCH,0.2f} }),

        makeMode (u8"4-MMC", u8"4-MMC",
                  u8"Méphédrone : euphorie nerveuse, le son fourmille, accélère, les aigus grésillent.",
                  0xffb6ff00, 0xffff00c8, 0xffffffff, Visual::Strobe,
                  { {TREM,0.25f},{TREM_RATE,4.f},{AIR,5.f},{DRIVE,0.35f},{COMP,0.6f},{WIDTH,1.6f},{STUTTER,0.1f},{DLY,0.15f},
                    {DLY_MS,150.f},{DLY_FB,0.3f},{CHAOS,0.1f},{PITCH,0.4f} }),

        makeMode (u8"PCP", u8"PCP",
                  u8"Poussière d'ange : corps anesthésié, son lointain et métallique, des coupures brutales.",
                  0xffff6b35, 0xff2e2e2e, 0xffd9d9d9, Visual::Glitch,
                  { {DRIVE,0.6f},{LP,0.35f},{WIDTH,0.3f},{VERB,0.45f},{VERB_SIZE,0.7f},{RING,0.2f},{RING_HZ,40.f},{STUTTER,0.25f},
                    {DROPOUT,0.1f},{COMP,0.7f},{CHAOS,0.3f},{PITCH,-2.f} }),

        makeMode (u8"IBOGAÏNE", u8"IBOGA",
                  u8"Voyage intérieur interminable : un bourdonnement sourd, des visions qui défilent en boucle.",
                  0xff8e44ad, 0xff1abc9c, 0xfff4d03f, Visual::Mandala,
                  { {RING,0.2f},{RING_HZ,8.f},{TREM,0.3f},{TREM_RATE,7.f},{VERB,0.55f},{VERB_SIZE,0.9f},{PHASER,0.3f},{PHASER_RATE,0.07f},
                    {BASS,4.f},{LP,0.3f},{FLFO,0.3f},{FLFO_RATE,0.05f},{WIDTH,1.4f},{DLY,0.25f},{DLY_MS,700.f},{DLY_FB,0.5f} }),

        makeMode (u8"5-MEO-DMT", u8"5-MEO",
                  u8"Lumière blanche totale : la mélodie se dissout dans une cathédrale d'harmoniques, plus d'ego, plus de bord.",
                  0xffffffff, 0xfffff6d5, 0xffb8e2ff, Visual::Rays,
                  { {SHIMMER,0.9f},{VERB,0.8f},{VERB_SIZE,1.f},{VERB_DAMP,0.2f},{HP,0.2f},{AIR,8.f},{WIDTH,2.f},{TREM,0.2f},
                    {TREM_RATE,13.f},{CHORUS,0.5f},{COMP,0.3f} }),

        makeMode (u8"LSA", u8"LSA",
                  u8"Graines d'ipomée : un LSD endormi, couleurs pastel, le son ondule mollement.",
                  0xff6a5acd, 0xff87ceeb, 0xffffb6c1, Visual::Kaleido,
                  { {PHASER,0.4f},{PHASER_RATE,0.15f},{CHORUS,0.35f},{LP,0.25f},{VERB,0.35f},{WOW,2.f},{WOW_RATE,0.3f},
                    {PAN,0.3f},{PAN_RATE,0.15f},{BASS,2.f} }),

        makeMode (u8"DOM", u8"DOM",
                  u8"Psychédélique-stimulant très long : grilles de couleurs qui vibrent, énergie qui ne redescend pas.",
                  0xffff3cac, 0xff784ba0, 0xff2b86c5, Visual::Lattice,
                  { {PHASER,0.5f},{PHASER_RATE,0.4f},{SHIMMER,0.3f},{TREM,0.2f},{TREM_RATE,6.f},{DRIVE,0.25f},{AIR,4.f},
                    {WIDTH,1.7f},{PAN,0.4f},{PAN_RATE,0.5f},{CHORUS,0.4f},{CHAOS,0.2f} }),

        makeMode (u8"AMANITE", u8"AMANIT",
                  u8"Tue-mouche : rêve étrange, tailles qui changent, le son grossit et rapetisse, des bouts disparaissent.",
                  0xffe63946, 0xfff1faee, 0xff2a9d8f, Visual::Mandala,
                  { {PITCH,-1.f},{WOW,4.f},{WOW_RATE,0.15f},{LP,0.4f},{VERB,0.5f},{VERB_SIZE,0.8f},{STUTTER,0.2f},{DROPOUT,0.1f},
                    {FLFO,0.3f},{FLFO_RATE,0.1f},{CHORUS,0.3f},{BASS,3.f},{CHAOS,0.3f} }),

        makeMode (u8"MUSCADE", u8"MUSCADE",
                  u8"Lourd et lent à venir : le son devient épais, cotonneux, avec des absences.",
                  0xffa0522d, 0xff3e2723, 0xffd2b48c, Visual::Smoke,
                  { {LP,0.5f},{VERB,0.5f},{VERB_SIZE,0.85f},{VERB_DAMP,0.8f},{WOW,3.f},{WOW_RATE,0.06f},{DROPOUT,0.15f},
                    {COMP,0.5f},{BASS,3.f},{AIR,-5.f},{STUTTER,0.1f} }),

        makeMode (u8"ABSINTHE", u8"ABSINT",
                  u8"La fée verte : ivresse scintillante, la mélodie tangue sous des reflets émeraude.",
                  0xff7cfc00, 0xff006400, 0xffadff2f, Visual::Aurora,
                  { {WOW,5.f},{WOW_RATE,0.3f},{CHAOS,0.3f},{PHASER,0.3f},{PHASER_RATE,0.2f},{SHIMMER,0.2f},{VERB,0.35f},
                    {DLY,0.2f},{DLY_MS,330.f},{DLY_FB,0.35f},{PAN,0.3f},{PAN_RATE,0.2f},{AIR,2.f},{DRIVE,0.15f} }),

        makeMode (u8"ADDERALL", u8"ADDY",
                  u8"Focus total : chaque note est nette, sèche, alignée, rien ne dépasse.",
                  0xffff8c00, 0xffffd580, 0xff1e1e1e, Visual::Lattice,
                  { {AIR,3.f},{COMP,0.6f},{HP,0.15f},{PITCH,0.2f},{DRIVE,0.15f},{WIDTH,0.9f},{TREM,0.05f},{TREM_RATE,8.f},{BASS,-1.f} }),

        makeMode (u8"MODAFINIL", u8"MODAF",
                  u8"Éveil clinique : propre, clair, légèrement plus brillant, sans euphorie.",
                  0xffe0f7fa, 0xff00acc1, 0xffffffff, Visual::Lattice,
                  { {AIR,4.f},{COMP,0.35f},{HP,0.1f},{WIDTH,1.1f} }),

        makeMode (u8"CBD", u8"CBD",
                  u8"Presque rien : les angles s'adoucissent, le son respire calmement.",
                  0xffa8e6cf, 0xffdcedc1, 0xff3b7a57, Visual::Waves,
                  { {LP,0.15f},{VERB,0.2f},{VERB_SIZE,0.5f},{BASS,1.5f},{AIR,-1.f},{COMP,0.2f},{CHORUS,0.1f} }),

        makeMode (u8"NICOTINE", u8"NICO",
                  u8"Petite montée de tête : un léger voile qui passe, puis tout redevient net.",
                  0xffbdbdbd, 0xff6d4c41, 0xffffe0b2, Visual::Smoke,
                  { {FLFO,0.2f},{FLFO_RATE,0.3f},{LP,0.15f},{TREM,0.1f},{TREM_RATE,1.f},{COMP,0.3f},{AIR,1.f},{DRIVE,0.1f},{WIDTH,1.1f} }),

        makeMode (u8"KAVA", u8"KAVA",
                  u8"Racine du Pacifique : détente sociale, bouche engourdie, son chaud et posé.",
                  0xffc2a878, 0xff556b2f, 0xfff5f5dc, Visual::Waves,
                  { {LP,0.3f},{VERB,0.3f},{BASS,2.f},{AIR,-2.f},{COMP,0.4f},{CHORUS,0.2f},{WIDTH,1.2f},{TREM,0.1f},{TREM_RATE,0.3f} }),

        makeMode (u8"KHAT", u8"KHAT",
                  u8"Feuilles mâchées : bavard, alerte, le son sautille et brille un peu.",
                  0xff2e8b57, 0xffffd700, 0xfff0fff0, Visual::Strobe,
                  { {AIR,3.f},{TREM,0.15f},{TREM_RATE,5.f},{COMP,0.4f},{DRIVE,0.15f},{HP,0.05f},{PAN,0.2f},{PAN_RATE,1.f} }),

        makeMode (u8"ZOLPIDEM", u8"ZOLPI",
                  u8"Somnifère où l'on reste éveillé : demi-rêve, objets qui fondent, phrases qui s'effacent.",
                  0xff1e3a8a, 0xff93c5fd, 0xfff472b6, Visual::Rain,
                  { {LP,0.4f},{DROPOUT,0.2f},{VERB,0.5f},{VERB_SIZE,0.85f},{WOW,3.f},{WOW_RATE,0.1f},{STUTTER,0.2f},
                    {CHAOS,0.3f},{WIDTH,1.4f},{BASS,2.f},{SHIMMER,0.15f} }),

        makeMode (u8"SPICE", u8"SPICE",
                  u8"Cannabinoïde de synthèse : panique numérique, boucles qui se coincent, son dur et sale.",
                  0xff39ff14, 0xffff073a, 0xff0d0d0d, Visual::Glitch,
                  { {CRUSH,0.3f},{STUTTER,0.35f},{DRIVE,0.5f},{LP,0.3f},{DROPOUT,0.15f},{CHAOS,0.5f},{RING,0.15f},{RING_HZ,60.f},
                    {PAN,0.4f},{PAN_RATE,2.f},{COMP,0.6f},{VERB,0.25f} }),

        makeMode (u8"SOLVANTS", u8"SOLV",
                  u8"Colle et gaz : bourdonnement métallique, effet tunnel, le son vibre dans le crâne.",
                  0xffd4d4d4, 0xffff9800, 0xff607d8b, Visual::Tunnel,
                  { {FLANGE,0.7f},{FLANGE_RATE,5.f},{RING,0.25f},{RING_HZ,25.f},{LP,0.45f},{TREM,0.4f},{TREM_RATE,6.f},{DLY,0.3f},
                    {DLY_MS,200.f},{DLY_FB,0.5f},{DROPOUT,0.1f},{COMP,0.4f},{WIDTH,1.3f} }),

        makeMode (u8"DPH", u8"DPH",
                  u8"Délirant antihistaminique : silhouettes dans le coin de l'œil, voix fantômes, la pièce n'est plus la même.",
                  0xff4b0082, 0xff000000, 0xff8b008b, Visual::Rain,
                  { {LP,0.55f},{DROPOUT,0.25f},{STUTTER,0.3f},{VERB,0.6f},{VERB_SIZE,0.95f},{VERB_DAMP,0.85f},{RING,0.15f},
                    {RING_HZ,5.f},{CHAOS,0.5f},{PAN,0.45f},{PAN_RATE,0.07f},{CRUSH,0.2f},{AIR,-6.f},{BASS,2.f} }),
    }};
    for (size_t i = 0; i < t.size(); ++i)
    {
        const auto& g = grainFlavours()[i];
        auto& v = t[i].values;
        v[G_MIX] = g.mix; v[G_SIZE] = g.size; v[G_DENS] = g.dens; v[G_SPRAY] = g.spray;
        v[G_PITCH] = g.pitch; v[G_REV] = g.rev; v[G_FB] = g.fb; v[G_SPREAD] = g.spread;
    }
    return t;
    }();
    return table;
}

inline juce::StringArray modeNames()
{
    juce::StringArray s;
    for (auto& m : modes())
        s.add (juce::String::fromUTF8 (m.name));
    return s;
}

// Couches de mélange : "AUCUN" + les 50 substances
inline juce::StringArray layerNames()
{
    juce::StringArray s { juce::String::fromUTF8 (u8"AUCUN") };
    s.addArray (modeNames());
    return s;
}

// Bornes de chaque valeur après mélange / overdose
inline float clampValue (int i, float v)
{
    switch (i)
    {
        case PITCH:  return juce::jlimit (-12.0f, 12.0f, v);
        case WOW:    return juce::jlimit (0.0f, 25.0f, v);
        case LP:     return juce::jlimit (0.0f, 0.85f, v);
        case HP:     return juce::jlimit (0.0f, 0.6f, v);
        case WIDTH:  return juce::jlimit (0.0f, 2.5f, v);
        case DLY_FB: return juce::jlimit (0.0f, 0.88f, v);
        case G_FB:   return juce::jlimit (0.0f, 0.8f, v);
        case AIR: case BASS: return juce::jlimit (-12.0f, 10.0f, v);
        case WOW_RATE: case FLFO_RATE: case RING_HZ: case TREM_RATE: case PAN_RATE: case DLY_MS:
        case VERB_SIZE: case VERB_DAMP: case PHASER_RATE: case FLANGE_RATE: case G_SIZE: case G_DENS:
        case G_SPRAY: case G_PITCH: case G_SPREAD:
            return v;
        default:     return juce::jlimit (0.0f, 1.0f, v);
    }
}

// Fusion de plusieurs substances : la principale + les écarts au "sobre" des couches
// (pondérés par leur dosage). Les vitesses / tailles sont moyennées.
inline Values mergeModes (int primary, const int* layers, const float* weights, int numLayers)
{
    const auto neutral = neutralValues();
    Values v = modes()[(size_t) primary].values;
    float wSum = 1.0f;
    Values rateSum = v;
    for (int k = 0; k < numLayers; ++k)
    {
        if (layers[k] < 0 || weights[k] <= 0.0f) continue;
        const auto& lv = modes()[(size_t) layers[k]].values;
        for (int i = 0; i < NUM_P; ++i)
        {
            if (isDoseScaled (i)) v[(size_t) i] += weights[k] * (lv[(size_t) i] - neutral[(size_t) i]);
            else                  rateSum[(size_t) i] += weights[k] * lv[(size_t) i];
        }
        wSum += weights[k];
        if (v[G_PITCH] == 0.0f && weights[k] >= 0.5f) v[G_PITCH] = lv[G_PITCH];
    }
    for (int i = 0; i < NUM_P; ++i)
    {
        if (! isDoseScaled (i) && i != G_PITCH) v[(size_t) i] = rateSum[(size_t) i] / wSum;
        v[(size_t) i] = clampValue (i, v[(size_t) i]);
    }
    return v;
}
} // namespace hitr
