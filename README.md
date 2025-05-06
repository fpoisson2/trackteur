# Suivi de véhicule DIY pour Traccar

Ce dépôt propose une solution complète de suivi de véhicule en temps réel, basée sur Arduino, GPS et GSM/GPRS, et compatible avec la plateforme Traccar (https://www.traccar.org/).

## Structure du dépôt
- `code/` : code source Arduino
- `pcb/` : fichiers de conception PCB sous KiCad
- `logo.png` : logo du projet

## Description
Le système récupère les données GPS, les enregistre sur une carte SD et les transmet via GSM/GPRS à un serveur Traccar, permettant de suivre la position du véhicule sur une interface web.

## Matériel requis
- Arduino (Pro Mini, Uno, etc.)
- Module GSM/GPRS (SIM800L, SIM900, etc.)
- Module GPS (Ublox NEO-6M ou équivalent)
- Carte SD et lecteur de carte SD
- Antennes GPS et GSM
- Câbles de connexion
- Alimentation (5 V ou batterie LiPo)

## Installation et utilisation
1. Cloner le dépôt :
   ```
   git clone https://github.com/votre-utilisateur/projet-suivi-vehicule.git
   ```
2. Ouvrir le dossier `code/` dans l'IDE Arduino.
3. Configurer les paramètres dans `code.ino` (serveur Traccar, APN, broches, etc.).
4. Connecter le matériel :
   - GPS vers broches RX/TX de l'Arduino
   - GSM vers broches RX/TX de l'Arduino
   - Lecteur SD sur SPI (pins 10–13)
5. Insérer la carte SIM et la carte SD.
6. Alimenter le montage.
7. Lancer votre serveur Traccar et vérifier la réception des positions.

## Personnalisation
- Adapter les constantes (APN, intervalles d'envoi) dans `common.h`.
- Modifier les broches et configurations dans `code.ino` selon votre matériel.

## Contribution
Les contributions sont les bienvenues !  
Forkez le dépôt, créez une branche et ouvrez une pull request.
