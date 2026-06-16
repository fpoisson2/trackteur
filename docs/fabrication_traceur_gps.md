# Fabrication du traceur GPS Trackteur

Ce guide présente l’assemblage mécanique et le câblage de base du traceur GPS **Trackteur**.

## 1. Matériel requis

Référez-vous au [BOM — Bill of Materials](BOM.md) pour la liste complète des composants avec images.

<img width="2985" height="1440" alt="image" src="https://github.com/user-attachments/assets/67494daa-26a2-4684-aba2-0f6935360b2e" />

## 2. Vue d’ensemble du système

Le traceur est installé dans un boîtier étanche IP67. Il est alimenté par le véhicule via le fil ACC 12 V et la masse. Un convertisseur abaisse la tension de 12 V vers 5 V pour alimenter le module LilyGo A7670G.

Le connecteur externe permet de débrancher rapidement le traceur sans ouvrir le boîtier.

## 3. Schéma de câblage

```mermaid
flowchart LR
    subgraph vehicule["Véhicule"]
        ACC[Fil ACC 12 V]
        GND[Masse]
    end

    subgraph jonction["Jonction 3 pattes"]
        J[Connexion ACC / GND]
    end

    subgraph connecteur["Connecteur 2 pattes<br/>(extérieur du boîtier)"]
        C[Déconnexion rapide]
    end

    subgraph boitier["Boîtier étanche IP67"]
        PE[Presse-étoupe]
        CONV[Convertisseur<br/>12 V → 5 V]
        LILYGO[LilyGo A7670G]
        BAT[Batteries 18650<br/>backup]
        SD[Carte SD]
        ANT1[Antenne GPS]
        ANT2[Antenne LTE / 4G]
    end

    ACC --> J
    GND --> J
    J -->|Câble weatherproof| C
    C -->|Câble weatherproof| PE
    PE --> CONV
    CONV -->|USB 5 V| LILYGO
    BAT -.->|Backup| LILYGO
    SD -.-> LILYGO
    ANT1 -.-> LILYGO
    ANT2 -.-> LILYGO
```

### Légende

* **Trait plein** : connexions d’alimentation
* **Trait pointillé** : connexions internes au boîtier

## 4. Préparation du boîtier

### 4.1 Percer le trou pour le presse-étoupe

Percer un trou dans le boîtier pour installer le presse-étoupe.

Diamètre recommandé :

* **12,5 mm**
* ou **1/2 po**

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/42c9418b-834a-42d7-9447-322003ebc5f7" />

### 4.2 Installer le presse-étoupe

Installer le presse-étoupe dans le trou, puis le serrer afin d’assurer l’étanchéité.

<img width="303" height="273" alt="image" src="https://github.com/user-attachments/assets/bd462352-a265-4979-a9f1-f41eb759ed26" />

## 5. Préparation du module LilyGo A7670G

### 5.1 Installer les pattes autocollantes

Poser les pattes autocollantes dans les trous du LilyGo A7670G, puis retirer la pellicule verte.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/e3b76e1d-88d5-4bd8-a8e9-e0f1917a29a9" />

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/b3091e58-6234-4b5c-a009-7035295e25de" />

### 5.2 Brancher l’antenne LTE

Brancher l’antenne LTE autocollante sur le module LilyGo.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/6359e923-b2cf-4724-8230-4ffdbd461d55" />

### 5.3 Brancher l’antenne GPS

Brancher l’antenne GPS sur le module LilyGo.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/267f575e-2e2a-441c-b963-96dfe97d88e7" />

### 5.4 Brancher le câble d’alimentation

Brancher le câble d’alimentation au module LilyGo.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/6b677180-23c1-4251-b589-66eb5b796cce" />

## 6. Installation des composants dans le boîtier

### 6.1 Coller le LilyGo dans le boîtier

Coller le LilyGo A7670G au fond du boîtier, environ au centre.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/831df10a-996d-4a53-8ed7-70a93542d98a" />

### 6.2 Coller l’antenne LTE

Coller l’antenne LTE sur une paroi intérieure du boîtier.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/4a2fa625-9a55-4beb-96da-9bc6836c81ca" />

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/3f305027-f5e9-4d01-b40a-2abe0cdee271" />

### 6.3 Coller l’antenne GPS

Coller l’antenne GPS à l’aide de ruban double-face.

Utiliser **3 couches de ruban double-face**.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/51e6fa70-1912-4951-a051-2b01e2bc5c0f" />

### 6.4 Coller le bloc d’alimentation

Coller le bloc d’alimentation au fond du boîtier à l’aide de ruban double-face.

Une seule couche de ruban double-face suffit pour cette pièce.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/7b6ff64e-5398-4153-a9a6-043a971625db" />

## 7. Passage et préparation du fil d’alimentation

### 7.1 Insérer le fil dans le presse-étoupe

Insérer le fil d’alimentation à travers le presse-étoupe.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/a8a77d6a-9725-454c-8aa5-37a8fa81979f" />

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/c877c51b-46a6-48b1-8dc5-525b4fba702f" />

### 7.2 Dénuder le fil d’alimentation

Dénuder les conducteurs du fil d’alimentation.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/5a607419-0586-4142-8fa8-b046d654291a" />

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/8a679835-4d70-4df9-a401-bc1ad0fd10b7" />

### 7.3 Étamer les fils

Étamer les bouts des fils dénudés.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/7b46ab0d-a296-45d8-80d6-2de44bb153f8" />

## 8. Connexion au bloc d’alimentation

### 8.1 Souder les fils

Souder les fils du câble d’alimentation aux fils correspondants du bloc d’alimentation.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/41f250ca-7469-4656-93e6-f4ac287b719e" />

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/7820e15a-13d7-4c28-a9d1-3e78bd7bce46" />

### 8.2 Isoler les soudures

Mettre du ruban électrique autour des fils soudés afin d’isoler les connexions.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/9266beb4-2f13-4888-a0c1-4ef91823baa6" />

## 9. Finition du montage

### 9.1 Sécuriser les fils

Sécuriser les fils au fond du boîtier à l’aide de tie-wraps.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/9ffe6e58-c641-4cc7-80a7-b825dee20b2c" />

### 9.2 Vérification finale

Avant de refermer le boîtier, vérifier que :

* le LilyGo A7670G est solidement fixé;
* les antennes GPS et LTE sont branchées;
* le câble d’alimentation est bien retenu par le presse-étoupe;
* les soudures sont isolées;
* les fils ne bougent pas librement dans le boîtier;
* rien ne force sur les connecteurs du LilyGo.

## 10. Montage terminé

Le traceur GPS Trackteur est maintenant assemblé.

<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/2190d82f-7dbe-4353-b673-b5b4f96c36e9" />
