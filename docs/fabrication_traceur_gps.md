# Fabrication du traceur GPS

Ce guide détaille l'assemblage mécanique du traceur GPS Trackteur.

## Matériel requis

Référez-vous au [BOM (Bill of Materials)](BOM.md) pour la liste complète des composants avec images.

<img width="2985" height="1440" alt="image" src="https://github.com/user-attachments/assets/67494daa-26a2-4684-aba2-0f6935360b2e" />


## Schéma de câblage

```mermaid
flowchart LR
    subgraph vehicule["Véhicule (Tracteur, Voiture, etc.)"]
        ACC[Fil ACC 12V]
        GND[Masse]
    end

    subgraph jonction["Jonction 3 pattes"]
        J[Connexion ACC]
    end

    subgraph connecteur["Connecteur 2 pattes<br/>(extérieur boîtier)"]
        C[Déconnexion rapide]
    end

    subgraph boitier["Boîtier étanche IP67"]
        PE[Presse-étoupe]
        CONV[Convertisseur<br/>12V → 5V]
        LILYGO[LilyGo A7670G]
        BAT[Batteries 18650<br/>backup]
        SD[Carte SD]
        ANT1[Antenne GPS]
        ANT2[Antenne 4G]
    end

    ACC --> J
    GND --> J
    J -->|Câble weatherproof| C
    C -->|Câble weatherproof| PE
    PE --> CONV
    CONV -->|USB 5V| LILYGO
    BAT -.->|Backup| LILYGO
    SD -.-> LILYGO
    ANT1 -.-> LILYGO
    ANT2 -.-> LILYGO
```

### Légende

- **Trait plein** : Connexions d'alimentation
- **Trait pointillé** : Connexions internes au boîtier

## Étapes d'assemblage

### Étape 1 : Préparation du boîtier

1. Percez un trou dans le boîtier pour le presse-étoupe (diamètre de 12.5mm ou 1/2 pouce)
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/42c9418b-834a-42d7-9447-322003ebc5f7" />
2. Installez le presse-étoupe et serrez-le pour assurer l'étanchéité
<img width="303" height="273" alt="image" src="https://github.com/user-attachments/assets/bd462352-a265-4979-a9f1-f41eb759ed26" />

### Étape 2 : Installation des composants dans le boîtier
1. Poser les pattes autocollantes dans les trous du LilygoA7670G et retirer la pellicule verte.
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/e3b76e1d-88d5-4bd8-a8e9-e0f1917a29a9" />
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/b3091e58-6234-4b5c-a009-7035295e25de" />
2. Brancher l'antenne LTE autocollante
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/6359e923-b2cf-4724-8230-4ffdbd461d55" />
3. Brancher l'antenne GPS
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/267f575e-2e2a-441c-b963-96dfe97d88e7" />
4. Brancher le câble d'alimentation
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/6b677180-23c1-4251-b589-66eb5b796cce" />
5. Coller le Lilygo A7670G au fond du boîtier. Le place environ au centre.
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/831df10a-996d-4a53-8ed7-70a93542d98a" />
6. Coller l'antenne LTE sur la paroi du boîtier
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/4a2fa625-9a55-4beb-96da-9bc6836c81ca" />
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/3f305027-f5e9-4d01-b40a-2abe0cdee271" />
7. Coller l'antenne GPS à l'aide du ruban double-face, mettre 3 couches de ruban double face.
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/51e6fa70-1912-4951-a051-2b01e2bc5c0f" />
8. Coller le bloc d'alimentation au fond du boîtier à l'aide du ruban double face, une couche suffit ici.
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/7b6ff64e-5398-4153-a9a6-043a971625db" />
9. Insérer le fil d'alimentation à travers le presse-étoupe
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/a8a77d6a-9725-454c-8aa5-37a8fa81979f" />
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/c877c51b-46a6-48b1-8dc5-525b4fba702f" />
10. Dénuder le fil d'alimentation
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/5a607419-0586-4142-8fa8-b046d654291a" />
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/8a679835-4d70-4df9-a401-bc1ad0fd10b7" />
11. Étamer les bouts de fils dénudés.
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/7b46ab0d-a296-45d8-80d6-2de44bb153f8" />
12. Souder avec les fils correspondandts du bloc d'alimentation
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/41f250ca-7469-4656-93e6-f4ac287b719e" />
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/7820e15a-13d7-4c28-a9d1-3e78bd7bce46" />
13. Mettre du ruban électrique autour des fils soudés
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/9266beb4-2f13-4888-a0c1-4ef91823baa6" />
14. Sécuriser à l'aide de tie-wrap les fils au fond du boîtier
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/9ffe6e58-c641-4cc7-80a7-b825dee20b2c" />
15. C'est terminé!
<img width="1440" height="1920" alt="image" src="https://github.com/user-attachments/assets/2190d82f-7dbe-4353-b673-b5b4f96c36e9" />
