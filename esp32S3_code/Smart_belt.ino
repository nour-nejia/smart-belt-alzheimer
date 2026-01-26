// ============================================================
// 🚀 SYSTÈME COMPLET DE DÉTECTION DE CHUTE
// ============================================================
// Fonctionnalités: 
// - Détection de chute avec ML (MPU6050)
// - Bouton d'urgence physique (PIN 7)
// - Géolocalisation Google API
// - Affichage LCD I2C
// - LED RGB pour statut
// - Envoi d'email via Blynk
// - Application Blynk pour monitoring
// ============================================================

// ==================== BLYNK CONFIG (DOIT ÊTRE EN PREMIER) ====================
#if __has_include("secrets.h")
#include "secrets.h"
#else
// Fallback placeholders. Create esp32S3_code/secrets.h to override.
#define BLYNK_TEMPLATE_ID "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "YOUR_TEMPLATE_NAME"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
#define GOOGLE_API_KEY "YOUR_GOOGLE_API_KEY"
#endif

#define BLYNK_PRINT Serial

// ==================== INCLUDES ====================
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <LiquidCrystal_I2C.h>

#include "fall_detection_model.h"
#include "scaler_config.h"

// ==================== WIFI CONFIG ====================
const char* ssid = WIFI_SSID;
const char* pass = WIFI_PASS;

// ==================== PINS CONFIG ====================
// MPU6050
#define SDA_PIN 8
#define SCL_PIN 9
#define INT_PIN 10
// LED RGB
#define LED_RED 4
#define LED_GREEN 5
#define LED_BLUE 6

// Bouton poussoir
#define BUTTON_PIN 7

// ==================== GOOGLE GEOLOCATION API ====================
const char* googleApiKey = GOOGLE_API_KEY;

// Coordonnées de secours
const float FALLBACK_LATITUDE = 36.843583;
const float FALLBACK_LONGITUDE = 10.196667;
const String FALLBACK_LINK = "https://www.google.com/maps?q=36.843583,10.196667";

// ==================== OBJETS GLOBAUX ====================
Adafruit_MPU6050 mpu;
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Changez 0x27 en 0x3F si nécessaire

// ==================== VARIABLES GÉOLOCALISATION ====================
float latitude = 0.0;
float longitude = 0.0;
float accuracy = 0.0;
String googleMapsLink = "";

// ==================== VARIABLES UTILISATEUR (Blynk) ====================
String userName = "";
int userAge = 0;
String userEmail = "";
String userPhoneNumber = "";

// ==================== VARIABLES ÉTAT ====================
const char* states[] = {"IDLE", "WALKING", "FALLING"};
String currentState = "IDLE";

// ==================== VARIABLES BOUTON ====================
int lastButtonState = HIGH;
int currentButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
bool systemReady = false;
int emailCount = 0;

// ==================== VARIABLES ML ====================
unsigned long lastSample = 0;
#define SAMPLE_RATE_MS 100

// ==================== ANTI-SPAM EMAIL ====================
unsigned long lastEmailSent = 0;
#define EMAIL_COOLDOWN 30000  // 30 secondes entre chaque email

// ==================== MESSAGE ALERTE ====================
String alertMessageButton = "URGENCE!  La personne atteinte d'Alzheimer a besoin d'aide.  "
                            "Le bouton d'urgence a ete active. "
                            "Verifiez immediatement sa situation. ";

