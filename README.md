# Afficheur des horaires de tram TCL

Petit projet basé sur un **ESP8266 et un écran LCD 16×2** permettant d'afficher les prochains passages de tram à un arrêt de la Métropole de Lyon.

Les horaires sont récupérés automatiquement depuis l'API Open Data de la Métropole de Lyon. Un bouton permet d'allumer ou d'éteindre l'écran et son rétroéclairage.

## Fonctionnalités

- Connexion au réseau Wi-Fi.
- Récupération des horaires depuis l'API TCL.
- Affichage des deux prochains passages sur un écran LCD 16×2.
- Actualisation automatique toutes les 30 secondes.
- Reconnexion au Wi-Fi en cas de perte de connexion.
- Synchronisation de l'heure via un serveur NTP.
- Activation ou désactivation de l'affichage à l'aide d'un bouton.
- Affichage du délai avant le prochain passage :
  - En minutes lorsque le tram est attendu dans moins d'une heure.
  - Sous forme d'heure lorsque le délai atteint une heure ou plus.
  - Avec un message lorsque le tram est proche ou déjà parti.

## Matériel nécessaire

- Un ESP8266, par exemple un NodeMCU ou un Wemos D1 mini.
- Un écran LCD 16×2 compatible avec la bibliothèque `LiquidCrystal`.
- Un bouton poussoir.
- Des fils de connexion.
- Une alimentation adaptée à l'ESP8266.
- Un réseau Wi-Fi disposant d'un accès à Internet.

**Attention :** utilisation d'un écran LCD en mode parallèle, et non d'un écran équipé d'une interface I²C.

## Câblage

### Écran LCD

| Écran LCD | ESP8266 |
|---|---|
| RS | D1 (GPIO5) |
| E | D2 (GPIO4) |
| D4 | D5 (GPIO14) |
| D5 | D6 (GPIO12) |
| D6 | D7 (GPIO13) |
| D7 | D0 (GPIO16) |
| VSS | GND |
| VDD | Alimentation adaptée |
| RW | GND |
| VO | Potentiomètre de contraste |
| A / LED+ | Circuit de rétroéclairage |
| K / LED− | GND |

Vérification de la documentation de l'écran pour les broches d'alimentation et de rétroéclairage.

### Bouton ON/OFF

| Élément | ESP8266 |
|---|---|
| Bouton | D8 (GPIO15) |
| Autre borne | 3,3 V |

Maintien de GPIO15 à l'état bas au démarrage de l'ESP8266. Respect des contraintes de démarrage de cette broche lors de la conception du montage.

### Rétroéclairage

Commande du rétroéclairage par la broche D3 (GPIO0).

**Attention :** maintien de GPIO0 à l'état haut au démarrage normal. Éviter le branchement direct d'une charge importante sur cette broche et privilégier un transistor ou un circuit de commande adapté si nécessaire.

## Installation

### 1. Installation de l'environnement Arduino

Installation de l'IDE Arduino, puis ajout du support des cartes ESP8266 si nécessaire.

Dans le gestionnaire de cartes, recherche et installation du paquet ESP8266 fourni par la communauté ESP8266.

Sélection de la carte correspondant au matériel utilisé.

### 2. Installation des bibliothèques

Le programme utilise les bibliothèques suivantes :

- `LiquidCrystal` : commande de l'écran LCD.
- `ESP8266WiFi` : connexion Wi-Fi.
- `ESP8266HTTPClient` : requêtes HTTP et HTTPS.
- `WiFiClientSecure` : connexions HTTPS.
- `ArduinoJson` : lecture des données JSON.

Les bibliothèques propres à l'ESP8266 sont généralement incluses avec le paquet de cartes. Installation de `ArduinoJson` depuis le gestionnaire de bibliothèques de l'IDE Arduino si nécessaire.

### 3. Configuration du programme

Modification des paramètres situés au début du fichier :

