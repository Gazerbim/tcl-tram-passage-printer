#include <LiquidCrystal.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>

// ─── Configuration ────────────────────────────────────────────────
const char* WIFI_SSID     = "WIFI_SSID";
const char* WIFI_PASSWORD = "WIFI_PASSWORD";
const char* GL_USER       = "GRAND_LYON_USER";
const char* GL_PASS       = "GRAND_LYON_PASSWORD";
const int   ARRET_ID      = 32135; // id de l'arret de tram à surveiller

const unsigned long REFRESH_INTERVAL    = 30000;
const unsigned long WIFI_RETRY_INTERVAL = 15000;

// Bouton ON/OFF
#define BOUTON_PIN  15       // GPIO15 (D8) 
#define RETRO_PIN   0        // GPIO0  (D3)
#define DEBOUNCE_MS 250

bool systemeActif        = true;
bool dernierEtatBouton   = HIGH;
unsigned long dernierAppui = 0;

// LCD : LiquidCrystal(RS, E, D4, D5, D6, D7) 
// LiquidCrystal lcd(RS, E, D4, D5, D6, D7)
// D1=GPIO5, D2=GPIO4, D5=GPIO14, D6=GPIO12, D7=GPIO13, D0=GPIO16
LiquidCrystal lcd(5, 4, 14, 12, 13, 16);

// Variables globales 
unsigned long lastRefresh   = 0;
unsigned long lastWifiRetry = 0;
String next1 = "--:--";
String next2 = "--:--";

// 
String base64Encode(const String& input);


// Lecture du bouton avec anti-rebond
// Retourne true si un appui vient d'être détecté
bool boutonAppuye() {
  bool etat = digitalRead(BOUTON_PIN);
  unsigned long now = millis();

  // GPIO15 : pull-down -> appui = HIGH
  if (etat == HIGH && dernierEtatBouton == LOW && (now - dernierAppui) > DEBOUNCE_MS) {
    dernierAppui = now;
    dernierEtatBouton = etat;
    return true;
  }
  dernierEtatBouton = etat;
  return false;
}

// gestion LCD
void eteindreLCD() {
  digitalWrite(RETRO_PIN, LOW);  // coupe le retroeclairage
  lcd.noDisplay();
}

void allumerLCD() {
  digitalWrite(RETRO_PIN, HIGH); // rallume le retroeclairage
  lcd.display();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(next1);
  lcd.setCursor(0, 1);
  lcd.print(next2);
}

// Syncro NTP
void syncNTP() {
  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
  int t = 0;
  while (time(nullptr) < 1000000000UL && t < 20) {
    ESP.wdtFeed();
    delay(500);
    t++;
  }
}

// parse l'heure de passage du tram
time_t parseHeurePassage(const char* heureStr) {
  struct tm tm = {};
  int y, mo, d, h, mi, s;
  if (sscanf(heureStr, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6) {
    tm.tm_year = y - 1900;
    tm.tm_mon  = mo - 1;
    tm.tm_mday = d;
    tm.tm_hour = h;
    tm.tm_min  = mi;
    tm.tm_sec  = s;
    tm.tm_isdst = -1;
    return mktime(&tm);
  }
  return 0;
}

// donne le délai avant le départ du tram
String formatDelai(long secondes) {
  if (secondes < -60) return "Parti";
  if (secondes < 60)  return "Proche";
  long minutes = secondes / 60;
  if (minutes >= 60) {
    time_t t = time(nullptr) + secondes;
    struct tm* tm = localtime(&t);
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
    return String(buf);
  }
  return String(minutes) + " min";
}

// S'exécute avant setup(), allume le rétroéclairage immédiatement
void initVariant() {
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH);
}

// gestion du wifi
void gererWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  unsigned long now = millis();
  if (now - lastWifiRetry < WIFI_RETRY_INTERVAL) return;
  lastWifiRetry = now;

  Serial.println(F("[WiFi] Reconnexion..."));
  WiFi.disconnect();
  yield();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 20) {
    ESP.wdtFeed();
    delay(500);
    t++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("[WiFi] Reconnecté"));
    syncNTP();
  }
}

// ─────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  ESP.wdtEnable(8000);

  pinMode(BOUTON_PIN, INPUT);
  pinMode(RETRO_PIN, OUTPUT);
  digitalWrite(RETRO_PIN, HIGH); // rétroéclairage allumé avant lcd.begin()

  lcd.begin(16, 2);

  lcd.begin(16, 2);
  lcd.print("Connexion WiFi..");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 20) {
    ESP.wdtFeed();
    delay(500);
    t++;
  }

  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Connecté : " + WiFi.localIP().toString());
    lcd.print("WiFi OK!");
    syncNTP();
    delay(500);
  } else {
    Serial.println(F("[WiFi] Echec connexion"));
    lcd.print("WiFi ERREUR");
  }
}

// gestion de l'url
String buildURL() {
  return String(F("https://data.grandlyon.com/fr/datapusher/ws/rdata/"))
       + F("tcl_sytral.tclpassagearret/all.json")
       + F("?compact=false&maxfeatures=20")
       + F("&field=id&value=") + String(ARRET_ID);
}