String alertMessageFall = "CHUTE DETECTEE! La personne est peut-etre en danger. "
                          "Une chute a ete detectee par le systeme. "
                          "Verifiez immediatement sa situation. ";

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);


  // Initialiser LED RGB
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  setLED(0, 0, 0);

  // Initialiser bouton
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialiser LCD
  initLCD();

  // Connexion WiFi
  connectWiFi();

  // Connexion Blynk
  connectBlynk();

  // Synchroniser les données utilisateur
  Blynk.syncVirtual(V4, V5, V7, V8);
  delay(1000);

  // Initialiser I2C pour MPU6050
  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialiser MPU6050
  initMPU6050();


  updateLCDStatus("Geo test.. .");
  String link = getLocation();


  // Lire l'état initial du bouton
  currentButtonState = digitalRead(BUTTON_PIN);
  lastButtonState = currentButtonState;
  // Système prêt
  systemReady = true;


  // LED verte pour indiquer système prêt
  setLED(0, 255, 0);
  updateLCDStatus("Systeme pret");
  Blynk.virtualWrite(V1, "Systeme pret");
  Blynk.virtualWrite(V3, "IDLE");

  delay(1000);
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  Blynk.run();

  // Vérifier connexion WiFi
  if (WiFi.status() != WL_CONNECTED) {
    setLED(255, 255, 0);  // Jaune = problème WiFi
    connectWiFi();
  }

  // Vérifier le bouton physique
  if (systemReady) {
    checkButton();
  }

  // Détection de chute ML
  if (systemReady && millis() - lastSample >= SAMPLE_RATE_MS) {
    lastSample = millis();
    runFallDetection();
  }
}

// ============================================================
// INITIALISATION LCD
// ============================================================
void initLCD() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Demarrage.. .");
}

// ============================================================
// CONNEXION WIFI
// ============================================================
void connectWiFi() {

  updateLCDStatus("WiFi...");
  setLED(255, 255, 0);  // Jaune pendant connexion

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);

    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
 

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi OK!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(1500);
  } else {
 
    setLED(255, 0, 0);  // Rouge = erreur
  }
}

// ============================================================
// CONNEXION BLYNK
// ============================================================
void connectBlynk() {
  updateLCDStatus("Blynk...");

  Blynk.config(BLYNK_AUTH_TOKEN);

  int attempts = 0;
  while (!Blynk.connected() && attempts < 30) {
    Blynk.run();
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (Blynk.connected()) {
 
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Blynk OK!");
    delay(1500);
  } 
}

// ============================================================
// INITIALISATION MPU6050
// ============================================================
void initMPU6050() {
  updateLCDStatus("MPU6050...");

  if (!mpu.begin()) {
    updateLCDStatus("MPU6050 ERROR!");

    // Clignoter rouge en cas d'erreur
    while (1) {
      setLED(255, 0, 0);
      delay(200);
      setLED(0, 0, 0);
      delay(200);
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

}

// ============================================================
// CONTRÔLE LED RGB
// ============================================================
void setLED(int red, int green, int blue) {
  analogWrite(LED_RED, red);
  analogWrite(LED_GREEN, green);
  analogWrite(LED_BLUE, blue);
}

// ============================================================
// MISE À JOUR LCD - STATUS
// ============================================================
void updateLCDStatus(String status) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(status);
}

// ============================================================
// MISE À JOUR LCD - INFOS UTILISATEUR
// ============================================================
void updateLCDUserInfo() {
  lcd.clear();

  // Ligne 1: Nom
  lcd.setCursor(0, 0);
  lcd.print("Nom: ");
  String displayName = userName;
  if (displayName.length() > 11) {
    displayName = displayName.substring(0, 11);
  }
  lcd.print(displayName);

  // Ligne 2: Téléphone
  lcd.setCursor(0, 1);
  lcd.print("Tel: ");
  String displayPhone = userPhoneNumber;
  if (displayPhone.length() > 11) {
    displayPhone = displayPhone.substring(0, 11);
  }
  lcd.print(displayPhone);
}

// ============================================================
// MISE À JOUR LCD - ÉTAT
// ============================================================
void updateLCDState(String state) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Etat: " + state);

  lcd.setCursor(0, 1);
  if (userName.length() > 0) {
    lcd.print(userName. substring(0, 16));
  }
}

// ============================================================
// VÉRIFICATION BOUTON PHYSIQUE
// ============================================================
void checkButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != currentButtonState) {
      currentButtonState = reading;

      // Bouton pressé (LOW car pull-up)
      if (currentButtonState == LOW) {
        onButtonPressed();
      }
    }
  }

  lastButtonState = reading;
}

