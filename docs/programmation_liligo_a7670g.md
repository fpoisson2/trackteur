# Programmation du LilyGo T-A7670G

Guide complet pour programmer le traceur GPS (module LilyGo T-A7670G, microcontrôleur
ESP32) et l'envoi des positions vers le serveur Traccar.

> Procédure vérifiée pas à pas lors du 2<sup>e</sup> bloc de formation (29 juin 2026),
> depuis un poste Windows.

## 📋 Fonctionnalités

- ✅ Envoi automatique des positions GPS (intervalle configurable, 2 minutes par défaut)
- ✅ Envoi vers le serveur Traccar (protocole OsmAnd)
- ✅ Logique de retry avec 3 tentatives en cas d'échec
- ✅ Sauvegarde automatique sur carte SD si l'envoi échoue
- ✅ Mode veille pour économiser la batterie
- ✅ Support GPS/GLONASS/BeiDou/Galileo
- ✅ Format CSV pour les données sauvegardées

## 🔧 Prérequis

### Matériel

- LilyGo T-A7670G (avec GPS intégré) — le traceur assemblé au bloc de formation précédent
- Carte SIM Hologram **activée** (voir [Création de carte SIM Hologram](creation_carte_sim_hologram.md))
- Carte microSD (optionnel, pour le backup)
- Antenne GPS et antenne 4G/LTE branchées
- **Un câble USB-A → USB-C**

> ⚠️ **Le type de câble USB est déterminant**
>
> Le LilyGo T-A7670G **ne s'alimente pas** avec un câble **USB-C → USB-C** : aucune
> LED ne s'allume et la carte n'apparaît pas dans la liste des ports de l'Arduino IDE.
> Il faut absolument un câble **USB-A (grosse prise rectangulaire) → USB-C**,
> branché sur un port USB-A de l'ordinateur. Un câble de ce type est fourni dans
> la boîte du traceur.
>
> Si la prise USB-C du boîtier est difficile d'accès, appuyer sur les supports
> blancs pour sortir délicatement la carte LilyGo du boîtier le temps de la
> programmation.

> 📌 **La batterie n'est pas nécessaire**
>
> Pour la programmation, l'alimentation par le port USB suffit. Inutile d'installer
> les piles 18650 ni de brancher le câble d'alimentation 12 V du véhicule.

### Logiciel

- Arduino IDE 1.8.x ou 2.x
- Support de carte ESP32 (Espressif)
- Bibliothèques du dépôt LilyGO-T-A76XX (fork TinyGSM de LilyGo)

## 📦 Installation de l'environnement

### 1. Installation de l'Arduino IDE

