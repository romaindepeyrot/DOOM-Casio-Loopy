# DOOM pour Casio Loopy

Portage du moteur Linux DOOM 1.10 sur le SH‑1 de la Casio Loopy. Le dépôt contient les sources, le moteur compilé et le script qui assemble l’image cartouche. Il faut ajouter les données du jeu pour démarrer une partie.

## Créer la ROM

Clonez le dépôt ou décompressez son ZIP. Placez `doom19s.zip` ou `DOOM1.WAD` du shareware 1.9 dans le dossier qui contient `doom-loopy-engine.bin`. L’archive DOS originale est disponible sur le [miroir idgames de la Freie Universität Berlin](https://ftp.fu-berlin.de/pc/games/idgames/idstuff/doom/doom19s.zip).

Avec Python 3, lancez la commande suivante depuis ce dossier. Remplacez `doom19s.zip` par `DOOM1.WAD` si vous utilisez le WAD directement.

```sh
python3 wad/build_rom.py doom-loopy-engine.bin doom19s.zip doom-loopy.bin
```

Sous Windows, remplacez `python3` par `py -3`. Le fichier `doom-loopy.bin` ainsi généré contient le moteur et les données du jeu. Avec le shareware 1.9, il occupe moins de 4 Mio et comporte le checksum attendu par la console.

Chargez `doom-loopy.bin` dans [LoopyMSE 0.5.3](https://github.com/partlyhuman/LoopyMSE/releases/tag/v0.5.3) avec un BIOS Casio Loopy de 32 Kio, ou copiez-le sur une cartouche flash compatible telle que Floopy Drive.

### Rupi sous Windows

Placez votre BIOS Casio Loopy de 32 Kio à côté de la ROM, puis lancez Rupi depuis PowerShell. Adaptez les noms des fichiers si nécessaire.

```powershell
.\Rupi.exe .\doom-loopy.bin .\bios.bin
```

Au premier lancement, `Warning: SRAM not found` signale l’absence de `doom-loopy.sav`. Rupi initialise cette mémoire automatiquement. Attendez quelques secondes que le moteur charge les textures.

Avec les touches par défaut de Rupi, Entrée ouvre le menu, Z valide ou tire, X annule ou ouvre une porte, A fait courir, S change d’arme, et Q + W affiche la carte.

## Manette

| Commande | En jeu |
| --- | --- |
| ↑ / ↓ | Avancer / reculer |
| ← / → | Tourner |
| A | Tirer |
| B | Utiliser ou ouvrir |
| C maintenu | Courir |
| D | Passer à la prochaine arme disponible |
| L / R | Pas latéral gauche / droit |
| L + R | Afficher ou fermer la carte |
| Start | Ouvrir ou fermer le menu |

Dans les menus, utilisez la croix pour sélectionner, A pour valider et B pour revenir.

## Recompiler

Installez Wonderful Toolchain et son paquet `toolchain-gcc-sh-elf`, qui fournit le compilateur SH et Picolibc. Python 3 et GNU Make sont nécessaires.

```sh
export PATH="/opt/wonderful/toolchain/gcc-sh-elf/bin:$PATH"
make
make rom IWAD=/chemin/vers/doom19s.zip
```

`make` produit le moteur dans `build/doom-loopy-engine.bin`. `make rom` assemble `build/doom-loopy.bin`. Vous pouvez aussi passer le préfixe complet du compilateur avec `make CROSS=/chemin/vers/bin/sh-elf-`.

## Vérification

Les neuf cartes E1M1 à E1M9 du shareware 1.9 ont été chargées et jouées brièvement dans le cœur LoopyMSE. Le test a aussi couvert le démarrage depuis le menu, la manette, la carte et l’intermission qui mène au niveau suivant. Le test utilise les composants SH‑1 et vidéo émulés avec un démarrage BIOS substitué. Le démarrage et l’entrée dans le premier niveau ont aussi été vérifiés dans le cœur de Rupi 0.3.0. Les essais sur console physique restent à faire.

## Version et sources

Cette version cible la campagne solo shareware. Elle emploie un rendu à détail réduit et adapte la sortie à 256 × 200 pixels. Le son, la musique, les sauvegardes et le multijoueur restent hors de cette version.

Le moteur adapté provient de la publication GPL v2 de [Linux DOOM 1.10 par id Software](https://github.com/id-Software/DOOM). La couche de démarrage, les définitions matérielles et le script de liaison sont issus du kit homebrew Casio Loopy de Kasami et ont été adaptés pour ce port. Les notices sont dans `GPL-2.0.txt`, `loopy/LICENSE.txt` et `runtime/`. Les données de jeu et le BIOS s’obtiennent séparément.
