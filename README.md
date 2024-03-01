# Protocole UART PaxOS

# Usage de ce dépôt GitHub
Ce dépôt contient 

# Pratiques de mise en oeuvre
Ce protocole sera peut-être étendu plus tard, ce qui justifie la présence de versions.

Ce protocole est basé la plupart du temps sur un modèle de Demande/Réponse.
Les identifiants utilisables par les packets suivant ce modèle sont situés entre `0x00` et `0xEF`.
Les packets réponses doivent impérativement utiliser la même version que le packet de demande le précédent.

Les magics numbers font toujours 2 octets: le premier octet différencie la version et le 2ème le packet différencie le type de packet (base, followup, microréponse)

Si une donnée non compréhensible intervient dans l'échange, ou qu'un timeout est atteint, le destinataire doît envoyer une microréponse de version 1, avec un statut de `0xA2`. Il est conseillé d'attendre la fin de la récéption de données après l'envoi de cette microréponse.

# Configuration UART
- Fréquence: 115200 Hz
- Pas de parité
- Données de 8 bits
- Stop de 1 bit
- Flow control: CTS/RTS

# Format des packets
## Version 1
### Packet de base
- Magic number = `0x91c1`
- ID: 1 octet -> Identifie le sujet du message
- Longueur du body: 2 octets
- Following: 4 octets -> Indique le nombre de packets de type followup
- Body: X octets -> Il est attendu qu'un body fasse une taille maximale de 2048 octets, et que si des packets de type followup sont attendus, il atteigne sa taille maximale. Du aux capacités de mémoire limitées de l'appareil recevant, il est envisagable que l'appareil concerné rejette un packet ne suivant pas ces instructions.
- Signature de sureté: 1 octet -> Un CRC-8/CCITT de toutes les valeurs ci-dessus. Si cette signature est incorrecte, une microréponse d'erreur doit être envoyée.

### Packet de type followup
- Magic number = `0x91c2`
- Longueur du body: 2 octets
- Body: X octets
- Signature de sureté: 1 octet -> Un CRC-8/CCITT de toutes les valeurs ci-dessus. Si cette signature est incorrecte, une microréponse d'erreur doit être envoyée.

### Microréponse à un packet de base ou de type followup
Les microréponses sont attendues dans tout les cas, mais leur transport correct n'est pas vérifié. Si une microréponse n'est pas transmise pour quelque raison après une limite de temps, le destinataire doit réessayer l'envoi du packet avant de se déclarer en échec.
Les erreurs statuées par les microréponses ne doivent uniquement être des erreurs pendant le transport. Si le format d'un corps n'est pas lisible, l'échange ne doit pas être répondu.

- Magic number = `0x91c3`
- ID du packet répondu: 1 octet
- Statut: 1 octet
  - `0xA0`: Le packet est arrivé sans problème. Si un packet de type followup est attendu ensuite, cette réponse demande son envoi.
  - `0xA1`: Une erreur est survenue, le destinataire attend un nouvel essai.
  - `0xA2`: Une erreur est survenue, le destinataire n'attend pas de nouvel essai.

# Groupes d'indentifiants de packets
- `0x00 -> 0x0F`: Demandes d'informations
- `0x10 -> 0x1F`: Installation des applications
- `0x20 -> 0x2F`: Mise à jour logicielles
- `0x30 -> 0x3F`: Gestion des fichiers à distance
- `0x60 -> 0x6F`: Transferts de données de masse

# Contenu des packets
## Packet METADATA_REQ (`0x00`)
| Depuis la version 1 | Provient de l'hôte | Sans données |
|-|-|-|

Demande à l'appareil ses métadonnées publiques.

## Packet METADATA_RES (`0x01`)
| Depuis la version 1 | Provient de l'appareil | 20 octets |
|-|-|-|

Répond à l'hôte les métadonnées publiques de l'appareil.

- Nom d'hôte: 16 octets
- Version majeure de PaxOS: 2 octets
- Version mineure de PaxOS: 2 octets

