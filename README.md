# CLI PaxOS

# Usage de ce dépôt GitHub
(A l'heure actuelle, aucune implémentation n'est terminée, et la V1 du protcole ne l'est probablement pas non plus)

Ce dépôt contient aussi les implémentations pour l'hôte, en Python et en JavaScript (passe par la technologie WebSerial).

# Idée de base
Pour une simplicité utilisateur, le modèle UART se fera autour d'un invité de commande alimenté par l'appareil. Les commandes seront dans intuitives, et une commande `help` sera fournie. Toute commande commancant par un symbole `~` indique une commande qui se fera programmatiquement. Certaines commandes ne sont pas disponibles en mode programme, et d'autres ne sont pas disponibles en mode utilisateur.

Du côté appareil en C++, une API ouverte est disponible pour implémenter l'invité à travers d'autres protocoles

# Configuration UART
- Fréquence: 115200 Hz
- Pas de parité
- Données de 8 bits
- Stop de 1 bit
- Flow control: CTS/RTS

# Arbre des commandes

- `info`: Indique la plupart des métadonnées de l'appareil

  - `hostname`: Donne le nom de l'appareil
  - `mac`: Donne les adresses MAC
    - `ble`: Donne l'adresse MAC pour module Bluetooth
    - `wifi`: Donne l'adresse MAC pour module WiFi
  - `version`: Donne la version de l'appareil
  - `hardware`: Indique les révisions du matériel physique utilisé
- `echo`: Active ou désactive le renvoi des caractères écrits.
- `apps`: Liste les applications installées
  - `install`: Installe une application (seulement en mode programme)
  - `remove <appid>`: Désinstalle l'application spécifiée
  - `launch <appid>`: Démarre l'application spécifiée. (Peut échouer)
  - `show <appid>`: Donne les details d'une application
    - `path`: Donne le chemin d'accès de l'application spécifiée.
- `files`: Ouvre un gestionnaire de fichiers sh-like.
- `files <cmd...>`: Execute une action dans le gestionnaire de fichiers, puis quitte.
- `elevate <permission>`: Demande une ou plusieurs permissions d'accès pour l'invité. Cette commande engendrera un message de confiramtion sur l'écran de l'appareil.
  - `cellular`: Demande les permissions pour les informations du module réseau
  - `*`: Demande l'accès à toutes les permissions
- `lte`: *Groupe de commandes* (**Nécéssite la permission `cellular`**) (seulement en mode programme)
  - `sendraw <cmd>`: Envoie une commande directe au module cellulaire de l'appareil, la réponse est renvoyée. 
  - `imei`: Donne l'IMEI du module réseau
  - `imsi`: Donne l'IMSI de la carte SIM


## Commandes du gestionnaire de fichiers

- `ls [path]`: Liste le contenu du dossier spécifié, ou du dossier courant. (Alias: `dir`)

- `cd <path>`: Change le répertoire courant pour le dossier spécifié (seulement en mode utilisateur)
- `pwd`: Affiche le répertoire courant (seulement en mode utilisateur)
- `touch <path>`: Crée un fichier
- `mkdir <path>`: Crée un dossier
- `rm <path>`: Supprime un dossier ou fichier
- `cp <src> <dst>`: Copie un fichier
- `mv <src> <dst>`: Deplace un fichier
- `upload <path>`: Télécharge un fichier sur l'appareil (seulement en mode programme)
- `download <path>`: Télécharge un fichier depuis l'appareil

# Envoi d'une commande en mode programme
L'appareil est prêt à recevoir des instructions dès que les caractères `>>>` sont affichés (Si l'echo est désactivé, l'invité affichera `>!>`). Après ca, toutes les entrées suivantes seront considérées comme des entrées de commande.

L'hôte ne doit pas oublier de prefixer la commande avec `~`. Pour terminer la commande, un retour à la ligne (0x0C) ou le caractère nul (0x00) peuvent être utilisés. Si le retour à la ligne est utilisé, il sera répété, mais pas le caractère nul.

Dès que l'hôte est près à commencer l'échange, il écrira `OK` (0x4F4B), ou `KO` (0x4B4F) si la commande a échoué à l'initialisation. Si elle échoue, l'échange sera aussitôt annulé et l'invité réapparaitra.

# Echanges en mode programme
Pour chaque commande compatible programme, le descriptif suivant indiquera un échange type.

## `info`
| Minimum 23 octets |
|-|

**L'appareil envoie:**
- Version du firmware (2 octets)
- Indicateur du hardware utilisé (8 octets, réservé)
- Nom d'hôte (taille variable, chaîne de caractères terminée par 0x00)
- Adresse MAC Bluetooth (6 octets)
- Adresse MAC WiFi (6 octets)


## `info hostname`
| Minimum 1 octet |
|-|

**L'appareil envoie:**
- Nom d'hôte (taille variable, chaîne de caractères terminée par 0x00)


## `info mac`
| 12 octets |
|-|

**L'appareil envoie:**
- Adresse MAC Bluetooth (6 octets)
- Adresse MAC WiFi (6 octets)


## `info mac <component>`
| 6 octets |
|-|

**L'appareil envoie:**
- Adresse MAC du composant demandé (6 octets)


## `info version`
| 2 octets |
|-|

**L'appareil envoie:**
- Version du firmware (2 octets)

## `info hardware`
| 8 octets |
|-|

**L'appareil envoie:**
- Indicateur du hardware utilisé (8 octets, réservé)


## `apps`
| Minimum 2 octets |
|-|

**L'appareil envoie:**
- Nombre d'application installées (2 octets)
- Pour chaque application:
  - AppID (taille variable, chaîne de caractères terminée par 0x00)


## `apps install`
| Bidirectionnel |
|-|

**L'hôte envoie:**
- Chemin d'accès du fichier/dossier (taille variable, chaîne de caractères terminée par 0x00)
- Flags d'installation (1 octet)
  - Format du packet (2 bits)
    - `0b00`: Dossier
    - `0b01`: Archive tar
    - `0b10`: Archive tar.gz
    
**L'appareil répond:**
- Statut (1 octet)
  - `0x00`: Succès de l'installation
  - `0x01`: Erreur dans la décompression
  - `0x02`: Erreur dans l'extraction de l'archive
  - `0x10`: Manifest invalide


## `apps show <appid>`
| Minimum 2 octets |
|-|

**L'appareil envoie:**
- Taille du manifest (2 octets)
- Manifest (X octets)

## `apps launch <appid>`
| 1 octet |
|-|

**L'appareil envoie:**
- Statut (1 octet)
  - `0x00`: Succès du démarrage de l'application
  - `0x01`: Application introuvable
  - `0x02`: Appareil occupé


## `apps remove <appid>`
| 1 octet |
|-|

**L'appareil envoie:**
- Statut (1 octet)
  - `0x00`: Succès de la désinstallation
  - `0x01`: Application introuvable


## `files ls <path>`
| Minimum 3 octets |
|-|

> Même le statut est égal à autre chose que `0x00`, le nombre de fichiers doit être écrit et égal à 0.

**L'appareil envoie:**
- Statut (1 octet)
  - `0x00`: Succès
  - `0x01`: Dossier introuvable
  - `0x02`: Accès restreint
- Nombre d'entrées (2 octets)
- Pour chaque fichier:
  - Flags (1 octet)
    - Est un dossier (1 bit)
    - Est corrompu (1 bit)
  - Taille du fichier (4 octets)
  - Nom (taille variable, chaîne de caractères terminée par 0x00)


## `files mkdir <path>`
| 1 octet |
|-|

**L'appareil envoie:**
- Statut (1 octet)
  - `0x00`: Succès
  - `0x01`: Dossier introuvable
  - `0x02`: Accès restreint


## `files touch <path>`
| 1 octet |
|-|

**L'appareil envoie:**
- Statut (1 octet)
  - `0x00`: Succès
  - `0x01`: Dossier introuvable
  - `0x02`: Accès restreint


## `files rm <path>`
| 1 octet |
|-|

**L'appareil envoie:**
- Statut (1 octet)
  - `0x00`: Succès
  - `0x01`: Fichier introuvable
  - `0x02`: Accès restreint

## `files cp <src> <dst>`
| 1 octet |
|-|

**L'appareil envoie:**
- Statut (1 octet)
  - `0x00`: Succès
  - `0x01`: Fichier/dossier source introuvable
  - `0x02`: Accès restreint au fichier source
  - `0x03`: Fichier/dossier cible existe déjà
  - `0x04`: Accès restreint au fichier cible


## `files cp <src> <dst>`
| 1 octet |
|-|

**L'appareil envoie:**
- Statut (1 octet)
  - `0x00`: Succès
  - `0x01`: Fichier/dossier source introuvable
  - `0x02`: Accès restreint au fichier source
  - `0x03`: Fichier/dossier cible existe déjà
  - `0x04`: Accès restreint au fichier cible


## `files upload <path>`
| Bidirectionnel |
|-|

**L'hôte envoie:**
- Taille totale du fichier (4 octets)
- Nombre de blocs (4 octets)

**L'appareil répond:**
- Statut (1 octet)
  - `0x00`: Prêt
  - `0x01`: Le fichier existe déjà
  - `0x02`: Accès restreint au dossier

> Si un autre statut que `0x00` est envoyé ici, l'échange doît s'arrêter.

**Pour chaque bloc, l'hôte envoie:**
- Taille du bloc (2 octets)
- Contenu du bloc (X octets)
- CRC32 du contenu du bloc (4 octets)

**Pour chaque bloc, l'appareil envoie:**
- Statut (1 octet)
  - `0x00`: Succès de la réception
  - `0x01`: Erreur. Le bloc doit être réenvoyé.
  - `0x02`: Erreur. L'échange doît être interrompu.


## `files download <path>`
| Bidirectionnel |
|-|

**L'appareil envoie:**
- Taille totale du fichier (4 octets)
- Nombre de blocs (4 octets)

**L'hôte répond:**
- Statut (1 octet)
  - `0x00`: Prêt
  - `0x01`: Annuler l'échange

**Pour chaque bloc, l'appareil envoie:**
- Taille du bloc (2 octets)
- Contenu du bloc (X octets)
- CRC32 du contenu du bloc (4 octets)

**Pour chaque bloc, l'hôte envoie:**
- Statut (1 octet)
  - `0x00`: Succès de la réception
  - `0x01`: Erreur. Le bloc doit être réenvoyé.
  - `0x02`: Erreur. L'échange doît être interrompu.