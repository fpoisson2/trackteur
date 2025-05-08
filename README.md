# Suivi de véhicule DIY pour Traccar

Ce dépôt propose une solution complète de suivi de véhicule en temps réel, basée sur Arduino, GPS et GSM/GPRS, et compatible avec la plateforme Traccar (https://www.traccar.org/).

## Structure du dépôt
- `code/` : code source Arduino
- `pcb/` : fichiers de conception PCB sous KiCad
- `logo.png` : logo du projet

## Description
Le système récupère les données GPS, les enregistre sur une carte SD (formaté avec Petit FatFs) et les transmet via un module LTE ou GPRS à un serveur Traccar, permettant de suivre la position du véhicule sur une interface web.

Le projet est compatible avec les modules suivants :
 - SIM7000G (LTE-M / NB-IoT)
 - SIM7070G (LTE-M / NB-IoT)
 - A7670E (LTE Cat-1)

Le code détecte automatiquement le module connecté et ajuste les commandes AT, la gestion du GNSS et les protocoles réseau.

## Matériel requis
- Arduino (Pro Mini, Uno, etc.)
- Module LTE/GPRS : SIM7000G, SIM7070G ou A7670E
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
   - Module LTE/GSM vers broches RX/TX (utilise SoftwareSerial)
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