## Packet CHECK_APP_REQ (`0x02`)
| Depuis la version 1 | Provient de l'hôte | Contient des données |
|-|-|-|

Demande à l'appareil si une application tierce est installée.

- Taille de l'identifiant de l'application: 2 octets
- Identifiant de l'application: X octets

## Packet CHECK_APP_RES (`0x03`)
| Depuis la version 1 | Provient de l'appareil | 1 octet |
|-|-|-|

Répond à l'hôte sur le statut de l'installation d'une application.

- Statut: 1 octet
  - `0x00`: Non installé
  - `0x01`: Installé

## Packet LIST_APPS_REQ (`0x04`)
| Depuis la version 1 | Provient de l'hôte | 1 octet |
|-|-|-|

Demande à l'appareil une liste des applications tierces installées.
Pour que cette réponse soit envoyée, l'utilisateur doit effectuer une confirmation sur l'écran de l'appareil.

- Informations demandées: 1 octet
  - Bit 1: Présence de la taille de l'application
  - Bit 2: Présence de la date d'installation
  - Si des bits superflus sont présent dans l'octet, ils doivent être ignorés. Cela permet l'usage de la valeur `0xFF` pour demander toutes les informations optionnelles.

## Packet LIST_APPS_RES (`0x05`)
| Depuis la version 1 | Provient de l'appareil | Contient des données |
|-|-|-|

Répond à l'hôte la liste des applications tierces installées.

- Nombre d'applications: 4 octets
- Pour chaque application:
  - Taille de l'identifiant de l'application: 2 octets
  - Identifiant de l'application: X octets
  - *Taille de l'application: 4 octets*
  - *Date d'installation: 4 octets*

## Packet INSTALL_APP_REQ (`0x10`)
| Depuis la version 1 | Provient de l'hôte | Contient des données |
|-|-|-|

Propose à l'appareil l'installation d'une application tierce.

- Taille totale de l'application: 4 octets
- Taille du manifest: 4 octets
- Manifest: X octets

## Packet INSTALL_APP_RES (`0x11`)
| Depuis la version 1 | Provient de l'appareil | 2 octets |
|-|-|-|

Réponse de l'appareil concernant la demande d'installation d'une application tierce.
Pour que cette réponse soit envoyée, l'utilisateur doit effectuer une confirmation sur l'écran de l'appareil, et l'appareil doit vérifier qu'il dispose des capacités pour stocker cette installation.
Noter que le manifest ne sera pas envoyé pendant cette installation, il devra être écrit dans la mémoire à partir du contenu du [packet `0x10`](#packet-install_app_req-0x10) précédemment envoyé.

- Identifiant d'upload: 2 octets

## Packet END_INSTALL (`0x12`)
| Depuis la version 1 | Provient de l'hôte | Sans données |
|-|-|-|

Annonce la fin d'une installation.

## Packet END_INSTALL_STATUS (`0x13`)
| Depuis la version 1 | Provient de l'appareil | 1 octet |
|-|-|-|

Indique l'échec ou la réussite d'une installation.
Quel que soit l'issue, l'installation s'arrête. Ce packet n'a aucune repercussion logicielle, il est juste destiné à un retour utilisateur.

- Résultat: 1 octet
  - `0x00`: Succès de l'installation.
  - `0x01`: Echec de l'installation, quelque soit la raison. (fichiers manquants, annulation de l'utilisateur...)

## Packet REMOVE_APP_REQ (`0x14`)
| Depuis la version 1 | Provient de l'hôte | Contient des données |
|-|-|-|

Propose à l'appareil la suppression d'une application tierce.

- Taille de l'identifiant de l'application: 2 octets
- Identifiant de l'application: X octets

## Packet REMOVE_APP_RES (`0x15`)
| Depuis la version 1 | Provient de l'appareil | 1 octet |
|-|-|-|

