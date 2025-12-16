# Trackteur

![Logo](logo.png)

Trackteur est un projet de traceur GPS DIY basé sur le module **LilyGo T-A7670G**. Il permet de suivre des véhicules ou équipements en temps réel via une infrastructure serveur Traccar.

## Caractéristiques

- **Module**: LilyGo T-A7670G (ESP32 + modem 4G/LTE + GPS)
- **Connectivité**: 4G LTE via carte SIM Hologram
- **Protocole**: OsmAnd (compatible Traccar)
- **Intervalle d'envoi**: Toutes les 2 minutes (configurable)
- **Backup**: Sauvegarde automatique sur carte SD en cas d'échec réseau
- **Mode veille**: Économie d'énergie entre les envois
- **Multi-constellation GPS**: GPS, GLONASS, BeiDou, Galileo

## Architecture

```
┌─────────────────┐     ┌─────────────────────┐     ┌─────────────────┐
│   Traceur GPS   │────▶│  Cloudflare Worker  │────▶│ Serveurs Traccar│
│   (LilyGo)      │     │   (Load Balancer)   │     │   (Docker)      │
└─────────────────┘     └─────────────────────┘     └─────────────────┘
```

Le système utilise Cloudflare comme point d'entrée unique et répartiteur de charge vers plusieurs serveurs Traccar pour assurer une haute disponibilité.

## Structure du projet

```
trackteur/
├── code/
│   └── TraccarGPS/
│       ├── TraccarGPS.ino     # Code principal Arduino
│       ├── config.h           # Configuration (serveur, APN, intervalles)
│       └── utilities.h        # Définitions des pins
├── docs/
│   ├── index.md                       # Vue d'ensemble
│   ├── fabrication_traceur_gps.md     # Guide d'assemblage
│   ├── programmation_liligo_a7670g.md # Guide de programmation
│   ├── installation_docker.md         # Déploiement serveur
│   ├── configuration_cloudflare.md    # Configuration CDN
│   ├── creation_carte_sim_hologram.md # Activation SIM
│   └── installation_vehicule.md       # Installation physique
├── docker-compose.yml         # Configuration Docker serveur
└── mkdocs.yml                 # Configuration documentation
```

## Prérequis

### Matériel
- LilyGo T-A7670G
- Antenne GPS active
- Antenne 4G/LTE
- Carte SIM avec forfait data (Hologram recommandé)
- Carte microSD (optionnel, pour backup)
- Alimentation 5V (USB ou convertisseur 12V-5V pour véhicule)

### Logiciel
- Arduino IDE 1.8.x ou 2.x
- ESP32 Board Support Package
- Bibliothèques TinyGSM (fork LilyGo)

## Installation rapide

### 1. Cloner le dépôt
```bash
git clone https://github.com/votre-repo/trackteur.git
cd trackteur
```

### 2. Configurer le firmware
Éditez `code/TraccarGPS/config.h`:
```cpp
#define TRACCAR_DEVICE_ID    "VOTRE_ID_UNIQUE"
#define TRACCAR_SERVER_URL   "https://votre-serveur.com"
#define NETWORK_APN          "hologram"
```

### 3. Téléverser le code
1. Ouvrez `code/TraccarGPS/TraccarGPS.ino` dans Arduino IDE
2. Sélectionnez la carte "ESP32 Dev Module"
3. Téléversez le code

### 4. Déployer le serveur (optionnel)
```bash
cp docker-compose.example.yml docker-compose.yml
# Éditez docker-compose.yml avec vos paramètres
docker-compose up -d
```

## Documentation

La documentation complète est disponible via MkDocs:

```bash
pip install mkdocs mkdocs-mermaid2-plugin
mkdocs serve
```

Puis ouvrez http://localhost:8000

### Guides disponibles
- [Fabrication du traceur GPS](docs/fabrication_traceur_gps.md)
- [Programmation du LilyGo A7670G](docs/programmation_liligo_a7670g.md)
- [Déploiement des serveurs Traccar](docs/installation_docker.md)
- [Configuration de Cloudflare](docs/configuration_cloudflare.md)
- [Création de carte SIM Hologram](docs/creation_carte_sim_hologram.md)
- [Installation dans un véhicule](docs/installation_vehicule.md)

## Consommation d'énergie

| Mode | Consommation |
|------|--------------|
| Lecture GPS + Envoi | 350-500 mA |
| Mode veille | 2-3 mA |
| Cycle moyen (2 min) | 5-10 mA |

Avec une batterie de 3000 mAh, l'autonomie estimée est de 12-25 jours.

## Format des données

### Envoi vers Traccar (OsmAnd Protocol)
```
https://serveur.com/?id=DEVICE_ID&lat=48.8566&lon=2.3522&speed=15.5&altitude=35.2&bearing=270&timestamp=2025-12-08T10:30:00Z&hdop=1.2&batt=100
```

### Backup SD (CSV)
```csv
timestamp,latitude,longitude,speed,altitude,bearing,hdop
2025-12-08 10:30:00,48.8566000,2.3522000,15.50,35.20,270.00,1.20
```

## Contribuer

Les contributions sont les bienvenues! N'hésitez pas à ouvrir une issue ou une pull request.

## Licence

MIT License - Voir le fichier LICENSE pour plus de détails.

## Remerciements

- [LilyGo](https://github.com/Xinyuan-LilyGO) pour le module T-A7670G et les bibliothèques
- [Traccar](https://www.traccar.org/) pour la plateforme de tracking
- [TinyGSM](https://github.com/vshymanskyy/TinyGSM) pour la bibliothèque GSM/LTE
