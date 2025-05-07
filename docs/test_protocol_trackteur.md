# Protocole de Validation Exhaustif — Projet **Trackteur**

**Version :** 1.0   **Révision du firmware testée :** `$(git rev-parse --short HEAD)`

*NB : ce protocole s’appuie sur l’implémentation exacte observée dans le dépôt (fichiers `code/*`). Les libellés, messages **Serial** et noms de fonctions cités proviennent du code source et permettent une traçabilité directe entre les tests et l’implémentation.*

---

## Table des matières
1. Objectifs et portée
2. Prérequis
   2.1 Matériel 2.2 Logiciel 2.3 Préparation du banc
3. Convention de nommage & journalisation
4. Plan de test détaillé (pas-à-pas)
   * 4.1 Initialisation « cold-boot »
   * 4.2 Pile réseau LTE & détection SIM
   * 4.3 GNSS : acquisition & parsing (`getGpsData`)
   * 4.4 SD : moteur Petit FatFs (`sdlog.cpp`)
   * 4.5 Flux Traccar (`sendGpsToTraccar`)
   * 4.6 Watch-dog & résilience (`serviceNetwork`, `resetGsmModule`)
   * 4.7 Tests de bordure & corruption (fichiers, buffers, wrap-around)
5. Critères de succès/échec
6. Collecte des preuves & archivage
7. Annexes

---

## 1. Objectifs et portée

Valider que **Trackteur** – matériel, firmware et logique back-end – fonctionne dans toutes les conditions prévues par le code (fichiers `code/*.cpp`).

• Fiabilité du boot, absence de sur-consommation (< 200 mA @ 5 V lors du pic d’initialisation observé dans `setup()`).  
• Bon déroulement de la séquence **`serviceNetwork()`** : `initialCommunication()` ► `step1…4` ► état `NetState::ONLINE`.  
• Performance des fonctions critiques : `getGpsData`, `openDataStack`, `tcpSend`, `logRealPositionToSd`, etc.  
• Robustesse face aux fautes : perte réseau, corruption secteur, overflow de buffers (**`RESPONSE_BUFFER_SIZE` = 128 octets**), redémarrage forcé (watch-dog).  
• Conformité du format de trame HTTP GET attendu par Traccar.

## 2. Prérequis

### 2.1 Matériel

| Réf | Qté | Détails |
|---|---|---|
| Carte Trackteur assemblée | 2 | Une avec **SIM7000G**, une avec **A7670E** (détection via `ATI`) |
| Alim de labo 5 V / 2 A | 1 | Lim courant ajustable |
| Batterie Li-ion 18650 2400 mAh | 1 | Test coupure secteur |
| Câble USB-TTL 115 200 bps mini | 1 | Console debug (pins RX0/TX0) |
| Antennes LTE & GNSS | 1+1 | Connecteurs U.FL ≈50 Ω |
| SIM M2M **data** | 1 | APN : *onomondo* (cf. `const char* APN`) |
| micro-SD 4 Go class 10 | 1 | Format FAT32, sector size 512 o |
| PC Windows/Linux | 1 | Ports USB, Python ≥ 3.8 |
| Multimètre + oscillo ≥ 50 MHz | - | Vér. VCC, bruit, toggles GPIO |
| Cage Faraday (boite métal) | - | Simuler perte réseau |

### 2.2 Logiciel

• Toolchain Arduino / PlatformIO (AVR-gcc 5+).  
• Terminal série : `minicom`, `PuTTY` ou équivalent **avec log cru**.  
• Python 3 + dépendances (`pyserial`, `pandas`) : scripts `tools/trackteur_harness.py` (fourni).  
• Serveur **Traccar** (v5+) local : accès au log `tracker-server.log`.  
• Optionnel : simulateur NMEA (plages de tests GNSS en intérieur).

### 2.3 Préparation du banc

1. Flasher la révision *release-candidate* (`git checkout <tag>`).  
2. Sauvegarder le SHA-1 dans la feuille de résultat.  
3. Vérifier soudures, polarités et **continuité D2** (GPIO `powerPin`).  
4. Insérer la micro-SD puis exécuter `initializeSD()` via un sketch simple : assure un montage OK avant campagne.  
5. Insérer la carte SIM, antennes GNSS & LTE (vérifier pas d’inversion).  
6. Relier la console série (115 200 bps). Activer *capture RAW*.

---

## 3. Convention de nommage & journalisation