```cpp
const char* WIFI_SSID     = "WIFI_SSID";
const char* WIFI_PASSWORD = "WIFI_PASSWORD";
const char* GL_USER       = "GRAND_LYON_USER";
const char* GL_PASS       = "GRAND_LYON_PASSWORD";
const int   ARRET_ID      = 32135;
```

Remplacement des valeurs par :

- `WIFI_SSID` : le nom du réseau Wi-Fi.
- `WIFI_PASSWORD` : le mot de passe du réseau Wi-Fi.
- `GL_USER` : l'identifiant d'accès à l'API, si requis.
- `GL_PASS` : le mot de passe associé, si requis.
- `ARRET_ID` : l'identifiant de l'arrêt à surveiller.

Vérification des modalités d'accès actuelles à la plateforme Open Data de la Métropole de Lyon et utilisation des identifiants correspondants, si nécessaire.

### 4. Compilation et téléversement

1. Connexion de l'ESP8266 à l'ordinateur via USB.
2. Sélection de la carte et du port série appropriés dans l'IDE Arduino.
3. Compilation du programme.
4. Téléversement du programme sur la carte.
5. Ouverture du moniteur série à 115200 bauds pour consulter les messages de diagnostic.

## Fonctionnement

### Démarrage

Au démarrage, tentative de connexion de l'ESP8266 au réseau Wi-Fi. Une fois connecté, synchronisation de l'horloge via NTP.

Affichage des prochains passages récupérés depuis l'API.

### Affichage des horaires

Présentation des deux prochains passages détectés sur les deux lignes de l'écran.

Exemple :

```text
T1 5 min
T3 18 min
```

Cet affichage est illustratif : les lignes et les délais dépendent des données réellement renvoyées par l'API.

Ignorance des passages dont l'heure est dépassée, avec une marge d'une minute.

### Actualisation

Actualisation des données toutes les 30 secondes.

En cas de perte du Wi-Fi, tentative de rétablissement périodique de la connexion.

### Bouton ON/OFF

Basculement entre deux états à chaque appui sur le bouton :

- **Actif :** rétroéclairage allumé et actualisation des horaires.
- **Inactif :** écran éteint et suspension des requêtes de mise à jour.

Déclenchement d'une nouvelle actualisation lors du rallumage de l'écran.

## API utilisée

Interrogation du service de données de la Métropole de Lyon :

`https://data.grandlyon.com/fr/datapusher/ws/rdata/tcl_sytral.tclpassagearret/all.json`

Filtrage des résultats à l'aide de l'identifiant d'arrêt configuré.

Champs utilisés dans la réponse JSON :

- `heurepassage` : date et heure du passage.
- `ligne` : nom ou numéro de la ligne.

Vérification de la documentation officielle de la plateforme Open Data de la Métropole de Lyon en cas de problème lié à la structure des données ou aux modalités d'authentification.

## Dépannage

| Problème | Vérifications |
|---|---|
| Écran vide | Vérification du câblage, de l'alimentation et du contraste. |
| Échec de connexion Wi-Fi | Vérification du SSID, du mot de passe et de la couverture réseau. |
| Aucun tram affiché | Vérification de l'identifiant de l'arrêt et de la réponse de l'API. |
| Erreur HTTP | Vérification de la connexion Internet, des identifiants API et de la disponibilité du service. |
| Erreur JSON | Vérification du format et de la structure des données renvoyées par l'API. |
| Heure incorrecte | Vérification de la synchronisation NTP et du fuseau horaire. |
| ESP8266 qui ne démarre pas | Vérification des circuits connectés aux broches GPIO0 et GPIO15. |
| Bouton inopérant | Vérification du câblage et de l'état logique de GPIO15. |

Consultation des messages de diagnostic dans le moniteur série.

## Sécurité

- Protection des identifiants Wi-Fi et API contre toute publication dans un dépôt public.
- Évitement du stockage des mots de passe dans le code source versionné.
- Remplacement de `client.setInsecure()` par une vérification du certificat du serveur pour une utilisation durable. La configuration actuelle désactive la vérification du certificat HTTPS et réduit la sécurité de la connexion.

## Licence
Licence MIT