Réponse de l'appareil concernant la demande de suppression d'une application tierce.
Pour que cette réponse soit envoyée, l'utilisateur doit effectuer une confirmation sur l'écran de l'appareil.
Si la suppression est acceptée, cette réponse sera envoyée 2 fois: Pour avertir du démarrage de la suppression, et pour avertir de sa fin.
Ce packet n'a aucune repercussion logicielle, il est juste destiné à un retour utilisateur.

- Statut: 1 octet
  - `0x00`: Suppression acceptée.
  - `0x01`: Suppression terminée.
  - `0x10`: Application introuvable.
  - `0xFF`: Suppression refusée.

## Packet FILE_UPLOAD (`0x60`)
| Depuis la version 1 | Provient de l'hôte | Contient des données |
|-|-|-|

Envoi d'un fichier de l'hôte vers l'appareil.

- Identifiant d'upload: 2 octets
- Taille du nom de fichier: 2 octets
- Nom du fichier: X octets -> Pour les fichiers dans des sous-dossiers, le nom du fichier fera office de chemin relatif à partir de la racine de l'installation. Les sous-dossiers ne sont pas annoncés et doivent être créés automatiquement.
- Taille du fichier: 4 octets
- Contenu du fichier: X octets
- Signature de sureté: 8 octets -> CRC-32 du contenu du fichier seulement.

## Packet FILE_UPLOAD_DONE (`0x61`)
| Depuis la version 1 | Provient de l'appareil | 1 octet |
|-|-|-|

Indique l'échec ou la réussite du transfert d'un fichier.

- Erreur: 1 octet
  - `0x00`: Succès du transfert. L'hôte doit passer au fichier suivant.
  - `0x01`: La signature de sureté ne correspond pas. L'hôte doit réessayer ce transfert.
  - `0x02`: La mémoire des fichiers uploadés jusqu'à maintenant dépasse celle annoncée dans le [packet `0x10`](#packet-install_app_req-0x10) précédemment envoyé. L'installation s'arrête.
  - `0x03`: L'identifiant d'upload n'est pas reconnu ou l'installation s'est déjà terminée. L'installation s'arrête.
  - `0x04`: Le nom du fichier est invalide. L'installation s'arrête.
  - `0x05`: Le délai d'attente à été dépassé pour l'envoi du packet (maximum 5s d'attente entre chaque octet). L'hôte doit réessayer ce transfert.
  - `0xFF`: Autre erreur venant de l'appareil. L'installation s'arrête.

## Packet FILE_DOWNLOAD (`0x62`)
| Depuis la version 1 | Provient de l'hôte | Contient des données |
|-|-|-|

Demande de transfert d'un fichier de l'appareil vers l'hôte.

- Identifiant de download: 2 octets
- Taille du nom de fichier: 2 octets
- Nom du fichier: X octets -> Pour les fichiers dans des sous-dossiers, le nom du fichier fera office de chemin relatif.

## Packet FILE_DOWNLOAD_DATA (`0x63`)
| Depuis la version 1 | Provient de l'appareil | Contient des données |
|-|-|-|

Renvoie le fichier demandé par l'hôte.

- Taille du fichier: 4 octets
- Contenu du fichier: X octets
- Signature de sureté: 8 octets -> CRC-32 du contenu du fichier seulement

## Packet PROTOCOL_VERSION_REQ (`0x7C`)
| Depuis la version 1 | Provient de l'hôte | Sans données |
|-|-|-|

Demande à l'appareil la dernière version du protocole compatible.
Ce packet est toujours envoyé dans le format de la version 1 du protocole.

## Packet PROTOCOL_VERSION_RES (`0x7D`)
| Depuis la version 1 | Provient de l'appareil | 1 octet |
|-|-|-|

Réponse au [packet `0x7C`](#packet-protocol_version_req-0x7c)

- Version: 1 octet

## Packet ANNOUNCE_REVERSE_COMPATIBILITY (`0x7E`)
| Depuis la version 1 | Spécial | Sans données |
|-|-|-|

Annonce la compatibilité pour les packets non ordonnés.
