1. docker-compose.yml
'''version: "3.9"

services:

  database:
    image: timescale/timescaledb:latest-pg16
    restart: unless-stopped
    environment:
      POSTGRES_DB: traccar
      POSTGRES_USER: traccar
      POSTGRES_PASSWORD: traccar
      TIMESCALEDB_TELEMETRY: "off"
    volumes:
      - /opt/traccar/data:/var/lib/postgresql/data

  traccar:
    image: traccar/traccar:latest
    restart: unless-stopped
    depends_on:
      - database
    environment:
      CONFIG_USE_ENVIRONMENT_VARIABLES: "true"
      DATABASE_DRIVER: org.postgresql.Driver
      DATABASE_URL: jdbc:postgresql://database:5432/traccar
      DATABASE_USER: traccar
      DATABASE_PASSWORD: traccar
    healthcheck:
      test: [ "CMD", "wget", "-q", "--spider", "http://localhost:8082/api/health" ]
      interval: 2m
      timeout: 5s
      start_period: 1h
      retries: 3
    # Tu peux garder le port local pour debug,
    # mais il n'est pas nécessaire pour le tunnel Cloudflare.
    ports:
      - "8082:8082"
      - "5000-5500:5000-5500"
    volumes:
      - /opt/traccar/logs:/opt/traccar/logs

  cloudflared:
    image: cloudflare/cloudflared:latest
    restart: unless-stopped
    # Le token vient de Cloudflare (Zero Trust → Tunnels)
    command: tunnel --no-autoupdate run
    environment:
      - TUNNEL_TOKEN=${TUNNEL_TOKEN}
    depends_on:
      - traccar

  autoheal:
    image: willfarrell/autoheal:latest
    restart: always
    environment:
      AUTOHEAL_CONTAINER_LABEL: all
      AUTOHEAL_INTERVAL: 60
      AUTOHEAL_START_PERIOD: 3600
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock:ro'''


Le service cloudflared utilise le mode “tunnel token” recommandé par Cloudflare pour Docker : on lui passe simplement un TUNNEL_TOKEN et il se connecte à ton tunnel géré côté Cloudflare. 
hub.docker.com
+1

2. Fichier .env

Dans le même dossier que ton docker-compose.yml, crée un fichier .env :

TUNNEL_TOKEN=colle-ici-ton-token-cloudflare


Ce token sera fourni par Cloudflare quand tu créeras le tunnel (voir étape suivante).

3. Création du tunnel Cloudflare

Va sur le dashboard Cloudflare et assure-toi que ton domaine (ex. edxo.ca) est géré par Cloudflare. 
Medium
+1

Menu Zero Trust → Networks → Tunnels.

Clique sur Add a tunnel / Create a tunnel.

Donne un nom (ex. traccar-tunnel).

Choisis Docker comme méthode d’installation.

Ajoute un Public hostname :

Hostname : par ex. gps.edxo.ca

Service : http://traccar:8082

(Docker DNS verra traccar comme le nom du conteneur Traccar, accessible sur le port 8082 interne.)

Cloudflare te montrera une commande du type :

docker run cloudflare/cloudflared:latest tunnel --no-autoupdate run --token <TOKEN_TRES_LONG>


Récupère uniquement la valeur après --token et colle-la dans .env :

TUNNEL_TOKEN=<TOKEN_TRES_LONG>


À partir de là, c’est le conteneur cloudflared de ton docker-compose qui jouera ce rôle.

4. Démarrage des services

Dans le dossier où se trouvent docker-compose.yml et .env :

# (Optionnel) vérifier que les dossiers existent
sudo mkdir -p /opt/traccar/data /opt/traccar/logs

# Lancer en arrière-plan
docker compose up -d


Vérifier que tout roule :

docker compose ps
docker compose logs -f traccar
docker compose logs -f cloudflared

5. Accès à Traccar via Cloudflare

Une fois que :

traccar est healthy,

cloudflared tourne sans erreur,

tu devrais pouvoir accéder à Traccar via :

https://gps.edxo.ca


(en remplaçant par le hostname que tu as choisi dans Cloudflare).

6. Quelques notes utiles

Quand tout sera stable et que tu seras confiant avec le tunnel, tu peux enlever le mapping 8082:8082 si tu veux vraiment qu’il ne soit accessible que via Cloudflare.

Si tu veux plusieurs hostnames (par ex. traccar1.edxo.ca, traccar2.edxo.ca), tu peux ajouter plusieurs règles d’ingress dans la config de ton tunnel côté Cloudflare.

autoheal relancera automatiquement les conteneurs marqués “unhealthy” (via les healthchecks déjà présents sur Traccar).
