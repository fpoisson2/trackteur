# Déployer Traccar avec Docker et MySQL

Ce dépôt propose un exemple de stack Docker pour Traccar utilisant MySQL comme base de données, un tunnel Cloudflare et un conteneur d’auto-heal. Un fichier `docker-compose.example.yml` est fourni pour éviter le copier-coller.

## Prérequis
- Docker et Docker Compose installés sur l’hôte
- Accès à un domaine géré par Cloudflare (pour le tunnel)

### Installer Docker/Docker Compose sur Ubuntu (22.04 ou plus récent)
1) Désinstaller les anciens paquets si présents :
```bash
sudo apt remove docker docker-engine docker.io containerd runc
```
2) Installer les dépendances et ajouter la clé GPG officielle :
```bash
sudo apt update
sudo apt install -y ca-certificates curl gnupg
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg
```
3) Ajouter le dépôt Docker et installer le moteur avec le plugin Compose :
```bash
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt update
sudo apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
```
4) Vérifier que Docker et Compose fonctionnent :
```bash
sudo docker run --rm hello-world
docker compose version
```
5) (Optionnel) Autoriser l’utilisateur courant à utiliser Docker sans `sudo` :
```bash
sudo usermod -aG docker $USER
newgrp docker
```

## 1. Préparer le fichier de composition
Copiez l’exemple et adaptez-le si nécessaire (chemins de volumes, ports, options MySQL, etc.) :

```bash
cp docker-compose.example.yml docker-compose.yml
```

L’exemple utilise :
- MySQL 8.4 (`traccar`/`traccar`) avec les options JDBC recommandées pour Traccar
- Traccar (`traccar/traccar:latest`) exposé sur le port 8082 et la plage 5000-5500
- `cloudflared` en mode tunnel token
- `autoheal` pour relancer les conteneurs marqués `unhealthy`

## 2. Créer le fichier `.env`
Dans le même dossier que `docker-compose.yml`, créez un `.env` contenant votre token Cloudflare :

```env
TUNNEL_TOKEN=<TOKEN_TRES_LONG>
```

Le token est fourni par Cloudflare lors de la création du tunnel (voir étape suivante).

## 3. Créer le tunnel Cloudflare
1. Sur le dashboard Cloudflare, assurez-vous que votre domaine (ex. `edxo.ca`) est géré par Cloudflare.
2. Rendez-vous dans **Zero Trust → Networks → Tunnels** puis **Add a tunnel / Create a tunnel**.
3. Choisissez Docker comme méthode d’installation et notez le token fourni après `--token`.
4. Ajoutez un **Public hostname** pointant vers `http://traccar:8082` (le nom du service Docker).
5. Collez le token dans `.env` sous `TUNNEL_TOKEN`.

## 4. Démarrer les services
Depuis le dossier contenant `docker-compose.yml` et `.env` :

```bash
sudo mkdir -p /opt/traccar/data /opt/traccar/logs
# Lancer la stack en arrière-plan
docker compose up -d
```

Vérifier les journaux si besoin :

```bash
docker compose ps
docker compose logs -f traccar
docker compose logs -f cloudflared
```

## 5. Accéder à Traccar
Une fois que Traccar est en état *healthy* et que `cloudflared` tourne, accédez à l’interface via l’URL configurée dans Cloudflare (ex. `https://gps.edxo.ca`).

## 6. Notes utiles
- Vous pouvez retirer le mapping `8082:8082` pour n’exposer Traccar que via Cloudflare.
- Pour plusieurs sous-domaines, ajoutez plusieurs règles d’ingress dans le tunnel Cloudflare.
- `autoheal` relance automatiquement les conteneurs marqués `unhealthy` via les healthchecks.
