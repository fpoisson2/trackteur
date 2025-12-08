# Configuration de Cloudflare Tunnel

Ce guide explique comment configurer un tunnel Cloudflare pour exposer votre instance Traccar de manière sécurisée.

## Prérequis

-   Un compte Cloudflare actif.
-   Un nom de domaine enregistré et géré par Cloudflare.
-   Traccar fonctionnant (via Docker ou une autre méthode).

## 1. Créer le fichier `.env`

Le service `cloudflared` a besoin d'un token de tunnel pour s'authentifier auprès de Cloudflare. Ce token est fourni via une variable d'environnement.

1.  Créez un fichier `.env` à la racine du projet :

    ```bash
    touch .env
    ```

2.  Ajoutez votre token de tunnel Cloudflare au fichier `.env` :

    ```
    TUNNEL_TOKEN=<VOTRE_TOKEN_DE_TUNNEL_CLOUDFLARE>
    ```

    Remplacez `<VOTRE_TOKEN_DE_TUNNEL_CLOUDFLARE>` par le token que vous obtiendrez à l'étape suivante.

## 2. Créer le tunnel Cloudflare

1.  Sur le dashboard Cloudflare, assurez-vous que votre domaine (ex. `votredomaine.com`) est géré par Cloudflare.
2.  Rendez-vous dans **Zero Trust → Networks → Tunnels** puis **Add a tunnel / Create a tunnel**.
3.  Choisissez **Docker** comme méthode d’installation.
4.  Copiez le token fourni (il ressemble à `ey...`). C'est votre `TUNNEL_TOKEN`. Collez-le dans votre fichier `.env`.
5.  Dans la section **Public Hostnames**, créez un enregistrement :
    -   **Subdomain**: `traccar` (ou ce que vous préférez).
    -   **Domain**: Votre domaine.
    -   **Service**: `HTTP` et `http://traccar:8082`.
6.  Enregistrez le tunnel.

## 3. Démarrer les services

Si ce n'est pas déjà fait, démarrez la stack Docker :

```bash
sudo docker-compose up -d
```

Le conteneur `cloudflared` devrait maintenant démarrer et établir la connexion avec Cloudflare.

## 4. Accéder à Traccar

Une fois que `cloudflared` est en cours d'exécution, vous pouvez accéder à l'interface de Traccar via l'URL que vous avez configurée (par exemple, `https://traccar.votredomaine.com`).

## Notes utiles

-   Vous pouvez retirer le mapping de port `8082:8082` du service `traccar` dans `docker-compose.yml` pour n'exposer Traccar qu'à travers le tunnel Cloudflare.
-   Pour ajouter d'autres services, vous pouvez ajouter des "Public Hostnames" à votre tunnel.