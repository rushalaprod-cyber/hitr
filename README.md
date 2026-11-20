# h.i.t.r

**Psychoactive melody** — un plugin VST3 / AU qui fait entendre une mélodie comme on la ressentirait sous 25 substances différentes.

![h.i.t.r](dist/apercu-interface.png)

## Les 25 modes

Cannabis · Kétamine · LSD · Psilocybine · DMT · MDMA · Cocaïne · Speed · Méthamphétamine · Héroïne · Alcool · GHB · Lean · Xanax · Mescaline · Ayahuasca · Salvia · 2C-B · Protoxyde · Crack · Caféine · Poppers · Datura · 3-MMC · DXM

Chaque mode a son propre mélange d'effets (espace, filtre qui respire, échos, chorus, phaser, saturation, bégaiements, trous de mémoire, granulizer…), sa palette et son visuel animé.

## Contrôles

| Contrôle | Rôle |
|---|---|
| **DOSE** | de sobre à la dose complète du mode |
| **TRIP** | vitesse des mouvements (en BPM : multiplie la division) |
| **MIX / OUTPUT** | mélange à puissance constante, gain de sortie |
| **WIDTH** | largeur stéréo 0–200 %, basses < 140 Hz gardées en mono |
| **GRAIN / SIZE / DENSITY** | granulizer intégré à chaque mode (50 % = dosage du mode) |
| **FREEZE** | gèle le tampon du granulizer |
| **BPM / MS** | effets temporels calés sur le tempo de l'hôte, ou libres en millisecondes |
| **AUTO / division / TIME** | temps de l'écho (division musicale en BPM, millisecondes en MS) |
| **GAMME LIBRE** | autorise les modes à transposer (coupé par défaut : la gamme ne bouge jamais) |

## Qualité audio

- Lectures interpolées Hermite (wow, chorus, flanger, écho, grains)
- Saturation anti-aliasée (ADAA), reverb FDN 8 lignes modulée
- Modulations de délai bridées (< 7 cents) : pas de désaccord
- Paramètres lissés à l'échantillon, mix à puissance constante, anti-DC
- Gain automatique (même volume perçu dans tous les modes) + limiteur −1 dBFS

## Installer

Télécharge l'installeur dans l'onglet **Releases** :

- **Windows** : `hitr-…-windows.zip` → copie `h.i.t.r.vst3` dans `C:\Program Files\Common Files\VST3\`
- **Mac** (Intel et Apple Silicon) : `hitr-…-macos.pkg` installe le VST3, l'AU et l'application.
  Le plugin n'est pas notarisé par Apple : au premier lancement, clic droit sur le `.pkg` → **Ouvrir**
  (ou Réglages Système → Confidentialité et sécurité → **Ouvrir quand même**).

## Compiler

```
cmake -B build
cmake --build build --config Release --target hitr_VST3 hitr_Standalone hitrTest
```

JUCE 8.0.12 est téléchargé automatiquement (ou `-DJUCE_PATH=...`). `hitrTest` vérifie les 25 modes hors DAW.
Les builds Windows et macOS sont produits par GitHub Actions à chaque push (onglet **Actions** → artefacts).