// ============================================================
// ACTION BOUTON PRESSÉ
// ============================================================
void onButtonPressed() {
  emailCount++;

  // Afficher sur LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ALERTE BOUTON!");
  lcd.setCursor(0, 1);
  lcd.print("Envoi email...");

  // Clignoter LED rouge
  for (int i = 0; i < 3; i++) {
    setLED(255, 0, 0);
    delay(150);
    setLED(0, 0, 0);
    delay(150);
  }

  // Mettre à jour position GPS
  getLocation();

  // Envoyer l'alerte
  sendAlertEmail("BUTTON");

  // Remettre LED verte
  setLED(0, 255, 0);

  delay(2000);
  updateLCDState(currentState);
}

// ============================================================
// DÉTECTION DE CHUTE ML
// ============================================================
void runFallDetection() {
  // Lire les données du capteur
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  // Prédiction ML
  int32_t prediction = fall_model_predict_optimized(
    accel.acceleration.x,
    accel.acceleration.y,
    accel.acceleration. z,
    gyro.gyro.x,
    gyro.gyro.y,
    gyro.gyro.z
  );

  String newState = "";

  switch (prediction) {
    case 0:
      newState = "IDLE";
      setLED(0, 255, 0);  // VERT
      break;
    case 1:
      newState = "WALKING";
      setLED(0, 0, 255);  // BLEU
      break;
    case 2:
      newState = "FALLING";
      setLED(255, 0, 0);  // ROUGE
      triggerFallAlert();
      break;
  }

  // Mettre à jour si l'état a changé
  if (newState != currentState) {
    currentState = newState;

    Blynk.virtualWrite(V3, currentState);
    updateLCDState(currentState);
  }
}

// ============================================================
// ALERTE CHUTE DÉTECTÉE
// ============================================================
void triggerFallAlert() {


  // Afficher sur LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("!!  CHUTE ! !");
  lcd.setCursor(0, 1);
  lcd.print("Alerte envoyee");

  // Envoyer email seulement si le cooldown est passé
  if (millis() - lastEmailSent > EMAIL_COOLDOWN) {
    // Mettre à jour position GPS
    getLocation();

    // Envoyer l'alerte
    sendAlertEmail("FALL");
    lastEmailSent = millis();
  } 

  // Clignoter rouge rapidement
  for (int i = 0; i < 6; i++) {
    setLED(255, 0, 0);
    delay(250);
    setLED(0, 0, 0);
    delay(250);
  }
}

// ============================================================
// ENVOI EMAIL VIA BLYNK
// ============================================================
void sendAlertEmail(String alertType) {

  String message;

  if (alertType == "BUTTON") {
    message = alertMessageButton;
  } else {
    message = alertMessageFall;
  }

  // Ajouter les informations
  message += " | Nom: " + (userName.length() > 0 ? userName : "? ");
  message += " | Age: " + (userAge > 0 ? String(userAge) : "?");
  message += " | Tel:  " + (userPhoneNumber. length() > 0 ? userPhoneNumber : "?");
  message += " | GPS: " + googleMapsLink;

  // Envoyer via Blynk Event
  Blynk.logEvent("button_alert", message);


}

