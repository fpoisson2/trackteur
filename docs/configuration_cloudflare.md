# Configuration de Cloudflare avec des Tunnels

La configuration de Cloudflare est essentielle pour sécuriser et optimiser l'accès à votre serveur Traccar. L'utilisation de Cloudflare Tunnels est particulièrement avantageuse pour exposer des services internes de manière sécurisée sans ouvrir de ports sur votre pare-feu.

## Prérequis

- Un compte Cloudflare actif.
- Un nom de domaine enregistré et géré par Cloudflare.
- Un serveur Linux avec Traccar installé et fonctionnant (généralement sur `localhost:8082`).

## Qu'est-ce que Cloudflare Tunnel ?

Cloudflare Tunnel (anciennement Argo Tunnel) crée une connexion sécurisée et chiffrée entre votre serveur et le réseau Cloudflare, sans qu'il soit nécessaire d'ouvrir des ports entrants sur votre pare-feu. Votre serveur initie la connexion sortante vers Cloudflare, ce qui rend l'exposition de services beaucoup plus sécurisée.

## 1. Installation de `cloudflared` sur votre serveur

`cloudflared` est le démon client de Cloudflare Tunnel.

1.  **Téléchargez `cloudflared` :**

    Pour les systèmes basés sur Debian/Ubuntu :
    ```bash
    curl -L https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64 -o /usr/local/bin/cloudflared
    ```
    *Adaptez l'URL de téléchargement à votre architecture si nécessaire (par exemple, `arm64` pour Raspberry Pi).*

2.  **Rendez l'exécutable :**
    ```bash
    sudo chmod +x /usr/local/bin/cloudflared
    ```

3.  **Vérifiez l'installation :**
    ```bash
    cloudflared --version
    ```

## 2. Authentification de `cloudflared` avec votre compte Cloudflare

Cette étape va lier `cloudflared` à votre compte Cloudflare et générer un certificat.

1.  **Lancez la commande d'authentification :**
    ```bash
    cloudflared tunnel login
    ```
2.  Une URL s'affichera dans votre terminal. Copiez-la et ouvrez-la dans votre navigateur web.
3.  Connectez-vous à votre compte Cloudflare et sélectionnez le domaine que vous souhaitez utiliser pour le tunnel.
4.  Une fois l'authentification réussie, un fichier de certificat (`cert.pem`) sera téléchargé et stocké dans le répertoire par défaut de `cloudflared` (généralement `~/.cloudflared/`).

## 3. Création et configuration du Tunnel

1.  **Créez un nouveau tunnel :**
    Donnez un nom unique à votre tunnel (par exemple, `traccar-tunnel`).
    ```bash
    cloudflared tunnel create traccar-tunnel
    ```
    Cette commande va créer le tunnel et générer un ID de tunnel (UUID) et un fichier de configuration pour celui-ci (par exemple, `~/.cloudflared/<UUID>.json`).

2.  **Créez un fichier de configuration pour le tunnel (`config.yml`) :**
    Ce fichier indique à `cloudflared` comment acheminer le trafic. Créez-le dans le même répertoire que votre certificat (`~/.cloudflared/`) ou dans un répertoire dédié au tunnel (par exemple, `/etc/cloudflared/traccar-tunnel/`).

    ```yaml
    # ~/.cloudflared/config.yml ou /etc/cloudflared/traccar-tunnel/config.yml
    tunnel: <YOUR_TUNNEL_UUID>
    credentials-file: /root/.cloudflared/<YOUR_TUNNEL_UUID>.json

    ingress:
      # Règle pour l'interface web de Traccar (port 8082)
      - hostname: traccar.<YOUR_DOMAIN.COM>
        service: http://localhost:8082
        originRequest:
          noTLSVerify: true # Utilisez cette option si Traccar est en HTTP local
                           # Ou configurez HTTPS sur Traccar si vous avez un certificat local valide

      # Règle pour le port des appareils GPS (ex: OsmAnd sur 5055)
      # Il faut exposer ce port directement, car Cloudflare ne proxyfie pas les ports non-HTTP/HTTPS par défaut
      # Pour exposer des ports non-HTTP/HTTPS via Tunnel, il faut utiliser Cloudflare Spectrum (payant)
      # Une solution plus simple est de ne pas proxyfier ce sous-domaine via Cloudflare (nuage gris)
      - hostname: gps.<YOUR_DOMAIN.COM>
        service: tcp://localhost:5055 # Exemple pour OsmAnd
        # Si vous exposez directement (sans proxy Cloudflare) ce sous-domaine (gps.YOUR_DOMAIN.COM)
        # vers votre IP, il n'y a pas besoin de règle d'ingress ici pour le port 5055.
        # Le tunnel est principalement pour le trafic HTTP/HTTPS.

      - service: http_status:404
    ```

    *Remplacez `<YOUR_TUNNEL_UUID>` par l'ID de votre tunnel et `<YOUR_DOMAIN.COM>` par votre nom de domaine.*

## 4. Configuration DNS dans Cloudflare

Pour que votre sous-domaine pointe vers votre tunnel, vous devez créer un enregistrement CNAME dans votre tableau de bord Cloudflare.

1.  **Connectez-vous à votre tableau de bord Cloudflare.**
2.  Sélectionnez votre nom de domaine.
3.  Dans le menu de gauche, allez dans la section **DNS**.
4.  Cliquez sur **Ajouter un enregistrement** (`Add record`).

    -   **Type**: `CNAME`
    -   **Nom**: `traccar` (ou le nom de votre sous-domaine)
    -   **Cible**: L'adresse fournie par `cloudflared` lors de la création du tunnel. Elle se termine généralement par `cfargotunnel.com`. Par exemple, `abcd.cfargotunnel.com`.
    -   **Proxy status**: Assurez-vous que le nuage orange est activé (Proxied).

5.  Cliquez sur **Enregistrer**.

*Note : Si vous exposez des ports non-HTTP/HTTPS (comme 5055 pour les appareils GPS) sans Cloudflare Spectrum, vous devrez créer un enregistrement `A` pointant directement vers votre IP de serveur pour un sous-domaine comme `gps.YOUR_DOMAIN.COM`, et vous assurer que ce sous-domaine **n'est pas proxyfié** (nuage gris).*

## 5. Exécution de `cloudflared` comme service système

Pour que votre tunnel reste actif, même après un redémarrage du serveur, exécutez `cloudflared` en tant que service système.

1.  **Installez le service :**
    ```bash
    sudo cloudflared tunnel install
    ```
2.  **Démarrez le service :**
    ```bash
    sudo systemctl start cloudflared
    ```
3.  **Vérifiez le statut du service :**
    ```bash
    sudo systemctl status cloudflared
    ```
4.  **Assurez-vous qu'il démarre au boot :**
    ```bash
    sudo systemctl enable cloudflared
    ```

## Conclusion

Votre serveur Traccar est maintenant accessible de manière sécurisée via Cloudflare Tunnel. Cette configuration offre une couche de sécurité supplémentaire en masquant l'adresse IP de votre serveur et en utilisant les protections DDoS et le pare-feu applicatif web (WAF) de Cloudflare.

N'oubliez pas de mettre à jour votre `/opt/traccar/conf/traccar.xml` pour que Traccar écoute sur `localhost` si ce n'est pas déjà le cas, et configurez vos appareils GPS pour qu'ils pointent vers `traccar.<YOUR_DOMAIN.COM>` (ou `gps.<YOUR_DOMAIN.COM>` si vous avez configuré un sous-domaine non proxyfié pour les ports non-HTTP/HTTPS).