• Un ID unique `T-<Domaine>-NNN` par cas (ex :`T-GSM-012`).  
• Tous les logs **Serial** doivent être conservés : `/logs/YYYY-MM-DD/<ID>.log`.  
• Scripts Python : chaque test écrit un CSV `/results/YYYY-MM-DD/<ID>.csv` avec **timestamp ISO8601**, `result`, `duration_ms`, et commentaires.  
• Photos/Vidéos : nom `IMG_<ID>_stepX.jpg`.

---

## 4. Plan de test détaillé

### Lecture des colonnes

* Pré-cond. = état et configuration à respecter avant déroulé.  
* Étapes = séquence manuelle/automatisée.  
* Attendu = critère de succès issu **des messages ou valeurs retournées**.  
* Timeout/itération = valeur max avant échec.

### 4.1 Cold-boot & Initialisation

| ID | Objectif | Pré-cond. | Étapes | Attendu / Timeout |
|----|----------|-----------|---------|-------------------|
| T-BOOT-001 | Absence de sur-conso | Alim labo 5 V ; I_lim = 500 mA | Alimenter. | I_crête < 200 mA (lecture multimètre ‹ 5 s). |
| T-BOOT-002 | Bannière USB visible | UART ouvert 115 200 bps | Presse **RESET** MCU | Log « `--- Arduino Initialized ---` » < 1 s. |
| T-BOOT-003 | Setup terminé | | Observer console | « `=== SETUP TERMINÉ ===` » < 10 s. |
| T-BOOT-004 | Watchdog actif | Modifier `initializeWatchdog()` pour activer `wdt_enable(WDTO_8S)` et re-flasher | Laisser tourner 30 min | Aucun reset impromptu. |

### 4.2 Pile réseau & SIM

| ID | Objectif | Pré-cond. | Étapes | Attendu |
|----|----------|-----------|---------|---------|
| T-GSM-010 | Détection modem | Démarrage normal | Vérifier log `>> Modem detected: ...` | Modèle correct (`SIM7000G` ou `A7670E`). |
| T-GSM-020 | `initialCommunication()` | | Surveiller log | Suite d’`OK` pour `ATE0`, `AT+CMEE=2` etc. |
| T-GSM-030 | APN configuré | | Chercher « `AT+CGDCONT=1,"IP","onomondo"` » | APN = constante `APN`. |
| T-GSM-040 | SIM Ready | | Confirmer `SIM Ready.` | Reçu < 15 s. |
| T-GSM-050 | Enregistré réseau | | `Registered.` dans log | ‹ 60 s sinon **E**. |
| T-GSM-060 | `openDataStack()` | | LOG « `✔ TCP connection successful` » ou `NETOPEN OK` | 3 tentatives max. |
| T-GSM-070 | Reconnexion OFFLINE→ONLINE | Forcer réseau OFF (boite Faraday 2 min) | Observer bascule `NetState::OFFLINE` puis reconnection | Reprise < RECONNECT_PERIOD (60 s). |

### 4.3 GNSS (`gps.cpp`)

| ID | Objectif | Étapes | Succès |
|----|----------|--------|---------|
| T-GPS-100 | `getGpsData` valid – SIM7000 | Champ libre ciel dégagé | Temps fix (`run=1, fix=1`) < 45 s. |
| T-GPS-110 | `getGpsData` valid – A7670 | `AT+CGPSINFO` | Champs non-vides, TS parsé. |
| T-GPS-120 | Format timestamp | Inspecter buffer `gpsTimestampTraccar` | Format `YYYY-MM-DD%20hh:mm:ss`. |
| T-GPS-130 | Précision position | Comparer à coord. réf. (DGPS) | Δ < ±15 m. |
| T-GPS-140 | Robustesse buffer | Envoyer réponse +CGNSINF de 200 car. (script) | Pas d’overflow (taille définie 128). |

### 4.4 SD & journalisation sectorielle

| ID | Objectif | Étapes | Attendu |
|----|----------|--------|---------|
| T-SD-200 | Montage FS | Power-up | « `Carte SD montée avec succès.` » |
| T-SD-210 | Écriture secteur | Forcer OFFLINE, laisser 5 positions | Secteur écrit, préfixe `!`, `sectorIndex` incrémente. |
| T-SD-220 | Marquage # envoyé | Reconnecter réseau puis appeler `resendLastLog()` | Secteur précédant passe `#`. |
| T-SD-230 | Corruption metadata | Éditer « GPS_LOG.CSV » : altérer signature | Au boot : log « `Signature invalide` » et `saveLogMetadata(1)`. |
| T-SD-240 | Overflow MAX_SECTORS | Simuler `sectorIndex=MAX_SECTORS+1` via debug | Index remis à 1. |

