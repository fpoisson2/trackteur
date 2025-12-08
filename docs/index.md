# Installation de Traccar avec Docker

Ce guide explique comment installer Traccar en utilisant Docker et Docker Compose. Cette méthode est recommandée car elle simplifie le déploiement et la gestion de Traccar et de ses dépendances.

## Prérequis

-   Un serveur Linux.
-   Un accès root ou un utilisateur avec des privilèges `sudo`.
-   [Docker](https://docs.docker.com/engine/install/) et [Docker Compose](https://docs.docker.com/compose/install/) installés.

### Installer Docker/Docker Compose sur Ubuntu (22.04 ou plus récent)

1.  **Désinstaller les anciens paquets si présents :**

    ```bash
    sudo apt remove docker docker-engine docker.io containerd runc
    ```

2.  **Installer les dépendances et ajouter la clé GPG officielle :**

    ```bash
    sudo apt update
    sudo apt install -y ca-certificates curl gnupg
    sudo install -m 0755 -d /etc/apt/keyrings
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
    sudo chmod a+r /etc/apt/keyrings/docker.gpg
    ```

3.  **Ajouter le dépôt Docker et installer le moteur avec le plugin Compose :**

    ```bash
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
    sudo apt update
    sudo apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
    ```

4.  **Vérifier que Docker et Compose fonctionnent :**

    ```bash
    sudo docker run --rm hello-world
    docker compose version
    ```

5.  **(Optionnel) Autoriser l’utilisateur courant à utiliser Docker sans `sudo` :**

    ```bash
    sudo usermod -aG docker $USER
    newgrp docker
    ```

## Fichier `docker-compose.yml`

Le fichier `docker-compose.yml` à la racine de ce projet définit les services suivants :

-   `database`: Un conteneur MySQL pour stocker les données de Traccar.
-   `traccar`: Le conteneur principal de Traccar.
-   `cloudflared`: Un conteneur pour le tunnel Cloudflare, qui expose Traccar de manière sécurisée.
-   `autoheal`: Un conteneur qui surveille et redémarre les conteneurs en mauvaise santé.

### Personnalisation du `docker-compose.yml`

-   **Volumes**: Les volumes sont configurés pour stocker les données de la base de données (`/opt/traccar/data`) et les logs de Traccar (`/opt/traccar/logs`) sur l'hôte. Vous pouvez changer ces chemins si nécessaire.
-   **Ports**: Le port `8082` est exposé pour l'interface web de Traccar. La plage de ports `5000-5500` est également exposée pour la communication avec les appareils GPS. Vous pouvez ajuster cette plage en fonction des ports requis par vos appareils.

## Démarrage des conteneurs

Pour démarrer tous les services, exécutez la commande suivante à la racine du projet :

```bash
sudo docker-compose up -d
```

-   `up`: Crée et démarre les conteneurs.
-   `-d`: Exécute les conteneurs en arrière-plan (detached mode).

Les images Docker seront téléchargées, et les conteneurs seront créés et démarrés.

## Gestion des services

-   **Voir les logs**: Pour voir les logs de tous les services :

    ```bash
    sudo docker-compose logs -f
    ```

    Pour voir les logs d'un service spécifique (par exemple, `traccar`) :

    ```bash
    sudo docker-compose logs -f traccar
    ```

-   **Arrêter les services**:

    ```bash
    sudo docker-compose down
    ```

-   **Redémarrer les services**:

    ```bash
    sudo docker-compose restart
    ```

## Accès à l'interface Web

Une fois les conteneurs démarrés, vous pouvez accéder à l'interface web de Traccar via l'URL que vous avez configurée avec votre tunnel Cloudflare (par exemple, `https://traccar.votredomaine.com`).

-   **Identifiant par défaut**: `admin`
-   **Mot de passe par défaut**: `admin`

Il est fortement recommandé de changer le mot de passe administrateur dès votre première connexion.

## Configuration de Traccar

Avec cette configuration Docker, les paramètres de Traccar sont gérés via des variables d'environnement dans le fichier `docker-compose.yml`. Si vous avez besoin de modifier la configuration avancée, vous pouvez monter un fichier `traccar.xml` personnalisé dans le conteneur `traccar` en ajoutant un volume :

```yaml
volumes:
  - /chemin/vers/votre/traccar.xml:/opt/traccar/conf/traccar.xml
  - /opt/traccar/logs:/opt/traccar/logs
```

Consultez la [documentation officielle de Traccar](https://www.traccar.org/documentation/traccar-with-docker/) pour plus d'informations sur la configuration avec Docker.