// ============================================================
// GOOGLE GEOLOCATION API (AVEC FALLBACK)
// ============================================================
String getLocation() {
  if (WiFi.status() != WL_CONNECTED) {

    latitude = FALLBACK_LATITUDE;
    longitude = FALLBACK_LONGITUDE;
    accuracy = 100.0;
    googleMapsLink = FALLBACK_LINK;
    return FALLBACK_LINK;
  }


  int numNetworks = WiFi.scanNetworks();

  if (numNetworks == 0) {
    WiFi.scanDelete();
    latitude = FALLBACK_LATITUDE;
    longitude = FALLBACK_LONGITUDE;
    accuracy = 100.0;
    googleMapsLink = FALLBACK_LINK;
    return FALLBACK_LINK;
  }


  // Construire la requête JSON
  DynamicJsonDocument doc(2048);
  doc["considerIp"] = true;

  JsonArray wifiAccessPoints = doc. createNestedArray("wifiAccessPoints");

  int maxNetworks = min(numNetworks, 10);

  for (int i = 0; i < maxNetworks; i++) {
    JsonObject ap = wifiAccessPoints.createNestedObject();
    ap["macAddress"] = WiFi.BSSIDstr(i);
    ap["signalStrength"] = WiFi. RSSI(i);
    ap["channel"] = WiFi.channel(i);
  }

  String requestBody;
  serializeJson(doc, requestBody);

  // Envoyer la requête HTTP
  HTTPClient http;

  String url = "https://www.googleapis.com/geolocation/v1/geolocate?key=";
  url += googleApiKey;

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);

  int httpCode = http.POST(requestBody);

  String result = "";

  if (httpCode == 200) {
    String response = http.getString();

    DynamicJsonDocument responseDoc(1024);
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error) {
      latitude = FALLBACK_LATITUDE;
      longitude = FALLBACK_LONGITUDE;
      accuracy = 100.0;
      googleMapsLink = FALLBACK_LINK;
      result = FALLBACK_LINK;
    } else {
      latitude = responseDoc["location"]["lat"];
      longitude = responseDoc["location"]["lng"];
      accuracy = responseDoc["accuracy"];


      googleMapsLink = "https://www.google.com/maps? q=";
      googleMapsLink += String(latitude, 6);
      googleMapsLink += ",";
      googleMapsLink += String(longitude, 6);

      result = googleMapsLink;
    }
  } else {
    latitude = FALLBACK_LATITUDE;
    longitude = FALLBACK_LONGITUDE;
    accuracy = 100.0;
    googleMapsLink = FALLBACK_LINK;
    result = FALLBACK_LINK;
  }

  http.end();
  WiFi.scanDelete();

  return result;
}

// ============================================================
// BLYNK HANDLERS
// ============================================================

// Bouton V0 → Demande localisation
BLYNK_WRITE(V0) {
  if (param.asInt() == 1) {
    Blynk.virtualWrite(V1, "Recherche GPS...");

    String link = getLocation();
    Blynk.virtualWrite(V1, link);
  }
}

// Bouton V2 → État aléatoire (pour test)
BLYNK_WRITE(V2) {
  if (param.asInt() == 1) {
    int randomIndex = random(3);
    String state = states[randomIndex];


    currentState = state;
    Blynk.virtualWrite(V3, state);
    updateLCDState(state);

    // Changer LED selon état
    if (state == "IDLE") setLED(0, 255, 0);
    else if (state == "WALKING") setLED(0, 0, 255);
    else if (state == "FALLING") {
      setLED(255, 0, 0);
      triggerFallAlert();
    }
  }
}

// Nom utilisateur → V4
BLYNK_WRITE(V4) {
  userName = param.asString();

  if (userName.length() > 0 && userPhoneNumber.length() > 0) {
    updateLCDUserInfo();
  }
}

// Age utilisateur → V5
BLYNK_WRITE(V5) {
  userAge = param.asInt();
}

// Email utilisateur → V7
BLYNK_WRITE(V7) {
  userEmail = param.asString();
}

// Téléphone utilisateur → V8
BLYNK_WRITE(V8) {
  userPhoneNumber = param.asString();

  if (userName.length() > 0 && userPhoneNumber.length() > 0) {
    updateLCDUserInfo();
  }
}

// Bouton virtuel V9 pour tester l'email
BLYNK_WRITE(V9) {
  if (systemReady && param.asInt() == 1) {
    onButtonPressed();
  }
}

// ============================================================
// FONCTIONS UTILITAIRES
// ============================================================
float getLatitude() { return latitude; }
float getLongitude() { return longitude; }
float getAccuracy() { return accuracy; }
String getGoogleMapsLink() { return googleMapsLink; }
String getCurrentState() { return currentState; }
