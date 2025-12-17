# Création de carte SIM Hologram

Guide pour activer et configurer une carte SIM Hologram IoT pour le tracker GPS.

## Pourquoi Hologram?

- **Couverture mondiale**: Roaming automatique dans 200+ pays
- **Bandes Africa**: Compatible avec les réseaux Sénégal (Orange, Free, Expresso)
- **Tarification IoT**: Optimisé pour faible volume de données
- **Pas de contrat**: Pay-as-you-go
- **Dashboard**: Monitoring en temps réel de la consommation

## Prérequis

- Compte Hologram (gratuit)
- Carte de crédit pour l'activation
- Carte SIM Hologram (commandée sur hologram.io)

## Étape 1: Création du compte

1. Aller sur [https://dashboard.hologram.io/register](https://dashboard.hologram.io/register)
2. Créer un compte avec email professionnel
3. Valider l'email de confirmation

## Étape 2: Commander les cartes SIM

1. Dans le dashboard, aller sur **Devices** → **Order SIMs**
2. Sélectionner le type de SIM:
   - **Hologram Hyper SIM** (recommandé) - Meilleure couverture
   - Format: **Nano SIM** (pour LilyGo T-A7670G)
3. Quantité selon besoin
4. Livraison: ~1-2 semaines vers le Sénégal

## Étape 3: Activation de la SIM

### Via Dashboard

1. Aller sur **Devices** → **Activate SIM**
2. Scanner ou entrer le **ICCID** (numéro sur la carte SIM)
3. Donner un nom au device: `TRACTEUR_001`
4. Sélectionner le plan:
   - **Pay-as-you-go**: $0.40/MB (petits volumes)
   - **Pooled plan**: À partir de $1/mois pour 1MB partagé

### Configuration du device

1. Cliquer sur le device activé
2. Aller dans **Configuration**
3. Vérifier l'APN: `hologram` (par défaut)
4. Activer **Data** (toggle ON)

## Étape 4: Configuration firmware

Dans `config.h` du tracker:

```cpp
#define NETWORK_APN "hologram"
```

C'est tout! L'APN Hologram ne nécessite ni username ni password.

## Étape 5: Vérification

### Dans le dashboard Hologram

1. **Usage** → Voir la consommation data
2. **Device Logs** → Voir les connexions réseau
3. **Coverage Map** → Vérifier la couverture dans votre zone

### Sur le tracker

Moniteur série (115200 baud):
```
Setting network APN: hologram
Registering on network...
Network registered!
Activating network...
IP Address: 10.x.x.x
```

## Consommation data estimée

| Fréquence | Data/jour | Data/mois | Coût/mois |
|-----------|-----------|-----------|-----------|
| 2 min | ~50 KB | ~1.5 MB | ~$0.60 |
| 5 min | ~20 KB | ~600 KB | ~$0.25 |
| 10 min | ~10 KB | ~300 KB | ~$0.12 |

Chaque transmission ≈ 200-500 bytes (protocole OsmAnd)

## Alertes et monitoring

### Configurer des alertes

1. **Billing** → **Alerts** → **Add Alert**
2. Seuil de consommation (ex: 5MB)
3. Notification par email

### Webhook (optionnel)

Pour recevoir les événements device:
1. **Devices** → **Webhooks**
2. URL de votre endpoint
3. Events: `device.connect`, `device.disconnect`

## Dépannage

### SIM non reconnue

```
SIM status: SIM_ERROR
```

- Vérifier l'insertion (contacts vers le bas)
- Vérifier que la SIM est activée dans le dashboard
- Vérifier le format (Nano SIM)

### Pas de réseau

```
Network registration denied!
```

- Vérifier la couverture dans la zone
- Vérifier que le plan est actif (solde suffisant)
- Essayer de redémarrer le tracker

### Data ne fonctionne pas

```
Failed to activate network
```

- Vérifier l'APN dans config.h: `hologram`
- Vérifier que Data est activé dans le dashboard
- Attendre 1-2 minutes après insertion SIM

## Tarification Hologram (2025)

| Plan | Inclus | Prix |
|------|--------|------|
| Pay-as-you-go | - | $0.40/MB |
| 1MB Pool | 1MB/mois | $1.00/mois |
| 5MB Pool | 5MB/mois | $3.00/mois |
| 10MB Pool | 10MB/mois | $5.00/mois |

Pour un tracker à 2 min d'intervalle: **1MB Pool** suffit.

## Ressources

- [Documentation Hologram](https://www.hologram.io/docs/)
- [Coverage Map](https://www.hologram.io/coverage/)
- [Support](https://support.hologram.io/)
