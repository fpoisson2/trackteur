# Création et activation d'une carte SIM Hologram

Guide pour activer et configurer une carte SIM Hologram IoT pour le traceur GPS.

> Cette procédure a été réalisée pas à pas lors du 2<sup>e</sup> bloc de formation
> (29 juin 2026) sur le dashboard Hologram.

## Pourquoi Hologram?

- **Couverture mondiale**: Roaming automatique dans 200+ pays
- **Bandes Africa**: Compatible avec les réseaux Sénégal (Orange, Free, Expresso)
- **Tarification IoT**: Optimisé pour faible volume de données
- **Pas de contrat**: Pay-as-you-go
- **Dashboard**: Monitoring en temps réel de la consommation

## Prérequis

- Un compte Hologram (créé ou reçu par invitation d'un administrateur du projet)
- Un **solde disponible** suffisant sur le compte (voir [Gestion du solde](#gestion-du-solde-et-facturation))
- La carte SIM Hologram déjà insérée dans le traceur, **ou** son support cartonné
  (l'ICCID est imprimé sur les deux)

## Étape 1: Création du compte

1. Aller sur [https://dashboard.hologram.io/register](https://dashboard.hologram.io/register)
2. Créer un compte avec une adresse courriel professionnelle
3. Valider le courriel de confirmation

> Si un membre de l'équipe vous a envoyé une invitation, il suffit de l'accepter puis
> de se connecter avec **Login / Se connecter** sur [hologram.io](https://www.hologram.io).

> 💡 **Page qui ne charge pas**
>
> Il arrive que le dashboard affiche une erreur au premier chargement.
> Rafraîchir la page (F5) règle le problème dans la majorité des cas;
> sinon, essayer un autre navigateur (Chrome ↔ Edge).


## Étape 2: Commander les cartes SIM

1. Dans le dashboard, aller sur **Devices** → **Order SIMs**
2. Sélectionner le type de SIM:
   - **Hologram Hyper SIM** (recommandé) - Meilleure couverture
   - Format: **Nano SIM** (pour LilyGo T-A7670G)
3. Quantité selon besoin
4. Livraison: ~1-2 semaines vers le Sénégal

## Étape 3: Activation de la SIM (procédure détaillée)

Le tableau de bord d'accueil affiche l'état du parc: *cartes SIM actives*,
*cartes SIM connectées* et *cartes SIM prêtes à l'emploi*. On active ici une
carte parmi les cartes prêtes à l'emploi.

1. Cliquer sur la tuile **Cartes SIM actives** (*Active SIMs*)
2. Cliquer sur le gros bouton bleu **Activer les cartes SIM** (*Activate SIMs*)
3. **Saisir l'ICCID** de la carte à activer

    - L'ICCID est la longue séquence de chiffres imprimée **sur la carte SIM
      elle-même** et **sur le support cartonné** dans lequel elle était livrée.
    - Il commence par `89` et compte 18 à 20 chiffres.
    - Si la carte est déjà montée dans le traceur, utiliser le support cartonné.

    > ⚠️ **Carte invalide**
    >
    > Si le message *carte invalide* apparaît, c'est presque toujours une erreur
    > de saisie. Revérifier la séquence chiffre par chiffre. Le support cartonné
    > porte parfois **deux séquences différentes** (ICCID et numéro de lot):
    > si la première est refusée, essayer l'autre.


4. Cliquer sur **Vérifier les cartes SIM** (*Verify SIMs*)
5. Descendre en bas de page et cliquer sur **Confirmer les cartes SIM sélectionnées**
6. **Couverture et tarifs**: laisser les valeurs par défaut, cliquer sur le bouton bleu
7. **Limite d'utilisation des données**: choisir **Ne pas interrompre** (*Don't pause*)

    > Ce réglage évite que la carte soit coupée automatiquement si elle dépasse
    > un seuil de données. Avec le volume d'un traceur (quelques Mo/mois), le
    > risque financier est nul et une coupure ferait perdre le suivi.

8. **Nom du dispositif**: donner un identifiant clair et reconnaissable, par exemple
   `tracteur_pro_1`, `tracteur_2`, `tracteur_3`…

    > 💡 **Astuce**
    >
    > Utiliser la **même convention de nommage** que l'identifiant de l'appareil
    > dans Traccar et dans `config.h`, et **inscrire le nom sur le boîtier** du
    > traceur au marqueur. C'est ce qui permet de faire le lien entre un boîtier
    > physique, une carte SIM et un appareil sur la carte.


9. **Confirmer la configuration**
10. Section **Solde du compte**: cliquer sur **Payer et activer** (*Pay & Activate*)
11. Retourner à la liste des cartes SIM

    La carte apparaît d'abord comme *dispositif non identifié*, avec le statut
    **En attente** (*Pending*). **Rafraîchir la page** pour voir apparaître le nom
    choisi à l'étape 8. L'activation complète prend quelques minutes.

### Vérification de l'APN

1. Cliquer sur le device activé
2. Aller dans **Configuration**
3. Vérifier l'APN: `hologram` (valeur par défaut, à ne pas modifier)
4. Vérifier que **Data** est activé (toggle ON)

## Gestion du solde et facturation

Les cartes SIM Hologram fonctionnent avec un **solde prépayé**, comme une recharge
de téléphone portable. Sans solde disponible, les cartes cessent de fonctionner.

### Consulter le solde

1. Menu de gauche → **Billing** (Facturation) → **Account** (Compte)
2. La ligne **Solde disponible** (*Available balance*) affiche le crédit restant,
   en **dollars canadiens**

### Ajouter des fonds

1. Menu de gauche → **Billing** → **Account**
2. Cliquer sur le bouton bleu **Add balance** (Ajouter au solde)
3. Saisir le montant à ajouter (ex. `5` pour 5 $)
4. Confirmer avec la carte de crédit enregistrée

    > 📌 **Note**
    >
    > Il faut disposer des droits de facturation sur le compte pour voir ce bouton.
    > Demander à un administrateur du projet de vous les accorder au besoin.


### Coût récurrent

- **1 $ par carte SIM active et par mois**, prélevé automatiquement sur le solde
  au début de chaque mois.
- S'y ajoute la consommation de données (voir tableau plus bas).
- Prévoir de recharger le solde avant qu'il ne tombe à zéro; désactiver dans le
  dashboard les cartes SIM qui ne servent plus, pour ne pas payer le 1 $/mois inutilement.

> ⚠️ **Transfert de la facturation**
>
> Trois abonnements sont à la charge du projet: **Hologram** (cartes SIM),
> **l'hébergement cloud** des serveurs Traccar et le **nom de domaine**.
> À la fin du projet, la carte de crédit utilisée devra être remplacée par
> celle de l'organisation qui exploite le système.


## Étape 4: Configuration firmware

Dans `code/TraccarGPS/config.h` du traceur:

```cpp
#define NETWORK_APN "hologram"
```

L'APN Hologram s'écrit **en minuscules** et ne nécessite ni nom d'utilisateur ni
mot de passe.

> Dans les exemples fournis par LilyGo, l'APN est préconfiguré à `ctlte`
> (opérateur chinois). Il faut impérativement le remplacer par `hologram`.

## Étape 5: Vérification

### Dans le dashboard Hologram

1. **Usage** → Voir la consommation data
2. **Device Logs** → Voir les connexions réseau
3. **Coverage Map** → Vérifier la couverture dans votre zone

### Sur le traceur

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

### ICCID refusé à l'activation

```
Carte invalide / Invalid SIM
```

- Revérifier chaque chiffre de la séquence (les `0`/`O` et `1`/`7` sont les pièges classiques)
- Essayer la seconde séquence imprimée sur le support cartonné
- Vérifier que la carte n'est pas déjà activée sur un autre compte

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
- Vérifier que le plan est actif et que le **solde du compte est suffisant**
- Essayer de redémarrer le traceur

### Data ne fonctionne pas

```
Failed to activate network
```

- Vérifier l'APN dans `config.h`: `hologram`
- Vérifier que Data est activé dans le dashboard
- Attendre 1-2 minutes après insertion SIM

## Tarification Hologram

| Plan | Inclus | Prix |
|------|--------|------|
| Frais par SIM active | - | $1.00/mois |
| Pay-as-you-go | - | $0.40/MB |
| 1MB Pool | 1MB/mois | $1.00/mois |
| 5MB Pool | 5MB/mois | $3.00/mois |
| 10MB Pool | 10MB/mois | $5.00/mois |

Pour un traceur à 2 min d'intervalle: **1MB Pool** suffit.

## Ressources

- [Documentation Hologram](https://www.hologram.io/docs/)
- [Coverage Map](https://www.hologram.io/coverage/)
- [Support](https://support.hologram.io/)
