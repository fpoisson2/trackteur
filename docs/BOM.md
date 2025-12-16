# BOM - Bill of Materials (Liste des composants)

## Vue d'ensemble

Ce document liste tous les composants nécessaires à la fabrication d'un traceur GPS Trackteur.

**Projet destiné au Sénégal** - Fournisseurs chinois privilégiés pour les meilleurs prix et livraison internationale.

---

## Liste des composants

| # | Composant | Description | Qté | Prix (USD) | Fournisseur |
|---|-----------|-------------|-----|------------|-------------|
| 1 | **LilyGo T-A7670G** | Module ESP32 avec modem 4G LTE et GPS intégré | 1 | 30-45 $ | [AliExpress](https://www.aliexpress.com/item/1005004125654500.html) / [LilyGo Store](https://www.lilygo.cc/) |
| 2 | **Antenne GPS** | Antenne GPS/GNSS active, connecteur IPEX/U.FL, gain 28 dB | 1 | 3-6 $ | [AliExpress](https://www.aliexpress.com/w/wholesale-gps-antenna-ipex.html) |
| 3 | **Antenne 4G/LTE** | Antenne cellulaire SMA, bandes 700-2700 MHz, gain 3-5 dBi | 1 | 3-6 $ | [AliExpress](https://www.aliexpress.com/w/wholesale-4g-lte-antenna-sma.html) |
| 4 | **Carte SIM** | Nano-SIM avec forfait data M2M/IoT | 1 | 5 $ + forfait | [Hologram.io](https://www.hologram.io/) / Opérateur local |
| 5 | **Boîtier étanche** | Boîtier ABS IP65/IP67, min 120x80x40 mm | 1 | 5-12 $ | [AliExpress](https://www.aliexpress.com/w/wholesale-waterproof-junction-box-ip65.html) |
| 6 | **Vis de fixation** | Vis inox M3 ou M4 avec écrous | 4-8 | 1-3 $ | [AliExpress](https://www.aliexpress.com/w/wholesale-m3-stainless-steel-screws.html) |
| 7 | **Presse-étoupe** | Passe-câble étanche PG7 ou PG9, IP68 | 1-2 | 1-3 $ | [AliExpress](https://www.aliexpress.com/w/wholesale-cable-gland-pg7.html) |
| 8 | **Fil d'alimentation** | Câble 2 conducteurs, 0.5-1.0 mm², rouge/noir | 2 m | 2-4 $ | [AliExpress](https://www.aliexpress.com/w/wholesale-electrical-wire-2-conductor.html) |
| 9 | **Batterie LiPo** | Batterie 3.7V, 2000-5000 mAh, connecteur JST | 1 | 8-15 $ | [AliExpress](https://www.aliexpress.com/w/wholesale-lipo-battery-3.7v-3000mah-jst.html) |
| 10 | **Convertisseur 12V-5V** | Module DC-DC step-down, entrée 8-35V, sortie 5V 3A | 1 | 2-5 $ | [AliExpress](https://www.aliexpress.com/w/wholesale-dc-dc-12v-5v-3a.html) |

---

## Coût total

| | |
|---|---|
| **Total composants** | **60-105 $ USD** |
| **Forfait data** | Variable selon opérateur |

---

## Composants optionnels

| Composant | Description | Qté | Prix (USD) | Fournisseur |
|-----------|-------------|-----|------------|-------------|
| Carte microSD | Stockage backup données GPS, 8-32 Go | 1 | 3-8 $ | [AliExpress](https://www.aliexpress.com/w/wholesale-micro-sd-card.html) |
| Fusible inline | Protection circuit 12V, 2-5A | 1 | 1-3 $ | [AliExpress](https://www.aliexpress.com/w/wholesale-inline-fuse-holder.html) |
| Connecteur déconnexion | Connecteur 2 pins étanche | 1 | 2-5 $ | [AliExpress](https://www.aliexpress.com/w/wholesale-waterproof-connector-2-pin.html) |
| LED externe | LED 5mm avec résistance | 1 | 1 $ | [AliExpress](https://www.aliexpress.com/w/wholesale-led+5mm.html) |
| Interrupteur | Interrupteur on/off étanche | 1 | 2-4 $ | [AliExpress](https://www.aliexpress.com/w/wholesale-waterproof-toggle-switch.html) |

---

## Spécifications détaillées

### LilyGo T-A7670G
- **Version**: T-A7670G (avec GPS intégré, pas T-A7670E)
- **Processeur**: ESP32 dual-core
- **Modem**: SIMCOM A7670G (4G LTE Cat-1)
- **GPS**: Multi-constellation (GPS, GLONASS, BeiDou, Galileo)
- **Bandes LTE**: B1/B3/B5/B7/B8/B20 (compatibles Afrique)

### Antennes
- **GPS**: Antenne active céramique, câble 15-25 cm, connecteur IPEX
- **4G/LTE**: Antenne omnidirectionnelle, connecteur SMA mâle

### Carte SIM - Options pour le Sénégal
- **Hologram.io**: Roaming mondial, ~0.60 $/MB
- **Orange Sénégal**: Forfaits data locaux
- **Free Sénégal**: Forfaits data locaux
- **Expresso**: Forfaits data locaux

### Convertisseur DC-DC
- **Modèles recommandés**: LM2596, XL4015, MP1584
- **Entrée**: 8-35V DC (compatible 12V/24V véhicule)
- **Sortie**: 5V DC, 3A minimum
- **Efficacité**: >90%
- **Protection**: Surtension, court-circuit

### Boîtier
- **Indice IP**: IP65 minimum (IP67 recommandé pour climat tropical)
- **Matériau**: ABS ou polycarbonate
- **Dimensions internes**: Min 120x80x40 mm
- **Couleur**: Noir (discrétion, résistance UV)

---

## Fournisseurs recommandés

| Fournisseur | Type | Délai livraison Sénégal | Notes |
|-------------|------|-------------------------|-------|
| [AliExpress](https://www.aliexpress.com/) | Électronique générale | 3-6 semaines | Meilleurs prix, livraison gratuite souvent disponible |
| [LilyGo Store](https://www.lilygo.cc/) | LilyGo officiel | 2-4 semaines | Produits authentiques garantis |
| [Banggood](https://www.banggood.com/) | Électronique | 3-6 semaines | Alternative à AliExpress |
| [Hologram.io](https://www.hologram.io/) | Carte SIM IoT | 1-2 semaines | Livraison internationale |

---

## Notes pour le Sénégal

### Compatibilité réseau LTE
Le module A7670G supporte les bandes LTE utilisées au Sénégal:
- **Orange**: B3 (1800 MHz), B7 (2600 MHz), B20 (800 MHz)
- **Free**: B3 (1800 MHz), B7 (2600 MHz)
- **Expresso**: B3 (1800 MHz)

### Considérations climatiques
- Choisir un boîtier IP67 pour résister à l'humidité et la poussière
- Prévoir une ventilation passive si installation dans un véhicule exposé au soleil
- Batterie LiPo: éviter les températures >45°C

### Livraison
- Prévoir 3-6 semaines pour la livraison depuis la Chine
- Grouper les commandes pour économiser sur les frais de port
- Vérifier les frais de douane sénégalais

---

## Outils nécessaires

| Outil | Utilité |
|-------|---------|
| Fer à souder | Connexions optionnelles |
| Tournevis cruciforme/plat | Assemblage boîtier |
| Pince à dénuder | Préparation fils |
| Perceuse | Perçage boîtier (presse-étoupe) |
| Pistolet à colle chaude | Fixation interne |
| Multimètre | Vérification tensions |

---

## Notes importantes

1. **Vérifier la compatibilité** des bandes LTE avec l'opérateur choisi
2. **Commander les antennes** avec les bons connecteurs (IPEX pour GPS, SMA pour LTE)
3. **Tester le convertisseur** avant installation (vérifier 5V stable en sortie)
4. **Activer la carte SIM** avant de l'insérer dans le module
5. **Prévoir des pièces de rechange** vu les délais de livraison
