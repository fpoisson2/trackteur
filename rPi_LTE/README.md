# README – Installation de Raspberry Pi OS Lite et configuration de base

Ce guide explique, pas à pas, comment :

1. Installer **Raspberry Pi OS Lite** à l’aide de **Raspberry Pi Imager**
2. Activer et configurer la **connexion Wi-Fi** au premier démarrage
3. Installer et configurer **Git**

> **Prérequis** :
> - Un ordinateur (Windows, macOS, Linux) avec accès Internet
> - Une carte microSD (minimum 8 Go)
> - Un lecteur de carte microSD ou adaptateur USB

---

## 1. Préparation de la carte microSD

1. **Téléchargez et installez** Raspberry Pi Imager depuis :
   - Site officiel : https://www.raspberrypi.com/software/

*Fin du guide.*
2. **Insérez** la carte microSD dans votre ordinateur.

3. **Lancez** Raspberry Pi Imager.

4. Cliquez sur **`CHOOSE OS`**, puis sélectionnez :
   - **Raspberry Pi OS (other)** → **Raspberry Pi OS Lite (32-bit)**

5. Cliquez sur **`CHOOSE STORAGE`**, puis choisissez votre carte microSD.

6. (Optionnel) Cliquez sur l’icône ⚙️ **`Advanced options`** (accessible via Ctrl+Shift+X) pour :
   - Activer **SSH** (Tick « Enable SSH »)
   - Définir les identifiants (**username** et **password**) pour SSH
   - Configurer le **Wi-Fi** (SSID, mot de passe, pays)
   - Définir la **timezone** et la **localisation** du clavier

7. Cliquez sur **`WRITE`** et patientez jusqu’au flashage terminé.

8. Éjectez la carte microSD en toute sécurité de votre ordinateur.

---

## 2. Premier démarrage et configuration Wi-Fi

1. **Insérez** la carte microSD dans votre Raspberry Pi et **démarrez**.

2. **Connectez-vous** en SSH (si activé) ou sur le port série / console locale :

   ```bash
   ssh pi@raspberrypi.local
   ```

3. **Mot de passe par défaut** : raspberry (ou celui que vous avez défini)

4. Si vous n’avez pas configuré le Wi‑Fi via l’interface avancée de l’Imager, ouvrez **raspi-config** :

   ```bash
   sudo raspi-config
   ```

5. Dans le menu :
   1. Sélectionnez **`1 System Options`** (ou **`2 Network Options`** selon la version)
   2. Choisissez **`S1 Wireless LAN`** (ou **`N2 Wi-Fi`**)
   3. Entrez le **SSID** de votre réseau et votre **mot de passe**.
   4. Vérifiez que le **pays** est correct (`CA` pour Canada, `FR` pour France, etc.).

6. **Quittez** raspi-config et **rebootez** :

   ```bash
   sudo reboot
   ```

7. Après redémarrage, vérifiez la connexion :

   ```bash
   ip a show wlan0
   ping -c 4 8.8.8.8
   ```

   - `ip a` doit afficher une adresse **inet** 192.168.x.x ou équivalent.
   - `ping` doit renvoyer des réponses.

---

## 3. Installation de Git

1. **Mettez à jour** la liste des paquets :

   ```bash
   sudo apt update && sudo apt upgrade -y
   ```

2. **Installez Git** :

   ```bash
   sudo apt install git -y
   ```

3. **Vérifiez** l’installation :

   ```bash
   git --version
   ```

4. **Configurez** vos informations utilisateur :

   ```bash
   git config --global user.name "Votre Nom"
   git config --global user.email "votre.email@example.com"
   ```

---

🎉 **Félicitations !** Votre Raspberry Pi OS Lite est installé, connecté en Wi‑Fi au démarrage, et prêt à utiliser Git.

