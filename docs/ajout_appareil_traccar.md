# Ajout d'un appareil dans Traccar

Avant qu'un traceur puisse apparaître sur la carte, il doit être déclaré dans
Traccar. Le serveur accepte les positions uniquement pour les appareils dont
l'**identifiant** est déjà enregistré.

> Procédure réalisée lors du 2<sup>e</sup> bloc de formation (29 juin 2026).

## Quand le faire

À faire **avant le premier téléversement** du firmware, ou pendant que
l'installation du support ESP32 se termine dans l'Arduino IDE. L'identifiant
choisi ici doit ensuite être recopié dans le code (`client_id` / `TRACCAR_DEVICE_ID`).

## Procédure

1. Se connecter à l'interface web du serveur Traccar
   (ex. [https://serveur1e.trackteur.cc](https://serveur1e.trackteur.cc))
2. Ouvrir le menu **Paramètres** (*Settings*) → **Appareils** (*Devices*)
3. Cliquer sur **+** / **Ajouter un appareil** (*Add Device*)
4. Remplir la fiche:

    | Champ | Valeur | Exemple |
    |-------|--------|---------|
    | **Nom** | Libellé lisible pour l'utilisateur | `Tracteur pro 1` |
    | **Identifiant** | Identifiant technique, **en minuscules**, sans espace ni accent | `tracteur_pro_1` |
    | **Groupe** | Optionnel (utile pour regrouper par site ou par service) | — |

5. Cliquer sur **Enregistrer** (*Save*)

L'appareil apparaît alors dans la liste, sans position tant qu'aucune donnée
n'a été reçue.

## Règles de nommage

> ⚠️ **L'identifiant est la clé de tout le système**
>
> La même chaîne doit se retrouver à trois endroits:
>
> 1. le champ **Identifiant** de l'appareil dans Traccar;
> 2. la variable `client_id` (ou `TRACCAR_DEVICE_ID` dans `config.h`) du firmware;
> 3. le **nom du dispositif** dans le dashboard Hologram.
>
> Écrire également ce nom **au marqueur sur le boîtier** du traceur. C'est ce qui
> permet, six mois plus tard, de savoir quel boîtier correspond à quelle ligne sur
> la carte et à quelle carte SIM facturée.

Conventions retenues pour le projet:

- minuscules uniquement, pas de majuscules;
- pas d'espaces, pas d'accents; utiliser le tiret bas `_`;
- une numérotation séquentielle: `tracteur_1`, `tracteur_2`, `tracteur_pro_1`…

Le **Nom** affiché, lui, peut être modifié à tout moment sans rien casser.
L'**Identifiant**, non: le changer coupe la réception des positions tant que le
firmware n'est pas retéléversé.

## Vérification de la réception

1. Téléverser le firmware et ouvrir le moniteur série à 115200 baud
2. Attendre le fix GPS (peut prendre plusieurs minutes, voir
   [Programmation du LilyGo A7670G](programmation_liligo_a7670g.md))
3. Dans Traccar, sélectionner l'appareil: la position doit apparaître, puis se
   mettre à jour selon l'intervalle configuré (2 minutes par défaut)

### Si rien n'arrive

| Symptôme | Cause probable |
|----------|----------------|
| Le moniteur série indique un envoi réussi, mais l'appareil reste « inconnu » dans Traccar | L'**Identifiant** ne correspond pas exactement au `client_id` (majuscules, espace, faute de frappe) |
| Aucun envoi dans le moniteur série | Pas encore de fix GPS, ou pas de connexion réseau (voir dépannage Hologram) |
| Erreur HTTP dans le moniteur série | URL du serveur erronée dans le code |

## Ressources

- [Documentation Traccar](https://www.traccar.org/documentation/)
- [Protocole OsmAnd](https://www.traccar.org/osmand/)