### 4.5 Transmission Traccar

| ID | Objectif | Étapes | Succès |
|----|----------|--------|---------|
| T-NET-300 | Connexion TCP | `tcpOpen()` | « `✔ TCP connection successful` » sous 30 s. |
| T-NET-310 | Construction HTTP | Inspecter `httpReq` | Chaîne = `GET /?id=<DEVICE_ID>&lat=<lat>&lon=<lon>&timestamp=<ts>` + CRLFCRLF |
| T-NET-320 | `tcpSend` prompt | Vérifier réception char `>` | < 3 s. |
| T-NET-330 | `SEND OK` | Dans 10 s après payload | OK. |
| T-NET-340 | Fermeture propre | `tcpClose` → `CLOSE OK` ou `+CIPCLOSE:` | Fermé < 10 s. |
| T-NET-350 | Charge offline 200 trames | Forcer stock SD, rétablir réseau | 200 requêtes reçues dans **ordre ASC** (vér. log Traccar). |

### 4.6 Watch-dog & recovery

| ID | Objectif | Étapes | Succès |
|----|----------|--------|---------|
| T-WDG-400 | `wdt_reset` appelé | Brancher oscillo sur pin 13, placer un `digitalWrite(13, !digitalRead(13));` dans `wdt_reset()` | Toggle périodique < 120 ms. |
| T-WDG-410 | Boucle bloquée | Injecter `while(1);` via serial debug (commande `DBG_LOCK`) | Reboot < 8 s. |
| T-WDG-420 | `resetGsmModule()` | Débrancher antenne LTE 5 min | Log « `*** Power-cycling GSM module ***` ». |
| T-WDG-430 | Compteur `consecutiveNetFails` | Simuler 5 échecs  réseau | Reset module puis compteur = 0. |

### 4.7 Bords, limites et injection d’erreurs

| ID | Scénario | Méthode | Attendu |
|----|----------|---------|---------|
| T-ERR-500 | Buffer série plein | Script Python envoie 256 chars rapidement | Log `⚠️ overflow, flushing` puis fonctionnement normal. |
| T-ERR-510 | Wrap-around `sectorIndex` | Positionner `sectorIndex=MAX_SECTORS` +1 | Au log suivant, index=1, aucun crash. |
| T-ERR-520 | TCP refusé | Noter déconnexion serveur | `tcpOpen()` → échec ; firmware stocke data SD, reste ONLINE=false. |

---

## 5. Critères de succès / échec

• **Succès** : tous les IDs marqués « S » dans la feuille de résultats, aucune alarme critique (Serial « FATAL » ou « ERROR ») non résolue.  
• **Échec bloquant** : un seul KO dans les sections 4.1-4.5 impose correctif avant production.  
• **Échec non-bloquant** : cas de stress (4.7) peuvent ouvrir une **issue GitHub** sans stopper la série.

## 6. Collecte des preuves & archivage

1. Script `tools/collect_logs.py` : range `.log` et `.csv` automatiquement.  
2. Compression zip + checksum SHA-256.  
3. Push dans dépôt interne `validation/trackteur_tests_<date>.zip`.  
4. Rédiger ticket JIRA si KO.

## 7. Annexes

### A. Modèle de feuille de résultats

| ID | Date/Heure | Résultat (S/E) | Observateur | Notes |
|----|------------|---------------|-------------|-------|

### B. Commandes AT fréquentes (extrait)

```
ATI
AT+CPIN?
AT+CREG?
AT+CGATT?
AT+CGNSINF      # SIM7000
AT+CGPSINFO     # A7670
AT+CIPSTART="TCP","trackteur.ve2fpd.com",5055
AT+CIPSEND=<len>
```

### C. Points de mesure — PCB v1

| Repère | Désignation | Commentaire |
|--------|-------------|-------------|
| TP1 | VCC_5V | 5 V régulateur LM2596 |
| TP2 | V_BAT | Li-ion après protection | 
| TP3 | D2 (PWRKEY) | Doit commuter 0 → 1 pour ON module |

---

**Fin de document – Protocole de test Trackteur**
