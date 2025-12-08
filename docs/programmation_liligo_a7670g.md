# Code GPS Tracker - LilyGo A7670G pour Traccar

Ce dossier contient le code Arduino pour le module LilyGo A7670G afin d'envoyer les données GPS au serveur Traccar.

## 📋 Fonctionnalités

- ✅ Envoi automatique des données GPS toutes les 2 minutes
- ✅ Serveur Traccar: `serveur1d.trackteur.cc:5055`
- ✅ Logique de retry avec 3 tentatives en cas d'échec
- ✅ Sauvegarde automatique sur carte SD si l'envoi échoue
- ✅ Mode veille pour économiser la batterie
- ✅ Support GPS/GLONASS/BeiDou/Galileo
- ✅ Format CSV pour les données sauvegardées

## 🔧 Prérequis

### Matériel
- LilyGo T-A7670G (avec GPS intégré)
- Carte SIM avec données mobiles actives
- Carte microSD (optionnel, pour backup)
- Antenne GPS
- Antenne 4G/LTE

### Logiciel
- Arduino IDE 1.8.x ou 2.x
- Bibliothèque TinyGSM (version fork LilyGo)
- ESP32 Board Support Package

## 📦 Installation

### 1. Installation de l'Arduino IDE
Téléchargez et installez Arduino IDE depuis [arduino.cc](https://www.arduino.cc/en/software)

### 2. Installation du support ESP32
1. Ouvrez Arduino IDE
2. Allez dans `File` > `Preferences`
3. Dans "Additional Board Manager URLs", ajoutez:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Allez dans `Tools` > `Board` > `Boards Manager`
5. Recherchez "ESP32" et installez "esp32 by Espressif Systems"

### 3. Installation de la bibliothèque TinyGSM
⚠️ **IMPORTANT**: Utilisez la version fork de LilyGo, pas la version standard!

1. Téléchargez le dépôt: https://github.com/Xinyuan-LilyGO/LilyGO-T-A76XX
2. Copiez tous les dossiers du répertoire `lib` vers le dossier Arduino libraries:
   - Windows: `C:\Users\[VotreNom]\Documents\Arduino\libraries\`
   - Mac: `~/Documents/Arduino/libraries/`
   - Linux: `~/Arduino/libraries/`

Les bibliothèques nécessaires sont:
- TinyGSM (fork LilyGo)
- StreamDebugger (si DUMP_AT_COMMANDS est activé)

### 4. Configuration du code

Ouvrez le fichier `TraccarGPS.ino` et modifiez les paramètres suivants:

```cpp
// ID unique de votre appareil (OBLIGATOIRE)
const char *client_id = "DEVICE_ID_001";  // Remplacez par un ID unique

// URL du serveur Traccar (déjà configuré)
const char *request_url = "http://serveur1d.trackteur.cc:5055";

// APN de votre opérateur (si nécessaire)
// Décommentez et modifiez selon votre opérateur:
// #define NETWORK_APN "internet"      // Orange/Sosh
// #define NETWORK_APN "free"          // Free Mobile
// #define NETWORK_APN "sl2sfr"        // SFR
// #define NETWORK_APN "mmsbouygtel.com"  // Bouygues
```

### 5. Configuration de la carte dans Arduino IDE

1. Connectez la carte LilyGo A7670G via USB
2. Dans Arduino IDE:
   - `Tools` > `Board` > `ESP32 Arduino` > `ESP32 Dev Module`
   - `Tools` > `Port` > Sélectionnez le port COM correspondant
   - `Tools` > `Upload Speed` > `115200`
   - `Tools` > `Flash Size` > `4MB (32Mb)`
   - `Tools` > `Partition Scheme` > `Default 4MB with spiffs`

### 6. Compilation et téléversement

1. Vérifiez le code: Cliquez sur ✓ (Verify)
2. Téléversez: Cliquez sur → (Upload)
3. Ouvrez le moniteur série: `Tools` > `Serial Monitor`
4. Configurez le baud rate à `115200`

## 📊 Format des données

### Données envoyées à Traccar
Le code envoie les données au format Traccar OsmAnd protocol:
```
http://serveur1d.trackteur.cc:5055/?id=DEVICE_ID&lat=48.8566&lon=2.3522&speed=15.5&altitude=35.2&bearing=270&timestamp=2025-12-08T10:30:00Z&hdop=1.2&batt=100
```

### Données sauvegardées sur SD (CSV)
Format du fichier `gps_data.csv`:
```csv
timestamp,latitude,longitude,speed,altitude,bearing,hdop
2025-12-08 10:30:00,48.8566000,2.3522000,15.50,35.20,270.00,1.20
```

## 🔍 Diagnostic

### LED de statut
- LED allumée: GPS en cours de lecture
- LED éteinte: Mode veille

### Moniteur série
Le moniteur série affiche:
- État du modem (démarrage, enregistrement réseau)
- État de la carte SIM
- Qualité du signal
- Données GPS reçues
- Résultat des envois HTTP
- Sauvegarde sur SD

### Messages d'erreur courants

| Erreur | Solution |
|--------|----------|
| `No SD card attached` | Vérifiez que la carte SD est bien insérée |
| `Network registration denied` | Vérifiez l'APN et que la carte SIM a un forfait data actif |
| `HTTP request failed! Error code: XXX` | Vérifiez la connexion réseau et l'URL du serveur |
| `GPS fix not available` | Placez l'antenne GPS à l'extérieur ou près d'une fenêtre |
| `SIM card locked` | Déverrouillez la carte SIM (retirez le code PIN) |

## 📡 Configuration Traccar

### Ajout de l'appareil sur le serveur

1. Connectez-vous à votre serveur Traccar: `http://serveur1d.trackteur.cc`
2. Allez dans `Settings` > `Devices` > `Add Device`
3. Configurez:
   - **Name**: Nom de votre tracker
   - **Identifier**: L'ID que vous avez mis dans `client_id` (ex: DEVICE_ID_001)
   - **Group**: (optionnel)
4. Cliquez sur `Add`

### Vérification de la réception
1. Dans Traccar, sélectionnez votre appareil
2. Vous devriez voir la position mise à jour toutes les 2 minutes
3. Consultez les logs pour voir les données reçues

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
Décommentez pour voir toutes les commandes AT:
```cpp
#define DUMP_AT_COMMANDS
```

## 📝 Structure du projet

```
code/
├── README.md                 # Ce fichier
└── TraccarGPS/
    ├── TraccarGPS.ino       # Code principal
    └── utilities.h          # Configuration des pins
```

## 🔒 Sécurité

⚠️ **Important**:
- Ne partagez pas votre `client_id` publiquement
- Utilisez un ID unique pour chaque appareil
- Changez les identifiants par défaut

## 🆘 Support

Pour obtenir de l'aide:
1. Vérifiez les messages du moniteur série
2. Consultez la documentation LilyGo: https://github.com/Xinyuan-LilyGO/LilyGO-T-A76XX
3. Documentation Traccar: https://www.traccar.org/osmand/

## 📄 Licence

MIT License - Libre d'utilisation et de modification

## 🔄 Changelog

### Version 1.0 (2025-12-08)
- Envoi initial vers serveur1d.trackteur.cc
- Support carte SD avec sauvegarde CSV
- Logique de retry (3 tentatives)
- Intervalle de 2 minutes
- Mode veille pour économie d'énergie