// recuperation de la data
String fetchData(const String& url) {
  ESP.wdtFeed();
  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(512, 512);
  HTTPClient http;
  http.setTimeout(8000);

  String authHeader = "Basic " + base64Encode(String(GL_USER) + ":" + String(GL_PASS));
  http.begin(client, url);
  http.addHeader("Authorization", authHeader);
  http.addHeader("Accept", "application/json");

  ESP.wdtFeed();
  int httpCode = http.GET();
  String payload = "";
  if (httpCode == HTTP_CODE_OK) {
    payload = http.getString();
  } else {
    Serial.println("[API] Erreur HTTP : " + String(httpCode));
  }
  http.end();
  ESP.wdtFeed();
  return payload;
}

// parse le json pour récupérer les valeurs qui nous intéressent
void parsePassages(const String& payload) {
  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print(F("[JSON] Erreur : "));
    Serial.println(err.c_str());
    return;
  }

  time_t maintenant = time(nullptr);
  JsonArray values = doc["values"].as<JsonArray>();

  time_t meilleureHeure1 = LONG_MAX, meilleureHeure2 = LONG_MAX;
  String bestLigne1 = "", bestLigne2 = "";
  String bestDelai1 = "", bestDelai2 = "";

  for (JsonObject row : values) {
    const char* heure = row["heurepassage"];
    const char* ligne = row["ligne"];
    if (!heure || !ligne) continue;

    time_t t = parseHeurePassage(heure);
    if (t == 0 || t < maintenant - 60) continue;

    if (t < meilleureHeure1) {
      meilleureHeure2 = meilleureHeure1; bestLigne2 = bestLigne1; bestDelai2 = bestDelai1;
      meilleureHeure1 = t; bestLigne1 = String(ligne);
      bestDelai1 = formatDelai((long)(t - maintenant));
    } else if (t < meilleureHeure2) {
      meilleureHeure2 = t; bestLigne2 = String(ligne);
      bestDelai2 = formatDelai((long)(t - maintenant));
    }
  }

  if (bestLigne1.length() == 0) {
    next1 = "Aucun tram"; next2 = "-----";
  } else {
    next1 = bestLigne1 + " " + bestDelai1;
    next2 = (bestLigne2.length() > 0) ? bestLigne2 + " " + bestDelai2 : "-----";
    if (next1.length() > 16) next1 = next1.substring(0, 16);
    if (next2.length() > 16) next2 = next2.substring(0, 16);
  }
  Serial.println("[TCL] " + next1 + " / " + next2);
}

// affiche les infos contenues dans next1 et next2 sur le lcd
void afficherLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(next1);
  lcd.setCursor(0, 1);
  lcd.print(next2);
}

String base64Encode(const String& input) {
  const char* b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String output = "";
  int i = 0;
  unsigned char buf3[3], buf4[4];
  int len = input.length();
  const char* data = input.c_str();
  while (len--) {
    buf3[i++] = *data++;
    if (i == 3) {
      buf4[0] = (buf3[0] & 0xfc) >> 2;
      buf4[1] = ((buf3[0] & 0x03) << 4) + ((buf3[1] & 0xf0) >> 4);
      buf4[2] = ((buf3[1] & 0x0f) << 2) + ((buf3[2] & 0xc0) >> 6);
      buf4[3] =  buf3[2] & 0x3f;
      for (i = 0; i < 4; i++) output += b64chars[buf4[i]];
      i = 0;
    }
  }
  if (i) {
    for (int j = i; j < 3; j++) buf3[j] = '\0';
    buf4[0] = (buf3[0] & 0xfc) >> 2;
    buf4[1] = ((buf3[0] & 0x03) << 4) + ((buf3[1] & 0xf0) >> 4);
    buf4[2] = ((buf3[1] & 0x0f) << 2) + ((buf3[2] & 0xc0) >> 6);
    for (int j = 0; j < i + 1; j++) output += b64chars[buf4[j]];
    while (i++ < 3) output += '=';
  }
  return output;
}

// loop
void loop() {
  // Gestion bouton on/off
  if (boutonAppuye()) {
    systemeActif = !systemeActif;
    if (systemeActif) {
      Serial.println(F("[BTN] Allumage"));
      allumerLCD();
      lastRefresh = 0; // force un rafraîchissement immédiat
    } else {
      Serial.println(F("[BTN] Extinction"));
      eteindreLCD();
    }
  }

  // Si éteint stopper le rafraichissement
  if (!systemeActif) {
    yield();
    return;
  }

  // sinon rafraichir
  gererWifi();
  yield();

  unsigned long now = millis();
  if (now - lastRefresh >= REFRESH_INTERVAL || lastRefresh == 0) {
    lastRefresh = now;
    if (WiFi.status() == WL_CONNECTED) {
      String payload = fetchData(buildURL());
      if (payload.length() > 0) parsePassages(payload);
      afficherLCD();
    }
  }

  yield();
}