Téléchargez et installez Arduino IDE depuis [arduino.cc](https://www.arduino.cc/en/software).
Au démarrage, les invitations à mettre à jour peuvent être ignorées.

### 2. Installation du support ESP32

L'ESP32 est le microcontrôleur monté sur la carte LilyGo (le petit module argenté).
Il remplace l'Arduino Nano utilisé lors des formations précédentes; il est plus
performant et le module cellulaire est intégré directement sur la même carte.

1. Ouvrir Arduino IDE
2. Menu **Outils** (*Tools*) → **Carte** (*Board*) → **Gestionnaire de carte** (*Boards Manager*)
3. Dans la barre de recherche du panneau de gauche, saisir `ESP32`
4. Dans la liste, choisir **esp32 by Espressif Systems** (2<sup>e</sup> résultat) → **Installer**

> ⚠️ **Prévoir du temps**
>
> L'installation du support ESP32 est volumineuse et peut prendre **30 à 60 minutes**
> selon la connexion. La mention *traitement en cours* reste affichée en bas à droite
> de la fenêtre pendant tout ce temps. Ne pas fermer l'Arduino IDE et ne pas relancer
> l'installation. **Il est fortement recommandé de faire cette étape à l'avance**,
> avant une séance de formation ou une installation sur le terrain.

### 3. Installation des bibliothèques LilyGo

> ⚠️ **IMPORTANT**: utiliser la version fork de LilyGo, pas la version standard de TinyGSM.

1. Ouvrir [https://github.com/Xinyuan-LilyGO/LilyGO-T-A76XX](https://github.com/Xinyuan-LilyGO/LilyGO-T-A76XX)
2. Cliquer sur le bouton vert **Code** → **Download ZIP** (≈ 128 Mo, prévoir quelques minutes)
3. Dans l'explorateur Windows, ouvrir le dossier **Téléchargements**, clic droit sur
   l'archive → **Extraire tout**
4. Entrer dans le dossier extrait, puis dans le sous-dossier **`lib`**
5. Sélectionner tout son contenu (`Ctrl+A`) et copier (`Ctrl+C`)
6. Coller (`Ctrl+V`) dans le dossier des bibliothèques Arduino:
   - Windows: `C:\Users\[VotreNom]\Documents\Arduino\libraries\`
   - Mac: `~/Documents/Arduino/libraries/`
   - Linux: `~/Arduino/libraries/`

> ⚠️ **Windows + OneDrive: deux dossiers « Documents »**
>
> Lorsque OneDrive est activé, il existe souvent **deux** dossiers Documents:
> `C:\Users\[Nom]\Documents\` et `C:\Users\[Nom]\OneDrive\Documents\`.
> L'Arduino IDE peut utiliser l'un ou l'autre selon la configuration.
> En cas d'erreur de compilation du type *bibliothèque introuvable*,
> **copier les bibliothèques dans les deux emplacements** — c'est sans risque.
>
> Le chemin réellement utilisé est visible dans Arduino IDE:
> **Fichier** → **Préférences** → *Emplacement du croquis*.

Les bibliothèques nécessaires (toutes présentes dans `lib`) sont notamment:

- TinyGSM (fork LilyGo)
- StreamDebugger (si `DUMP_AT_COMMANDS` est activé)

### 4. Ouverture de l'exemple Traccar

1. Dans Arduino IDE: **Fichier** → **Ouvrir**
2. Naviguer jusqu'au dossier extrait dans **Téléchargements**, puis
   `examples` → `Traccar` → ouvrir le fichier `.ino`

> ⚠️ **Ouvrir le croquis depuis le dossier extrait**
>
> Si le croquis est ouvert depuis une copie placée ailleurs, la compilation échoue
> avec des erreurs de bibliothèques manquantes (ex. `SoftwareSerial`). Ouvrir bien
> le fichier situé dans le dossier extrait du dépôt LilyGo. Le dossier peut être
> déplacé plus tard, une fois la configuration validée.

## ⚙️ Configuration du code

### a) Fichier `utilities.h` — choix de la carte

Ouvrir l'onglet **`utilities.h`** en haut de la fenêtre.

1. Aller à la **ligne 22** (ou la ligne correspondant à la carte **T-A7670G**)
2. **Retirer les deux barres obliques `//`** en début de ligne pour décommenter
   la définition de la carte
3. **Remettre `//`** devant la ligne de la carte qui était active auparavant
4. Vérifier qu'il **ne reste aucun espace avant le `#`** — le `#define` doit
   commencer en toute première colonne, sinon la compilation échoue

```cpp
// Définir le board utilisé (décommenter celui que vous utilisez)
#define LILYGO_T_A7670G
```

### b) Fichier principal `.ino` — paramètres du projet

Toujours dans le même croquis, onglet du fichier `.ino`:

| Ligne (indicative) | Paramètre | Valeur à mettre |
|--------------------|-----------|-----------------|
| 24 | `REPORT_LOCATION_RATE_SECOND` | `120` (2 minutes). L'exemple est réglé à 20 s, ce qui consomme trop de données |
| 43 | `NETWORK_APN` | Décommenter la ligne et remplacer `ctlte` par `hologram` (**en minuscules**) |
| 55 | `client_id` (Traccar device ID) | L'identifiant de l'appareil, ex. `tracteur_pro_1` (**en minuscules**, sans espace ni accent) |
| 56 | `request_url` | L'URL du serveur, ex. `https://serveur1e.trackteur.cc` |

```cpp
#define REPORT_LOCATION_RATE_SECOND   120          // intervalle d'envoi
#define NETWORK_APN                   "hologram"   // APN Hologram

const char *client_id   = "tracteur_pro_1";                 // identique à Traccar
const char *request_url = "https://serveur1e.trackteur.cc"; // serveur Traccar
```

> 📌 **L'identifiant doit être identique partout**
>
> Le `client_id` du code, l'**Identifiant** de l'appareil dans Traccar et le **nom
> du dispositif** dans Hologram doivent correspondre. Écrire aussi ce nom sur le
> boîtier du traceur. Sans cette correspondance, les positions arrivent au serveur
> mais ne sont associées à aucun appareil.

Sauvegarder (`Ctrl+S`).

## 🔌 Téléversement

1. Brancher le traceur avec le **câble USB-A → USB-C**
2. **Outils** → **Carte** → **ESP32 Arduino** → **ESP32 Dev Module**
3. **Outils** → **Port** → sélectionner le port COM apparu au branchement (ex. `COM3`)
4. Cliquer sur la flèche **→** (Téléverser) au centre de la barre d'outils
5. Agrandir la zone du bas (**Sortie** / **Moniteur série**) pour suivre la compilation

Si aucune erreur n'apparaît et que le message de fin de téléversement s'affiche,
le firmware est en place.

Réglages complémentaires (généralement corrects par défaut):

- **Upload Speed**: `115200`
- **Flash Size**: `4MB (32Mb)`
- **Partition Scheme**: `Default 4MB with spiffs`

## 🖥️ Vérification au moniteur série

1. **Outils** → **Moniteur série**
2. En bas à droite du moniteur, changer la vitesse de `9600` à **`115200`**

Séquence attendue:

```
Signal quality: 99
Network registered!
Connecting to network... OK
IP Address: 10.x.x.x
GPS enabled
GPS fix acquired
HTTP POST ... 200
```

> 💡 **Le premier fix GPS peut être long**
>
> À la première mise sous tension, ou après un long arrêt, le module peut mettre
> **plusieurs minutes** à obtenir un fix GPS. Placer le traceur **près d'une fenêtre
> ou à l'extérieur**, antenne GPS dégagée, et attendre. Tant que le fix n'est pas
> obtenu, aucune position n'est envoyée: ce n'est pas une panne, c'est un délai.
> Débrancher/rebrancher le câble USB permet de repartir d'un état propre.

## 📊 Format des données

### Données envoyées à Traccar

Format OsmAnd:

```
https://serveur1e.trackteur.cc/?id=tracteur_pro_1&lat=48.8566&lon=2.3522&speed=15.5&altitude=35.2&bearing=270&timestamp=2026-06-29T10:30:00Z&hdop=1.2&batt=100
```

### Données sauvegardées sur SD (CSV)

Fichier `gps_data.csv`:

```csv
timestamp,latitude,longitude,speed,altitude,bearing,hdop
2026-06-29 10:30:00,48.8566000,2.3522000,15.50,35.20,270.00,1.20
```

> 📌 **Enregistrement sur carte SD**
>
> L'exemple `Traccar` fourni par LilyGo **n'inclut pas** l'enregistrement des
> positions sur la carte SD. Cette fonction est ajoutée dans le firmware du projet
> (`code/TraccarGPS/`), à téléverser en remplacement une fois l'exemple validé.
> La procédure de téléversement est identique.

## 📡 Ajout de l'appareil dans Traccar

À faire **avant** ou pendant la programmation, voir la page dédiée:
[Ajout d'un appareil dans Traccar](ajout_appareil_traccar.md).

En résumé:

1. Se connecter au serveur Traccar
2. **Paramètres** → **Appareils** → **+**
3. **Nom**: libellé lisible (ex. `Tracteur pro 1`)
4. **Identifiant**: exactement le `client_id` du code, en minuscules (ex. `tracteur_pro_1`)
5. **Enregistrer**

## 🔍 Diagnostic

### Messages d'erreur courants

| Erreur / symptôme | Solution |
|--------------------|----------|
| Aucune LED, aucun port COM au branchement | Utiliser un câble **USB-A → USB-C** (les câbles USB-C → USB-C ne fonctionnent pas) |
| `SoftwareSerial.h: No such file or directory` ou autre bibliothèque introuvable | Croquis ouvert hors du dossier extrait, ou bibliothèques copiées dans le mauvais dossier `Documents` (voir OneDrive ci-dessus) |
| `#define` non reconnu dans `utilities.h` | Un espace subsiste avant le `#` en début de ligne |
| `No SD card attached` | Vérifier que la carte SD est bien insérée |
| `Network registration denied` | Vérifier l'APN `hologram`, l'activation de la SIM et le **solde du compte Hologram** |
| `HTTP request failed! Error code: XXX` | Vérifier l'URL du serveur et la connexion réseau |
| `GPS fix not available` | Antenne GPS à l'extérieur ou près d'une fenêtre; patienter plusieurs minutes |
| `SIM card locked` | Retirer le code PIN de la carte SIM |
| Aucune position dans Traccar alors que le moniteur série envoie | L'**Identifiant** de l'appareil dans Traccar ne correspond pas au `client_id` |

### Moniteur série

Le moniteur série (115200 baud) affiche:

- État du modem (démarrage, enregistrement réseau)
- État de la carte SIM et qualité du signal
- Obtention de l'adresse IP
- Activation du GPS et données reçues
- Résultat des envois HTTP et sauvegarde sur SD

## ⚡ Consommation d'énergie

- **Lecture GPS + Envoi**: ~350-500 mA
- **Mode veille**: ~2-3 mA
- **Cycle complet (2 min)**: Moyenne ~5-10 mA

💡 Pour une batterie de 3000 mAh, autonomie estimée: 12-25 jours

## 🛠️ Personnalisation

### Modifier l'intervalle d'envoi

```cpp
#define REPORT_LOCATION_RATE_SECOND     120  // 120 = 2 minutes
```

### Modifier le nombre de retry

```cpp
#define MAX_RETRY_ATTEMPTS              3    // Nombre de tentatives
```

### Désactiver la sauvegarde SD

```cpp
#define USE_SD_CARD                     false
```

### Activer le debug AT

Décommenter pour voir toutes les commandes AT échangées avec le modem:

```cpp
#define DUMP_AT_COMMANDS
```

Utile pour diagnostiquer un problème réseau; à **recommenter** ensuite (`Ctrl+Z`
juste après le test), car cette option rend le moniteur série très verbeux.

## 📝 Structure du projet

```
code/
├── README.md                 # Notes de développement
└── TraccarGPS/
    ├── TraccarGPS.ino       # Code principal
    ├── config.h             # Paramètres du projet (ID, serveur, APN)
    └── utilities.h          # Configuration des pins
```

## 🔒 Sécurité

⚠️ **Important**:

- Ne pas partager le `client_id` publiquement
- Utiliser un identifiant unique pour chaque appareil
- Changer les identifiants par défaut

## 🆘 Support

1. Vérifier les messages du moniteur série
2. Documentation LilyGo: [https://github.com/Xinyuan-LilyGO/LilyGO-T-A76XX](https://github.com/Xinyuan-LilyGO/LilyGO-T-A76XX)
3. Documentation Traccar: [https://www.traccar.org/osmand/](https://www.traccar.org/osmand/)

## 📄 Licence

MIT License - Libre d'utilisation et de modification

## 🔄 Changelog

### Version 1.1 (2026-06-29)

- Procédure détaillée validée en formation (activation SIM, IDE, exemple Traccar)
- Ajout des exigences de câblage USB-A → USB-C
- Ajout des lignes précises à modifier dans `utilities.h` et le `.ino`
- Ajout des pièges Windows/OneDrive sur le dossier `libraries`
- Rappel: l'exemple LilyGo n'inclut pas l'enregistrement sur carte SD

### Version 1.0 (2025-12-08)

- Envoi initial vers le serveur Traccar
- Support carte SD avec sauvegarde CSV
- Logique de retry (3 tentatives)
- Intervalle de 2 minutes
- Mode veille pour économie d'énergie
